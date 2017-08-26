#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "jura.h"
#include "UsageReporter.h"
#include "JobLogFile.h"

//TODO cross-platform
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sstream>

#include <arc/ArcRegex.h>
#include <arc/Utils.h>

namespace ArcJura
{

  /** Constructor. Pass name of directory containing job log files,
   *  expiration time of files in seconds, list of URLs in case of 
   *  interactive mode.
   */
  UsageReporter::UsageReporter(std::string job_log_dir_, time_t expiration_time_,
                               std::vector<std::string> urls_, std::vector<std::string> topics_,
                               std::string vo_filters_,
                               std::string out_dir_):
    logger(Arc::Logger::rootLogger, "JURA.UsageReporter"),
    dests(NULL),
    job_log_dir(job_log_dir_),
    expiration_time(expiration_time_),
    urls(urls_),
    topics(topics_),
    vo_filters(vo_filters_),
    out_dir(out_dir_)
  {
    logger.msg(Arc::INFO, "Initialised, job log dir: %s",
               job_log_dir.c_str());
    logger.msg(Arc::VERBOSE, "Expiration time: %d seconds",
               expiration_time);
    if (!urls.empty())
      {
        logger.msg(Arc::VERBOSE, "Interactive mode.",
                   expiration_time);
      }
    //Collection of logging destinations:
    dests=new Destinations();
  }

  /**
   *  Parse usage data and publish it via the appropriate destination adapter.
   */
  int UsageReporter::report()
  {
    //ngjobid->url mapping to keep track of which loggerurl is replaced
    //by the '-u' options
    std::map<std::string,std::string> dest_to_duplicate;
    //Collect job log file names from job log dir
    //(to know where to get usage data from)
    DIR *dirp = NULL;
    dirent *entp = NULL;
    errno=0;
    if ( (dirp=opendir(job_log_dir.c_str()))==NULL )
      {
        logger.msg(Arc::ERROR, 
                   "Could not open log directory \"%s\": %s",
                   job_log_dir.c_str(),
                   Arc::StrError(errno)
                   );
        return -1;
      }

    DIR *odirp = NULL;
    errno=0;
    if ( !out_dir.empty() && (odirp=opendir(out_dir.c_str()))==NULL )
      {
        logger.msg(Arc::ERROR, 
                   "Could not open output directory \"%s\": %s",
                   out_dir.c_str(),
                   Arc::StrError(errno)
                   );
        closedir(dirp);
        return -1;
      }
    if (odirp != NULL) {
      closedir(odirp);
    }

    // Seek "<jobnumber>.<randomstring>" files.
    Arc::RegularExpression logfilepattern("^[0-9A-Za-z]+\\.[^.]+$");
    errno = 0;
    while ((entp = readdir(dirp)) != NULL)
      {
        if (logfilepattern.match(entp->d_name))
          {
            //Parse log file
            JobLogFile *logfile;
            //TODO handle DOS-style path separator!
            std::string fname=job_log_dir+"/"+entp->d_name;
            logfile=new JobLogFile(fname);
            if (!out_dir.empty())
              {
                if ((*logfile)["jobreport_option_archiving"] == "")
                  {
                    (*logfile)["jobreport_option_archiving"] = out_dir;
                  } else {
                    (*logfile)["outputdir"] = out_dir;
                  }
              }

            if ( vo_filters != "")
              {
                  (*logfile)["vo_filters"] = vo_filters;
              }
            //A. Non-interactive mode: each jlf is parsed, and if valid, 
            //   submitted to the destination given  by "loggerurl=..."
            if (urls.empty())
              {
                // Check creation time and remove it if really too old
                if( expiration_time>0 && logfile->olderThan(expiration_time) )
                  {
                    logger.msg(Arc::INFO,
                               "Removing outdated job log file %s",
                               logfile->getFilename().c_str()
                               );
                    logfile->remove();
                  } 
                else
                  {
                    // Set topic option if it is needed
                    if ( (*logfile)["loggerurl"].substr(0,4) == "APEL" ) {
                       (*logfile)["topic"] = (*logfile)["jobreport_option_topic"];
                    }
                    //Pass job log file content to the appropriate 
                    //logging destination
                    dests->report(*logfile);
                    //(deep copy performed)
                  }
              }

            //B. Interactive mode: submit only to services specified by
            //   command line option '-u'. Avoid repetition if several jlfs
            //   are created with same content and different destination.
            //   Keep all jlfs on disk.
            else
              {
                if ( dest_to_duplicate.find( (*logfile)["ngjobid"] ) ==
                     dest_to_duplicate.end() )
                  {
                    dest_to_duplicate[ (*logfile)["ngjobid"] ]=
                      (*logfile)["loggerurl"];
                  }

                //submit only 1x to each!
                if ( dest_to_duplicate[ (*logfile)["ngjobid"] ] ==
                     (*logfile)["loggerurl"] )
                  {
                    //Duplicate content of log file, overwriting URL with
                    //each '-u' command line option, disabling file deletion
                    JobLogFile *dupl_logfile=
                      new JobLogFile(*logfile);
                    dupl_logfile->allowRemove(false);

                    for (int it=0; it<(int)urls.size(); it++)
                      {
                        (*dupl_logfile)["loggerurl"] = urls[it];
                        if (!topics[it].empty())
                          {
                            (*dupl_logfile)["topic"] = topics[it];
                          }

                        //Pass duplicated job log content to the appropriate 
                        //logging destination
                        dests->report(*dupl_logfile);
                        //(deep copy performed)

                      }
                    delete dupl_logfile;
                  }
              }

            delete logfile;
          }
        errno = 0;
      }
    closedir(dirp);

    if (errno!=0)
      {
        logger.msg(Arc::ERROR, 
                   "Error reading log directory \"%s\": %s",
                   job_log_dir.c_str(),
                   Arc::StrError(errno)
                   );
        return -2;
      }
    return 0;
  }
  
  UsageReporter::~UsageReporter()
  {
    delete dests;
    logger.msg(Arc::INFO, "Finished, job log dir: %s",
               job_log_dir.c_str());
  }
  
}
