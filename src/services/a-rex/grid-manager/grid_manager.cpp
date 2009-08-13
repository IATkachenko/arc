#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sys/types.h>
#include <pwd.h>
#include <string>
#include <cstdio>
#include <fstream>
#include <list>
#include <signal.h>

#include <arc/Logger.h>
#include <arc/Run.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>
#include "jobs/users.h"
#include "jobs/states.h"
#include "jobs/commfifo.h"
#include "conf/environment.h"
#include "conf/conf_file.h"
#include "conf/daemon.h"
#include "files/info_types.h"
#include "files/delete.h"
#include "run/run_parallel.h"

#include "grid_manager.h"

/* do job cleaning every 2 hours */
#define HARD_JOB_PERIOD 7200

/* cache cleaning and registration every 5 minutes */
#define CACHE_CLEAN_PERIOD 300

#define DEFAULT_LOG_FILE "/var/log/grid-manager.log"
#define DEFAULT_PID_FILE "/var/run/grid-manager.pid"

namespace ARex {

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AREX:GM");

static void* cache_func(void* arg) {
  const JobUsers* users = (const JobUsers*)arg;
  Arc::Run *proc = NULL;
  JobUser gmuser(getuid()); // Cleaning should run under the GM user 
  
  // run cache cleaning periodically 
  for(;;) {
    // go through each user and clean their caches. One user is processed per clean period
    for (JobUsers::const_iterator cacheuser = users->begin(); cacheuser != users->end(); ++cacheuser) {
      int exit_code = -1;
      // if previous process has not finished
      if(proc != NULL) {  
        if(!(proc->Running())) {
          exit_code=proc->Result();
          delete proc;
          proc=NULL;
        };
      };
      if(proc == NULL) { // previous already exited
        CacheConfig * cache_info = cacheuser->CacheParams();
        if (!cache_info->cleanCache()) continue;
        
        // get the cache dirs
        std::list<std::string> cache_info_dirs = cache_info->getCacheDirs();

        // in arc.conf % of used space is given, but cleanbyage uses % of free space
        std::string minfreespace = Arc::tostring(100-cache_info->getCacheMax());
        std::string maxfreespace = Arc::tostring(100-cache_info->getCacheMin());  

        // set log file location - controldir/job.cache-clean.errors
        // TODO: use GM log?
        gmuser.SetControlDir(cacheuser->ControlDir()); // Should this requirement be removed ?
        int argc=0;
        char* args[6+cache_info_dirs.size()+1];
        
        // do cache-clean -h for explanation of options
        std::string cmd = nordugrid_libexec_loc + "/cache-clean";
        args[argc++]=(char*)cmd.c_str();
        args[argc++]=(char*)"-m";
        args[argc++]=(char*)minfreespace.c_str();
        args[argc++]=(char*)"-M";
        args[argc++]=(char*)maxfreespace.c_str();
        args[argc++]=(char*)"-D";
        std::list<std::string> cache_dirs;
        // have to loop over twice to avoid repeating the same pointer in args
        for (std::list<std::string>::iterator i = cache_info_dirs.begin(); i != cache_info_dirs.end(); i++) {
          cache_dirs.push_back(i->substr(0, i->find(" ")));
        }
        for (std::list<std::string>::iterator i = cache_dirs.begin(); i != cache_dirs.end(); i++) {
          args[argc++]=(char*)(*i).c_str();
        }
        args[argc]=NULL;
        if(!RunParallel::run(gmuser,"cache-clean",args,&proc,false,false)) {
          logger.msg(Arc::ERROR,"Failed to run cache cleanup script: %s", cmd);
        };
      };
      for(unsigned int t=CACHE_CLEAN_PERIOD;t;) t=sleep(t);
    };
  };
  return NULL;
}

typedef struct {
  pthread_cond_t* sleep_cond;
  pthread_mutex_t* sleep_mutex;
  CommFIFO* timeout;
} sleep_st;

static void* wakeup_func(void* arg) {
  sleep_st* s = (sleep_st*)arg;
  for(;;) {
    s->timeout->wait();
    pthread_mutex_lock(s->sleep_mutex);
    pthread_cond_signal(s->sleep_cond);
    pthread_mutex_unlock(s->sleep_mutex);
  };
  return NULL;
}

static void kick_func(void* arg) {
  sleep_st* s = (sleep_st*)arg;
  pthread_mutex_lock(s->sleep_mutex);
  pthread_cond_signal(s->sleep_cond);
  pthread_mutex_unlock(s->sleep_mutex);
}

typedef struct {
  int argc;
  char** argv;
} args_st;

static void grid_manager(void* arg) {
  const char* config_filename = (const char*)arg;
  if(!arg) return;
  unsigned int clean_first_level=0;
  // int n;
  // int argc = ((args_st*)arg)->argc;
  // char** argv = ((args_st*)arg)->argv;
  setpgid(0,0);
  opterr=0;
  nordugrid_config_loc="";

  logger.msg(Arc::INFO,"Starting grid-manager thread");
  Daemon daemon;
  // Only supported option now is -c
  /*
  while((n=daemon.getopt(argc,argv,"hvC:c:")) != -1) {
    switch(n) {
      case ':': { logger.msg(Arc::ERROR,"Missing argument"); return; };
      case '?': { logger.msg(Arc::ERROR,"Unrecognized option: %s",(char)optopt); return; };
      case '.': { return; };
      case 'h': {
        std::cout<<"grid-manager [-C clean_level] [-v] [-h] [-c configuration_file] "<<daemon.short_help()<<std::endl;
         return;
      };
      case 'v': {
        std::cout<<"grid-manager: version "<<VERSION<<std::endl;
        return;
      };
      case 'C': {
        if(sscanf(optarg,"%u",&clean_first_level) != 1) {
          logger.msg(Arc::ERROR,"Wrong clean level");
          return;
        };
      }; break;
      case 'c': {
        nordugrid_config_loc=optarg;
      }; break;
      default: { logger.msg(Arc::ERROR,"Option processing error"); return; };
    };
  };
  */
  if(config_filename) nordugrid_config_loc=config_filename;

  JobUsers users;
  std::string my_username("");
  uid_t my_uid=getuid();
  JobUser *my_user = NULL;
  if(!read_env_vars()) {
    logger.msg(Arc::FATAL,"Can't initialize runtime environment - EXITING"); return;
  };
  
  /* recognize itself */
  {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwuid_r(my_uid,&pw_,buf,BUFSIZ,&pw);
    if(pw != NULL) { my_username=pw->pw_name; };
  };
  if(my_username.length() == 0) {
    logger.msg(Arc::FATAL,"Can't recognize own username - EXITING"); return;
  };
  my_user = new JobUser(my_username);
  if(!configure_serviced_users(users,my_uid,my_username,*my_user,&daemon)) {
    logger.msg(Arc::INFO,"Used configuration file %s",nordugrid_config_loc);
    logger.msg(Arc::FATAL,"Error processing configuration - EXITING"); return;
  };
  if(users.size() == 0) {
    logger.msg(Arc::FATAL,"No suitable users found in configuration - EXITING"); return;
  };

  //daemon.logfile(DEFAULT_LOG_FILE);
  //daemon.pidfile(DEFAULT_PID_FILE);
  //if(daemon.daemon() != 0) {
  //  perror("Error - daemonization failed");
  //  exit(1);
  //}; 
  logger.msg(Arc::INFO,"Used configuration file %s",nordugrid_config_loc);
  print_serviced_users(users);

  //unsigned int wakeup_period = JobsList::WakeupPeriod();
  CommFIFO wakeup_interface;
  pthread_t wakeup_thread;
  pthread_t cache_thread;
  time_t hard_job_time; 
  pthread_cond_t sleep_cond = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
  sleep_st wakeup_h;
  wakeup_h.sleep_cond=&sleep_cond;
  wakeup_h.sleep_mutex=&sleep_mutex;
  wakeup_h.timeout=&wakeup_interface;
  for(JobUsers::iterator i = users.begin();i!=users.end();++i) {
    wakeup_interface.add(*i);
  };
  wakeup_interface.timeout(JobsList::WakeupPeriod());

  // Prepare signal handler(s). Must be done after fork/daemon and preferably
  // before any new thread is started. 
  //# RunParallel run(&sleep_cond);
  //# if(!run.is_initialized()) {
  //#   logger.msg(Arc::ERROR,"Error - initialization of signal environment failed");
  //#   goto exit;
  //# };

  // I hope nothing till now used Globus

  // It looks like Globus screws signal setup somehow
  //# run.reinit(false);

  /* start timer thread - wake up every 2 minutes */
  if(pthread_create(&wakeup_thread,NULL,&wakeup_func,&wakeup_h) != 0) {
    logger.msg(Arc::ERROR,"Failed to start new thread"); return;
  };
  RunParallel::kicker(&kick_func,&wakeup_h);
  if(clean_first_level) {
    bool clean_finished = false;
    bool clean_active = false;
    bool clean_junk = false;
    if(clean_first_level >= 1) {
      clean_finished=true;
      if(clean_first_level >= 2) {
        clean_active=true;
        if(clean_first_level >= 3) {
          clean_junk=true;
        };
      };
    };
    for(;;) { 
      bool cleaned_all=true;
      for(JobUsers::iterator user = users.begin();user != users.end();++user) {
        size_t njobs = user->get_jobs()->size();
        user->get_jobs()->ScanNewJobs(false);
        if(user->get_jobs()->size() == njobs) break;
        cleaned_all=false;
        if(!(user->get_jobs()->DestroyJobs(clean_finished,clean_active)))  {
          logger.msg(Arc::WARNING,"Not all jobs are cleaned yet");
          sleep(10); 
          logger.msg(Arc::WARNING,"Trying again");
        };
        kill(getpid(),SIGCHLD);  /* make sure no child is missed */
      };
      if(cleaned_all) {
        if(clean_junk && clean_active && clean_finished) {  
          /* at the moment cleaning junk means cleaning all the files in 
             session and control directories */
          for(JobUsers::iterator user=users.begin();user!=users.end();++user) {
            std::list<FileData> flist;
            logger.msg(Arc::INFO,"Cleaning all files in directories %s and %s",user->SessionRoot(),user->ControlDir());
            delete_all_files(user->SessionRoot(),flist,true);
            delete_all_files(user->ControlDir(),flist,true);
          };
        };
        break;
      };
    };
    logger.msg(Arc::INFO,"Jobs cleaned");
  };
  // check if cleaning is enabled for any user, if so activate cleaning thread
  for (JobUsers::const_iterator cacheuser = users.begin(); cacheuser != users.end(); ++cacheuser) {
    if (cacheuser->CacheParams() && cacheuser->CacheParams()->cleanCache()) {
      if(pthread_create(&cache_thread,NULL,&cache_func,(void*)(&users))!=0) {
        logger.msg(Arc::INFO,"Failed to start new thread: cache won't be cleaned");
      }
      break;
    }
  }
  /* create control and session directories */
  for(JobUsers::iterator user = users.begin();user != users.end();++user) {
    user->CreateDirectories();
  };
  /* main loop - forewer */
  logger.msg(Arc::INFO,"Starting jobs' monitoring");
  hard_job_time = time(NULL) + HARD_JOB_PERIOD;
  for(;;) { 
    users.run_helpers();
    job_log.RunReporter(users);
    my_user->run_helpers();
    bool hard_job = time(NULL) > hard_job_time;
    for(JobUsers::iterator user = users.begin();user != users.end();++user) {
      /* look for new jobs */
      user->get_jobs()->ScanNewJobs(hard_job);
      /* process know jobs */
      user->get_jobs()->ActJobs(hard_job);
    };
    if(hard_job) hard_job_time = time(NULL) + HARD_JOB_PERIOD;
    pthread_mutex_lock(&sleep_mutex);
    pthread_cond_wait(&sleep_cond,&sleep_mutex);
    pthread_mutex_unlock(&sleep_mutex);
//#    if(run.was_hup()) {
//#      logger.msg(Arc::INFO,"SIGHUP detected");
//#//      if(!configure_serviced_users(users,my_uid,my_username,*my_user)) {
//#//        std::cout<<"Error processing configuration"<<std::endl; goto exit;
//#//      };
//#    }
//#    else {
      logger.msg(Arc::VERBOSE,"Timer kicking");
//#    };
  };
  return;
}

/*
GridManager::GridManager(Arc::XMLNode argv):active_(false) {
  args_st* args = new args_st;
  if(!args) return;
  args->argv=(char**)malloc(sizeof(char*)*(argv.Size()+1));
  args->argc=0;
  args->argv[args->argc]=strdup("grid-manager");
  logger.msg(Arc::DEBUG, "ARG: %s", args->argv[args->argc]);
  for(;;) {
    Arc::XMLNode arg = argv["arg"][args->argc];
    ++(args->argc);
    if(!arg) break;
    args->argv[args->argc]=strdup(((std::string)arg).c_str());
    logger.msg(Arc::DEBUG, "ARG: %s", args->argv[args->argc]);
  };
  active_=Arc::CreateThreadFunction(&grid_manager,args);
  if(!active_) delete args;
}
*/

GridManager::GridManager(const char* config_filename):active_(false) {
  void* arg = config_filename?strdup(config_filename):NULL;
  active_=Arc::CreateThreadFunction(&grid_manager,arg);
  if(!active_) if(arg) free(arg);
}

GridManager::~GridManager(void) {
}

} // namespace ARex

