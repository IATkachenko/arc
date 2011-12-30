// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <list>
#include <string>

#include <arc/ArcLocation.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/UserConfig.h>
#include <arc/client/Job.h>
#include <arc/client/JobSupervisor.h>
#include <arc/credential/Credential.h>

#include "utils.h"

#ifdef TEST
#define RUNMIGRATE(X) test_arcmigrate_##X
#else
#define RUNMIGRATE(X) X
#endif
int RUNMIGRATE(main)(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::Logger logger(Arc::Logger::getRootLogger(), "arcmigrate");
  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  ClientOptions opt(ClientOptions::CO_MIGRATE,
                    istring("[job ...]"),
                    istring("The arcmigrate command is used for "
                            "migrating queued jobs to another resource.\n"
                            "Note that migration is only supported "
                            "between A-REX powered resources."));

  std::list<std::string> jobIDsAndNames = opt.Parse(argc, argv);

  if (opt.showversion) {
    std::cout << Arc::IString("%s version %s", "arcmigrate", VERSION)
              << std::endl;
    return 0;
  }

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!opt.debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(opt.debug));

  Arc::UserConfig usercfg(opt.conffile, opt.joblist);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (opt.show_plugins) {
    std::list<std::string> types;
    types.push_back("HED:Submitter");
    types.push_back("HED:TargetRetriever");
    types.push_back("HED:Broker");
    showplugins("arcmigrate", types, logger, usercfg.Broker().first);
    return 0;
  }

  if (!usercfg.ProxyPath().empty() ) {
    Arc::Credential holder(usercfg.ProxyPath(), "", "", "");
    if (holder.GetEndTime() < Arc::Time()){
      std::cout << Arc::IString("Proxy expired. Job submission aborted. Please run 'arcproxy'!") << std::endl;
      return 1;
    }
  }
  else {
    std::cout << Arc::IString("Cannot find any proxy. arcmigrate currently cannot run without a proxy.\n"
                              "  If you have the proxy file in a non-default location,\n"
                              "  please make sure the path is specified in the client configuration file.\n"
                              "  If you don't have a proxy yet, please run 'arcproxy'!") << std::endl;
    return 1;
  }

  if (opt.debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  for (std::list<std::string>::const_iterator it = opt.jobidinfiles.begin(); it != opt.jobidinfiles.end(); it++) {
    if (!Arc::Job::ReadJobIDsFromFile(*it, jobIDsAndNames)) {
      logger.msg(Arc::WARNING, "Cannot read specified jobid file: %s", *it);
    }
  }

  if (opt.timeout > 0)
    usercfg.Timeout(opt.timeout);

  if (!opt.broker.empty())
    usercfg.Broker(opt.broker);

  if (!opt.joblist.empty() && jobIDsAndNames.empty() && opt.clusters.empty())
    opt.all = true;

  if (jobIDsAndNames.empty() && opt.clusters.empty() && !opt.all) {
    logger.msg(Arc::ERROR, "No jobs given");
    return 1;
  }

  // Removes slashes from end of cluster names, and put cluster to reject into separate list.
  std::list<std::string> rejectClusters;
  for (std::list<std::string>::iterator itC = opt.clusters.begin();
       itC != opt.clusters.end();) {
    if ((*itC)[itC->length()-1] == '/') {
      itC->erase(itC->length()-1);
    }
    if ((*itC)[0] == '-') {
      rejectClusters.push_back(itC->substr(1));
      itC = opt.clusters.erase(itC);
    }
    else {
      ++itC;
    }
  }

  std::list<Arc::Job> jobs;
  Arc::Job::ReadAllJobsFromFile(usercfg.JobListFile(), jobs);
  if (!opt.all) {
    for (std::list<Arc::Job>::iterator itJ = jobs.begin();
         itJ != jobs.end();) {
      if (jobIDsAndNames.empty() && opt.clusters.empty()) {
        // Remove remaing jobs.
        jobs.erase(itJ, jobs.end());
        break;
      }
      std::list<std::string>::iterator itJID = jobIDsAndNames.begin();
      for (;itJID != jobIDsAndNames.end(); ++itJID) {
        if (itJ->IDFromEndpoint.str() == *itJID) {
          break;
        }
      }
      if (itJID != jobIDsAndNames.end()) {
        // Job explicitly specified. Remove id from list so we dont iterate it again.
        jobIDsAndNames.erase(itJID);
        ++itJ;
        continue;
      }

      std::list<std::string>::const_iterator itC = opt.clusters.begin();
      for (; itC != opt.clusters.end(); ++itC) {
        if (itJ->Cluster.str() == *itC ||
            itJ->Cluster.Host() == *itC ||
            itJ->Cluster.Host() + "/" + itJ->Cluster.Path() == *itC ||
            itJ->Cluster.Host() + ":" + Arc::tostring(itJ->Cluster.Port()) == *itC ||
            itJ->Cluster.Host() + ":" + Arc::tostring(itJ->Cluster.Port()) + "/" + itJ->Cluster.Path() == *itC) {
          break;
        }
      }
      if (itC != opt.clusters.end()) {
        // Cluster on which job reside is explicitly specified.
        ++itJ;
        continue;
      }

      // Job is not selected - remove it.
      itJ = jobs.erase(itJ);
    }
  }

  // Filter jobs on rejected clusters.
  for (std::list<std::string>::const_iterator itC = rejectClusters.begin();
       itC != rejectClusters.end(); ++itC) {
    std::list<Arc::Job>::iterator itJ = jobs.begin();
    for (; itJ != jobs.end(); ++itJ) {
      if (itJ->Cluster.str() == *itC ||
          itJ->Cluster.Host() == *itC ||
          itJ->Cluster.Host() + "/" + itJ->Cluster.Path() == *itC ||
          itJ->Cluster.Host() + ":" + Arc::tostring(itJ->Cluster.Port()) == *itC ||
          itJ->Cluster.Host() + ":" + Arc::tostring(itJ->Cluster.Port()) + "/" + itJ->Cluster.Path() == *itC) {
        break;
      }
    }
    if (itJ != jobs.end()) {
      jobs.erase(itJ);
    }
  }

  if (!opt.qlusters.empty() || !opt.indexurls.empty()) {
    usercfg.ClearSelectedServices();
    if (!opt.qlusters.empty()) {
      usercfg.AddServices(opt.qlusters, Arc::COMPUTING);
    }
    if (!opt.indexurls.empty()) {
      usercfg.AddServices(opt.indexurls, Arc::INDEX);
    }
  }

  Arc::JobSupervisor jobmaster(usercfg, jobs);
  if (!jobmaster.JobsFound()) {
    std::cout << Arc::IString("No jobs selected for migration") << std::endl;
    return 0;
  }

  std::list<Arc::Job> migratedJobs;
  std::list<Arc::URL> notmigrated;
  if (jobmaster.Migrate(opt.forcemigration, migratedJobs, notmigrated) && migratedJobs.empty()) {
    std::cout << Arc::IString("No queuing jobs to migrate") << std::endl;
    return 0;
  }

  for (std::list<Arc::Job>::const_iterator it = migratedJobs.begin();
       it != migratedJobs.end(); ++it) {
    std::cout << Arc::IString("Job submitted with jobid: %s", it->IDFromEndpoint.str()) << std::endl;
  }

  if (!migratedJobs.empty() && !Arc::Job::WriteJobsToFile(usercfg.JobListFile(), migratedJobs)) {
    std::cout << Arc::IString("Warning: Failed to lock job list file %s", usercfg.JobListFile()) << std::endl;
    std::cout << Arc::IString("         To recover missing jobs, run arcsync") << std::endl;
  }

  if (!opt.jobidoutfile.empty() && !Arc::Job::WriteJobIDsToFile(migratedJobs, opt.jobidoutfile)) {
    logger.msg(Arc::WARNING, "Cannot write job IDs of submitted jobs to file (%s)", opt.jobidoutfile);
  }

  // Get job IDs of jobs to kill.
  std::list<Arc::URL> jobstobekilled;
  for (std::list<Arc::Job>::const_iterator it = migratedJobs.begin();
       it != migratedJobs.end(); ++it) {
    if (!it->ActivityOldID.empty()) {
      jobstobekilled.push_back(it->ActivityOldID.back());
    }
  }

  std::list<Arc::URL> notkilled;
  std::list<Arc::URL> killedJobs = jobmaster.Cancel(jobstobekilled, notkilled);
  for (std::list<Arc::URL>::const_iterator it = notkilled.begin();
       it != notkilled.end(); ++it) {
    logger.msg(Arc::WARNING, "Migration of job (%s) succeeded, but killing the job failed - it will still appear in the job list", it->str());
  }

  if (!opt.keep) {
    std::list<Arc::URL> notcleaned;
    std::list<Arc::URL> cleanedJobs = jobmaster.Clean(killedJobs, notcleaned);
    for (std::list<Arc::URL>::const_iterator it = notcleaned.begin();
         it != notcleaned.end(); ++it) {
      logger.msg(Arc::WARNING, "Migration of job (%s) succeeded, but cleaning the job failed - it will still appear in the job list", it->str());
    }

    if (!Arc::Job::RemoveJobsFromFile(usercfg.JobListFile(), cleanedJobs)) {
      std::cout << Arc::IString("Warning: Failed to lock job list file %s", usercfg.JobListFile()) << std::endl;
      std::cout << Arc::IString("         Use arcclean to remove non-existing jobs") << std::endl;
    }
  }

  if ((migratedJobs.size() + notmigrated.size()) > 1) {
    std::cout << std::endl << Arc::IString("Job migration summary:") << std::endl;
    std::cout << "-----------------------" << std::endl;
    std::cout << Arc::IString("%d of %d jobs were migrated", migratedJobs.size(), migratedJobs.size() + notmigrated.size()) << std::endl;
    if (!notmigrated.empty()) {
      std::cout << Arc::IString("The following %d were not migrated", notmigrated.size()) << std::endl;
      for (std::list<Arc::URL>::const_iterator it = notmigrated.begin();
           it != notmigrated.end(); ++it) {
        std::cout << it->str() << std::endl;
      }
    }
  }

  return notmigrated.empty();
}
