// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/XMLNode.h>
#include <arc/client/JobDescription.h>
#include <arc/data/DataMover.h>
#include <arc/data/DataHandle.h>
#include <arc/data/URLMap.h>
#include <arc/message/MCC.h>

#include "AREXClient.h"
#include "JobControllerARC1.h"

namespace Arc {

  Logger JobControllerARC1::logger(Logger::getRootLogger(), "JobController.ARC1");

  bool JobControllerARC1::isEndpointNotSupported(const std::string& endpoint) const {
    const std::string::size_type pos = endpoint.find("://");
    return pos != std::string::npos && lower(endpoint.substr(0, pos)) != "http" && lower(endpoint.substr(0, pos)) != "https";
  }

  void JobControllerARC1::UpdateJobs(std::list<Job*>& jobs) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);

    for (std::list<Job*>::iterator iter = jobs.begin();
         iter != jobs.end(); iter++) {
      AREXClient ac((*iter)->Cluster, cfg, usercfg.Timeout());
      std::string idstr;
      AREXClient::createActivityIdentifier((*iter)->JobID, idstr);
      if (!ac.stat(idstr, **iter)) {
        logger.msg(WARNING, "Job information not found in the information system: %s", (*iter)->JobID.fullstr());
      }
    }
  }

  bool JobControllerARC1::RetrieveJob(const Job& job,
                                      std::string& downloaddir,
                                      bool usejobname,
                                      bool force) const {
    logger.msg(VERBOSE, "Downloading job: %s", job.JobID.fullstr());

    if (!downloaddir.empty()) {
      downloaddir += G_DIR_SEPARATOR_S;
    }
    if (usejobname && !job.Name.empty()) {
      downloaddir += job.Name;
    } else {
      std::string path = job.JobID.Path();
      std::string::size_type pos = path.rfind('/');
      downloaddir += path.substr(pos + 1);
    }

    std::list<std::string> files;
    if (!ListFilesRecursive(job.JobID, files)) {
      logger.msg(ERROR, "Unable to retrieve list of job files to download for job %s", job.JobID.fullstr());
      return false;
    }

    URL src(job.JobID);
    URL dst(downloaddir);

    std::string srcpath = src.Path();
    std::string dstpath = dst.Path();

    if (!force && Glib::file_test(dstpath, Glib::FILE_TEST_EXISTS)) {
      logger.msg(WARNING, "%s directory exist! Skipping job.", dstpath);
      return false;
    }

    if (srcpath.empty() || (srcpath[srcpath.size() - 1] != '/')) {
      srcpath += '/';
    }
    if (dstpath.empty() || (dstpath[dstpath.size() - 1] != G_DIR_SEPARATOR)) {
      dstpath += G_DIR_SEPARATOR_S;
    }

    bool ok = true;

    for (std::list<std::string>::iterator it = files.begin();
         it != files.end(); it++) {
      src.ChangePath(srcpath + *it);
      dst.ChangePath(dstpath + *it);
      if (!CopyJobFile(src, dst)) {
        logger.msg(INFO, "Failed dowloading %s to %s", src.str(), dst.str());
        ok = false;
      }
    }

    return ok;
  }

  bool JobControllerARC1::CleanJob(const Job& job) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(job.Cluster, cfg, usercfg.Timeout());
    std::string idstr;
    AREXClient::createActivityIdentifier(job.JobID, idstr);
    return ac.clean(idstr);
  }

  bool JobControllerARC1::CancelJob(const Job& job) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(job.Cluster, cfg, usercfg.Timeout());
    std::string idstr;
    AREXClient::createActivityIdentifier(job.JobID, idstr);
    return ac.kill(idstr);
  }

  bool JobControllerARC1::RenewJob(const Job& /* job */) const {
    logger.msg(INFO, "Renewal of ARC1 jobs is not supported");
    return false;
  }

  bool JobControllerARC1::ResumeJob(const Job& job) const {

    if (!job.RestartState) {
      logger.msg(INFO, "Job %s does not report a resumable state", job.JobID.fullstr());
      return false;
    }

    logger.msg(VERBOSE, "Resuming job: %s at state: %s (%s)", job.JobID.fullstr(), job.RestartState.GetGeneralState(), job.RestartState());

    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(job.Cluster, cfg, usercfg.Timeout());
    std::string idstr;
    AREXClient::createActivityIdentifier(job.JobID, idstr);
    bool ok = ac.resume(idstr);
    if (ok) logger.msg(VERBOSE, "Job resuming successful");
    return ok;
  }

  URL JobControllerARC1::GetFileUrlForJob(const Job& job,
                                          const std::string& whichfile) const {
    URL url(job.JobID);

    if (whichfile == "stdout") {
      url.ChangePath(url.Path() + '/' + job.StdOut);
    } else if (whichfile == "stderr") {
      url.ChangePath(url.Path() + '/' + job.StdErr);
    } else if (whichfile == "joblog") {
      url.ChangePath(url.Path() + "/" + job.LogDir + "/errors");
    }

    return url;
  }

  bool JobControllerARC1::GetJobDescription(const Job& job, std::string& desc_str) const {
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(job.Cluster, cfg, usercfg.Timeout());
    std::string idstr;
    AREXClient::createActivityIdentifier(job.JobID, idstr);
    if (ac.getdesc(idstr, desc_str)) {
      std::list<JobDescription> descs;
      if (JobDescription::Parse(desc_str, descs) && !descs.empty()) {
        return true;
      }
    }

    logger.msg(ERROR, "Failed retrieving job description for job: %s", job.JobID.fullstr());
    return false;
  }

  URL JobControllerARC1::CreateURL(std::string service, ServiceType /* st */) const {
    std::string::size_type pos1 = service.find("://");
    if (pos1 == std::string::npos)
      service = "https://" + service;
    // Default port other than 443?
    // Default path?
    return service;
  }

} // namespace Arc
