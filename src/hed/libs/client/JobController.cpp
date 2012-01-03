// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <fstream>
#include <iostream>

#include <unistd.h>
#include <glibmm/fileutils.h>
#include <glibmm.h>

#include <arc/ArcConfig.h>
#include <arc/FileLock.h>
#include <arc/IString.h>
#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/client/Broker.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/Submitter.h>
#include <arc/client/TargetGenerator.h>
#include <arc/UserConfig.h>
#include <arc/data/DataMover.h>
#include <arc/data/DataHandle.h>
#include <arc/data/FileCache.h>
#include <arc/data/URLMap.h>
#include <arc/loader/FinderLoader.h>
#include "JobController.h"

namespace Arc {

  Logger JobController::logger(Logger::getRootLogger(), "JobController");

  JobController::JobController(const UserConfig& usercfg,
                               const std::string& flavour)
    : flavour(flavour),
      usercfg(usercfg),
      data_source(NULL),
      data_destination(NULL) {}

  JobController::~JobController() {
    if(data_source) delete data_source;
    if(data_destination) delete data_destination;
  }

  bool JobController::FillJobStore(const Job& job) {

    if (job.Flavour != flavour) {
      logger.msg(WARNING, "The middleware flavour of the job (%s) does not match that of the job controller (%s)", job.Flavour, flavour);
      return false;
    }

    if (!job.JobID) {
      logger.msg(WARNING, "The job ID (%s) is not a valid URL", job.JobID.fullstr());
      return false;
    }

    if (!job.Cluster) {
      logger.msg(WARNING, "The resource URL is not a valid URL", job.Cluster.str());
      return false;
    }

    jobstore.push_back(job);
    return true;
  }

  bool JobController::Get(const std::list<std::string>& status,
                          const std::string& downloaddir,
                          bool keep,
                          bool usejobname,
                          bool force) {
    GetJobInformation();

    std::list< std::list<Job>::iterator > downloadable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {

      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      if (it->State == JobState::DELETED) {
        logger.msg(WARNING, "Job has already been deleted: %s",
                   it->JobID.fullstr());
        continue;
      }
      else if (!it->State.IsFinished()) {
        logger.msg(WARNING, "Job has not finished yet: %s", it->JobID.fullstr());
        continue;
      }

      downloadable.push_back(it);
    }

    bool ok = true;
    std::list<URL> toberemoved;
    for (std::list< std::list<Job>::iterator >::iterator it = downloadable.begin();
         it != downloadable.end(); it++) {

      bool downloaded = GetJob(**it, downloaddir, usejobname, force);
      if (!downloaded) {
        logger.msg(ERROR, "Failed downloading job %s", (*it)->JobID.fullstr());
        ok = false;
        continue;
      }

      if (!keep) {
        bool cleaned = CleanJob(**it);
        if (!cleaned) {
          logger.msg(ERROR, "Failed cleaning job %s", (*it)->JobID.fullstr());
          ok = false;
          continue;
        }

        toberemoved.push_back((*it)->IDFromEndpoint);
        jobstore.erase(*it);
      }
    }

    Job::RemoveJobsFromFile(usercfg.JobListFile(), toberemoved);
    return ok;
  }

  bool JobController::Kill(const std::list<std::string>& status,
                           bool keep) {
    GetJobInformation();

    std::list< std::list<Job>::iterator > killable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {

      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      if (it->State == JobState::DELETED) {
        logger.msg(WARNING, "Job has already been deleted: %s", it->JobID.fullstr());
        continue;
      }
      else if (it->State.IsFinished()) {
        logger.msg(WARNING, "Job has already finished: %s", it->JobID.fullstr());
        continue;
      }

      killable.push_back(it);
    }

    bool ok = true;
    std::list<URL> toberemoved;
    for (std::list< std::list<Job>::iterator >::iterator it = killable.begin();
         it != killable.end(); it++) {

      bool cancelled = CancelJob(**it);
      if (!cancelled) {
        logger.msg(ERROR, "Failed cancelling job %s", (*it)->JobID.fullstr());
        ok = false;
        continue;
      }

      if (!keep) {
        bool cleaned = CleanJob(**it);
        if (!cleaned) {
          logger.msg(ERROR, "Failed cleaning job %s", (*it)->JobID.fullstr());
          ok = false;
          continue;
        }

        toberemoved.push_back((*it)->IDFromEndpoint.fullstr());
        jobstore.erase(*it);
      }
    }

    Job::RemoveJobsFromFile(usercfg.JobListFile(), toberemoved);
    return ok;
  }

  bool JobController::Cat(std::ostream& out,
                          const std::list<std::string>& status,
                          const std::string& whichfile_) {
    std::string whichfile(whichfile_);
    if (whichfile != "stdout" && whichfile != "stderr" && whichfile != "joblog") {
      logger.msg(ERROR, "Unknown output %s", whichfile);
      return false;
    }

    GetJobInformation();

    std::list<Job*> catable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      if (it->State == JobState::DELETED) {
        logger.msg(WARNING, "Job deleted: %s", it->JobID.fullstr());
        continue;
      }

      // The job-log might be available before the job has started (middleware dependent).
      if (whichfile != "joblog" &&
          !it->State.IsFinished() &&
          it->State != JobState::RUNNING &&
          it->State != JobState::FINISHING) {
        logger.msg(WARNING, "Job has not started yet: %s", it->JobID.fullstr());
        continue;
      }

      if (((whichfile == "stdout") && (it->StdOut.empty())) ||
          ((whichfile == "stderr") && (it->StdErr.empty())) ||
          ((whichfile == "joblog") && (it->LogDir.empty()))) {
        logger.msg(ERROR, "Can not determine the %s location: %s",
                   whichfile, it->JobID.fullstr());
        continue;
      }

      catable.push_back(&(*it));
    }

    bool ok = true;
    for (std::list<Job*>::iterator it = catable.begin();
         it != catable.end(); it++) {
      // saving to a temp file is necessary because chunks from server
      // may arrive out of order
      std::string filename = Glib::build_filename(Glib::get_tmp_dir(), "arccat.XXXXXX");
      int tmp_h = Glib::mkstemp(filename);
      if (tmp_h == -1) {
        logger.msg(INFO, "Could not create temporary file \"%s\"", filename);
        logger.msg(ERROR, "Cannot create output of %s for job (%s)", whichfile, (*it)->JobID.fullstr());
        ok = false;
        continue;
      }

      logger.msg(VERBOSE, "Catting %s for job %s", whichfile, (*it)->JobID.fullstr());

      URL src = GetFileUrlForJob((**it), whichfile);
      if (!src) {
        logger.msg(ERROR, "Cannot create output of %s for job (%s): Invalid source %s", whichfile, (*it)->JobID.fullstr(), src.str());
        close(tmp_h);
        unlink(filename.c_str());
        continue;
      }

      URL dst("stdio:///"+tostring(tmp_h));
      if (!dst) {
        logger.msg(ERROR, "Cannot create output of %s for job (%s): Invalid destination %s", whichfile, (*it)->JobID.fullstr(), dst.str());
        close(tmp_h);
        unlink(filename.c_str());
        continue;
      }

      bool copied = ARCCopyFile(src, dst);

      if (copied) {
        out << IString("%s from job %s", whichfile,
                             (*it)->JobID.fullstr()) << std::endl;
        std::ifstream is(filename.c_str());
        char c;
        while (is.get(c)) {
          out.put(c);
        }
        is.close();
      }
      else {
        ok = false;
      }
      close(tmp_h);
      unlink(filename.c_str());
    }

    return ok;
  }

  bool JobController::SaveJobStatusToStream(std::ostream& out,
                                            const std::list<std::string>& status,
                                            bool longlist) {
    GetJobInformation();

    for (std::list<Job>::const_iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      it->SaveToStream(out, longlist);
    }
    return true;
  }

  void JobController::FetchJobs(const std::list<std::string>& status,
                                std::vector<const Job*>& jobs) {
    GetJobInformation();

    for (std::list<Job>::const_iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      jobs.push_back(&*it);
    }
  }

  bool JobController::Renew(const std::list<std::string>& status) {

    GetJobInformation();

    std::list<Job*> renewable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      if (it->State == JobState::FINISHED || it->State == JobState::KILLED || it->State == JobState::DELETED) {
        logger.msg(WARNING, "Job has already finished: %s", it->JobID.fullstr());
        continue;
      }

      renewable.push_back(&(*it));
    }

    if (renewable.empty()) return false;

    bool ok = true;
    for (std::list<Job*>::iterator it = renewable.begin();
         it != renewable.end(); it++) {
      bool renewed = RenewJob(**it);
      if (!renewed) {
        logger.msg(ERROR, "Failed renewing job %s", (*it)->JobID.fullstr());
        ok = false;
        continue;
      }
    }
    return ok;
  }

  bool JobController::Resume(const std::list<std::string>& status) {

    GetJobInformation();

    std::list<Job*> resumable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!it->State) {
        continue;
      }

      if (!status.empty() &&
          std::find(status.begin(), status.end(), it->State()) == status.end() &&
          std::find(status.begin(), status.end(), it->State.GetGeneralState()) == status.end())
        continue;

      if (!it->State.IsFinished()) {
        logger.msg(WARNING, "Job has not finished yet: %s", it->JobID.fullstr());
        continue;
      }

      resumable.push_back(&(*it));
    }

    bool ok = true;
    for (std::list<Job*>::iterator it = resumable.begin();
         it != resumable.end(); it++) {
      bool resumed = ResumeJob(**it);
      if (!resumed) {
        logger.msg(ERROR, "Failed resuming job %s", (*it)->JobID.fullstr());
        ok = false;
        continue;
      }
    }
    return ok;
  }

  std::list<std::string> JobController::GetDownloadFiles(const URL& dir) {

    std::list<std::string> files;
    std::list<FileInfo> outputfiles;

    DataHandle handle(dir, usercfg);
    if (!handle) {
      logger.msg(INFO, "Unable to list files at %s", dir.str());
      return files;
    }
    if(!handle->List(outputfiles, (Arc::DataPoint::DataPointInfoType)
                                  (DataPoint::INFO_TYPE_NAME | DataPoint::INFO_TYPE_TYPE))) {
      logger.msg(INFO, "Unable to list files at %s", dir.str());
      return files;
    }

    for (std::list<FileInfo>::iterator i = outputfiles.begin();
         i != outputfiles.end(); i++) {

      if (i->GetName() == ".." || i->GetName() == ".")
        continue;

      if (i->GetType() == FileInfo::file_type_unknown ||
          i->GetType() == FileInfo::file_type_file)
        files.push_back(i->GetName());
      else if (i->GetType() == FileInfo::file_type_dir) {

        std::string path = dir.Path();
        if (path[path.size() - 1] != '/')
          path += "/";
        URL tmpdir(dir);
        tmpdir.ChangePath(path + i->GetName());
        std::list<std::string> morefiles = GetDownloadFiles(tmpdir);
        std::string dirname = i->GetName();
        if (dirname[dirname.size() - 1] != '/')
          dirname += "/";
        for (std::list<std::string>::iterator it = morefiles.begin();
             it != morefiles.end(); it++)
          files.push_back(dirname + *it);
      }
    }
    return files;
  }

  bool JobController::ARCCopyFile(const URL& src, const URL& dst) {

    DataMover mover;
    mover.retry(true);
    mover.secure(false);
    mover.passive(true);
    mover.verbose(false);

    logger.msg(VERBOSE, "Now copying (from -> to)");
    logger.msg(VERBOSE, " %s -> %s", src.str(), dst.str());

    URL src_(src);
    URL dst_(dst);
    src_.AddOption("checksum=no");
    dst_.AddOption("checksum=no");

    if ((!data_source) || (!*data_source) ||
        (!(*data_source)->SetURL(src_))) {
      if(data_source) delete data_source;
      data_source = new DataHandle(src_, usercfg);
    }
    DataHandle& source = *data_source;
    if (!source) {
      logger.msg(ERROR, "Unable to initialise connection to source: %s", src.str());
      return false;
    }

    if ((!data_destination) || (!*data_destination) ||
        (!(*data_destination)->SetURL(dst_))) {
      if(data_destination) delete data_destination;
      data_destination = new DataHandle(dst_, usercfg);
    }
    DataHandle& destination = *data_destination;
    if (!destination) {
      logger.msg(ERROR, "Unable to initialise connection to destination: %s",
                 dst.str());
      return false;
    }

    // Set desired number of retries. Also resets any lost
    // tries from previous files.
    source->SetTries((src.Protocol() == "file")?1:3);
    destination->SetTries((dst.Protocol() == "file")?1:3);

    // Turn off all features we do not need
    source->SetAdditionalChecks(false);
    destination->SetAdditionalChecks(false);

    FileCache cache;
    DataStatus res =
      mover.Transfer(*source, *destination, cache, URLMap(), 0, 0, 0,
                     usercfg.Timeout());
    if (!res.Passed()) {
      if (!res.GetDesc().empty())
        logger.msg(ERROR, "File download failed: %s - %s", std::string(res), res.GetDesc());
      else
        logger.msg(ERROR, "File download failed: %s", std::string(res));
      // Reset connection because one can't be sure how failure
      // affects server and/or connection state.
      // TODO: Investigate/define DMC behavior in such case.
      delete data_source;
      data_source = NULL;
      delete data_destination;
      data_destination = NULL;
      return false;
    }

    return true;

  }

  std::list<Job> JobController::GetJobDescriptions(const std::list<std::string>& status,
                                                   bool getlocal) {
    GetJobInformation();

    // Only selected jobs with specified status
    std::list<Job> gettable;
    for (std::list<Job>::iterator it = jobstore.begin();
         it != jobstore.end(); it++) {
      if (!status.empty() && !it->State) {
        continue;
      }

      if (!status.empty() && std::find(status.begin(), status.end(),
                                       it->State()) == status.end()) {
        continue;
      }

      gettable.push_back(*it);
    }

    // Get job description for those jobs without one.
    for (std::list<Job>::iterator it = gettable.begin();
         it != gettable.end();) {
      if (!it->JobDescriptionDocument.empty()) {
        it++;
        continue;
      }
      if (GetJobDescription(*it, it->JobDescriptionDocument)) {
        logger.msg(VERBOSE, "Job description retrieved from execution service for job (%s)", it->IDFromEndpoint.fullstr());
        it++;
      }
      else {
        logger.msg(INFO, "Failed retrieving job description for job (%s)", it->IDFromEndpoint.fullstr());
        it = gettable.erase(it);
      }
    }

    return gettable;
  }

  JobControllerLoader::JobControllerLoader()
    : Loader(BaseConfig().MakeConfig(Config()).Parent()) {}

  JobControllerLoader::~JobControllerLoader() {
    for (std::list<JobController*>::iterator it = jobcontrollers.begin();
         it != jobcontrollers.end(); it++)
      delete *it;
  }

  JobController* JobControllerLoader::load(const std::string& name,
                                           const UserConfig& usercfg) {
    if (name.empty())
      return NULL;

    if(!factory_->load(FinderLoader::GetLibrariesList(),
                       "HED:JobController", name)) {
      logger.msg(ERROR, "Unable to locate the \"%s\" plugin. Please refer to installation instructions and check if package providing support for \"%s\" plugin is installed", name, name);
      logger.msg(DEBUG, "JobController plugin \"%s\" not found.", name);
      return NULL;
    }

    JobControllerPluginArgument arg(usercfg);
    JobController *jobcontroller =
      factory_->GetInstance<JobController>("HED:JobController", name, &arg, false);

    if (!jobcontroller) {
      logger.msg(ERROR, "Unable to locate the \"%s\" plugin. Please refer to installation instructions and check if package providing support for \"%s\" plugin is installed", name, name);
      logger.msg(DEBUG, "JobController %s could not be created", name);
      return NULL;
    }

    jobcontrollers.push_back(jobcontroller);
    logger.msg(DEBUG, "Loaded JobController %s", name);
    return jobcontroller;
  }

} // namespace Arc
