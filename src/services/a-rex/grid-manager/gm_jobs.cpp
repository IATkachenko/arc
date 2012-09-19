#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdio>
#include <fstream>
#include <pwd.h>
#include <unistd.h>

#include <arc/XMLNode.h>
#include <arc/ArcConfig.h>
#include <arc/DateTime.h>
#include <arc/FileUtils.h>
#include <arc/Logger.h>
#include <arc/OptionParser.h>
#include <arc/StringConv.h>

#include "jobs/users.h"
#include "conf/conf_file.h"
#include "conf/conf_staging.h"
#include "jobs/plugins.h"
#include "files/info_files.h"
#include "jobs/commfifo.h"
#include "jobs/job_config.h"
#include "log/job_log.h"
#include "run/run_plugin.h"

static void get_arex_xml(Arc::XMLNode& arex,GMEnvironment& env) {
  
  Arc::Config cfg(env.nordugrid_config_loc().c_str());
  if (!cfg) return;

  if(cfg.Name() == "Service") {
    if (cfg.Attribute("name") == "a-rex") {
      cfg.New(arex);
    }
    return;
  }

  if(cfg.Name() != "ArcConfig") return;

  for (int i=0;; i++) {

    Arc::XMLNode node = cfg["Chain"];
    node = node["Service"][i];
    if (!node)
      return;
    if (node.Attribute("name") == "a-rex") {
      node.New(arex);
      return;
    }
  }
}

/** Fill maps with shares taken from data staging states log */
static void get_new_data_staging_shares(const std::string& control_dir,
                                 const GMEnvironment& env,
                                 std::map<std::string, int>& share_preparing,
                                 std::map<std::string, int>& share_preparing_pending,
                                 std::map<std::string, int>& share_finishing,
                                 std::map<std::string, int>& share_finishing_pending) {
  // get DTR configuration
  StagingConfig staging_conf(env);
  if (!staging_conf) {
    std::cout<<"Could not read data staging configuration from "<<env.nordugrid_config_loc()<<std::endl;
    return;
  }
  std::string dtr_log = staging_conf.get_dtr_log();
  if (dtr_log.empty()) dtr_log = control_dir+"/dtrstate.log";

  // read DTR state info
  std::list<std::string> data;
  if (!Arc::FileRead(dtr_log, data)) {
    std::cout<<"Can't read transfer states from "<<dtr_log<<". Perhaps A-REX is not running?"<<std::endl;
    return;
  }
  // format DTR_ID state priority share [destinatinon]
  // any state but TRANSFERRING is a pending state
  for (std::list<std::string>::iterator line = data.begin(); line != data.end(); ++line) {
    std::vector<std::string> entries;
    Arc::tokenize(*line, entries, " ");
    if (entries.size() < 4 || entries.size() > 6) continue;

    std::string state = entries[1];
    std::string share = entries[3];
    bool preparing = (share.find("-download") == share.size()-9);
    if (state == "TRANSFERRING") {
      preparing ? share_preparing[share]++ : share_finishing[share]++;
    }
    else {
      preparing ? share_preparing_pending[share]++ : share_finishing_pending[share]++;
    }
  }
}

class counters_t {
 public:
  unsigned int jobs_num[JOB_STATE_NUM];
  const static unsigned int jobs_pending;
  unsigned int& operator[](int n) { return jobs_num[n]; };
};

const unsigned int counters_t::jobs_pending = 0;

static bool match_list(const std::string& arg, std::list<std::string>& args, bool erase) {
  for(std::list<std::string>::const_iterator a = args.begin();
                                             a != args.end(); ++a) {
    if(*a == arg) {
      //if(erase) args.erase(a);
      return true;
    }
  }
  return false;
}

/**
 * Print info to stdout on users' jobs
 */
int main(int argc, char* argv[]) {

  // stderr destination for error messages
  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::DEBUG);
  
  Arc::OptionParser options(" ",
                            istring("gm-jobs displays information on "
                                    "current jobs in the system."));
  
  bool long_list = false;
  options.AddOption('l', "longlist",
                    istring("display more information on each job"),
                    long_list);

  std::string my_username;
  options.AddOption('U', "username",
                    istring("pretend utility is run by user with given name"),
                    istring("name"), my_username);
  
  int my_uid = getuid();
  options.AddOption('u', "userid",
                    istring("pretend utility is run by user with given UID"),
                    istring("uid"), my_uid);
  
  std::string conf_file;
  options.AddOption('c', "conffile",
                    istring("use specified configuration file"),
                    istring("file"), conf_file);
  
  std::string control_dir;
  options.AddOption('d', "controldir",
                    istring("read information from specified control directory"),
                    istring("dir"), control_dir);
                    
  bool show_share = false;
  options.AddOption('s', "showshares",
		    istring("print summary of jobs in each transfer share"),
		    show_share);

  bool notshow_jobs = false;
  options.AddOption('J', "notshowjobs",
		    istring("do not print list of jobs"),
		    notshow_jobs);

  bool notshow_states = false;
  options.AddOption('S', "notshowstates",
		    istring("do not print number of jobs in each state"),
		    notshow_states);

  bool show_service = false;
  options.AddOption('w', "showservice",
		    istring("print state of the service"),
		    show_service);

  std::list<std::string> filter_users;
  options.AddOption('f', "filteruser",
		    istring("show only jobs of user(s) with specified DN(s)"),
		    istring("dn"), filter_users);

  std::list<std::string> cancel_jobs;
  options.AddOption('k', "killjob",
                    istring("request to cancel job(s) with specified id(s)"),
                    istring("id"), cancel_jobs);

  std::list<std::string> cancel_users;
  options.AddOption('K', "killuser",
                    istring("request to cancel jobs belonging to user(s) with specified DN(s)"),
                    istring("dn"), cancel_users);

  std::list<std::string> clean_jobs;
  options.AddOption('r', "remjob",
                    istring("request to clean job(s) with specified id(s)"),
                    istring("id"), clean_jobs);

  std::list<std::string> clean_users;
  options.AddOption('R', "remuser",
                    istring("request to clean jobs belonging to user(s) with specified DN(s)"),
                    istring("dn"), clean_users);

  std::list<std::string> params = options.Parse(argc, argv);

  if(show_share) { // Why?
    notshow_jobs=true;
    notshow_states=true;
  }

  if (!my_username.empty()) {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwnam_r(my_username.c_str(), &pw_, buf, BUFSIZ, &pw);
    if(pw == NULL) {
      std::cout<<"Unknown user: "<<my_username<<std::endl;
      return 1;
    }
    my_uid=pw->pw_uid;
  } else {
    struct passwd pw_;
    struct passwd *pw;
    char buf[BUFSIZ];
    getpwuid_r(my_uid, &pw_, buf, BUFSIZ, &pw);
    if(pw == NULL) {
      std::cout<<"Can't recognize myself."<<std::endl;
      return 1;
    }
    my_username=pw->pw_name;
  }

  JobLog job_log;
  JobsListConfig jobs_cfg;
  ContinuationPlugins plugins;
  RunPlugin cred_plugin;
  GMEnvironment env(job_log,jobs_cfg,plugins,cred_plugin);

  if (!conf_file.empty())
    env.nordugrid_config_loc(conf_file);
  
  //if(!read_env_vars())
  if(!env)
    exit(1);
  
  JobUsers users(env);
  
  if (control_dir.empty()) {
    JobUser my_user(env,my_username);
    std::ifstream cfile;
    if(!config_open(cfile,env)) {
      std::cout<<"Can't read configuration file"<<std::endl;
      return 1;
    }
    bool enable_arc = false;
    bool enable_emies = false;
    if (config_detect(cfile) == config_file_XML) {
      // take out the element that can be passed to configure_serviced_users
      Arc::XMLNode arex;
      get_arex_xml(arex,env);
      if (!arex || !configure_serviced_users(arex, users/*, my_uid, my_username*/, my_user, enable_arc, enable_emies)) {
        std::cout<<"Error processing configuration."<<std::endl;
        return 1;
      }
    } else if(!configure_serviced_users(users/*, my_uid, my_username*/, my_user, enable_arc, enable_emies)) {
      std::cout<<"Error processing configuration."<<std::endl;
      return 1;
    }
    if(users.size() == 0) {
      std::cout<<"No suitable users found in configuration."<<std::endl;
      return 1;
    }
    print_serviced_users(users);
  }
  else {
    ContinuationPlugins plugins;
    JobUsers::iterator jobuser = users.AddUser(my_username, NULL, control_dir);
    JobsList *jobs = new JobsList(*jobuser,plugins); 
    (*jobuser)=jobs; 
  }

  if((!notshow_jobs) || (!notshow_states) || (show_share) ||
     (cancel_users.size() > 0) || (clean_users.size() > 0) ||
     (cancel_jobs.size() > 0) || (clean_jobs.size() > 0)) {
    std::cout << "Looking for current jobs" << std::endl;
  }

  bool service_alive = false;

  counters_t counters;
  counters_t counters_pending;

  for(int i=0; i<JOB_STATE_NUM; i++) {
    counters[i] = 0;
    counters_pending[i] = 0;
  }

  std::map<std::string, int> share_preparing;
  std::map<std::string, int> share_preparing_pending;
  std::map<std::string, int> share_finishing;
  std::map<std::string, int> share_finishing_pending;

  unsigned int jobs_total = 0;
  std::list<std::pair<JobDescription*,JobUser*> > cancel_jobs_list;
  std::list<std::pair<JobDescription*,JobUser*> > clean_jobs_list;
  for (JobUsers::iterator user = users.begin(); user != users.end(); ++user) {
    if((!notshow_jobs) || (!notshow_states) || (show_share) ||
       (cancel_users.size() > 0) || (clean_users.size() > 0) ||
       (cancel_jobs.size() > 0) || (clean_jobs.size() > 0)) {
      if (show_share && jobs_cfg.GetNewDataStaging()) {
        get_new_data_staging_shares(user->ControlDir(), env,
                                    share_preparing, share_preparing_pending,
                                    share_finishing, share_finishing_pending);
      }
      user->get_jobs()->ScanAllJobs();
      for (JobsList::iterator i=user->get_jobs()->begin(); i!=user->get_jobs()->end(); ++i) {
        // Collecting information
        bool pending;
        job_state_t new_state = job_state_read_file(i->get_id(), *user, pending);
        if (new_state == JOB_STATE_UNDEFINED) {
          std::cout<<"Job: "<<i->get_id()<<" : ERROR : Unrecognizable state"<<std::endl;
          continue;
        }
        Arc::Time job_time(job_state_time(i->get_id(), *user));
        jobs_total++;
        counters[new_state]++;
        if (pending) counters_pending[new_state]++;
        if (!i->GetLocalDescription(*user)) {
          std::cout<<"Job: "<<i->get_id()<<" : ERROR : No local information."<<std::endl;
          continue;
        }
        JobLocalDescription& job_desc = *(i->get_local());
        if (show_share && !jobs_cfg.GetNewDataStaging()) {
          if(new_state == JOB_STATE_PREPARING && !pending) share_preparing[job_desc.transfershare]++;
          else if(new_state == JOB_STATE_ACCEPTED && pending) share_preparing_pending[job_desc.transfershare]++;
          else if(new_state == JOB_STATE_FINISHING) share_finishing[job_desc.transfershare]++;
          else if(new_state == JOB_STATE_INLRMS && pending) {
            std::string jobid = i->get_id();
            if (job_lrms_mark_check(jobid,*user)) share_finishing_pending[job_desc.transfershare]++;
          }
        }
        if(match_list(job_desc.DN,cancel_users,false)) {
          cancel_jobs_list.push_back(std::pair<JobDescription*,JobUser*>(&(*i),&(*user)));
        }
        if(match_list(i->get_id(),cancel_jobs,false)) {
          cancel_jobs_list.push_back(std::pair<JobDescription*,JobUser*>(&(*i),&(*user)));
        }
        if(match_list(job_desc.DN,clean_users,false)) {
          clean_jobs_list.push_back(std::pair<JobDescription*,JobUser*>(&(*i),&(*user)));
        }
        if(match_list(i->get_id(),clean_jobs,false)) {
          clean_jobs_list.push_back(std::pair<JobDescription*,JobUser*>(&(*i),&(*user)));
        }
        // Printing information
        if((filter_users.size() > 0) && (!match_list(job_desc.DN,filter_users,false))) continue;
        if((!show_share) && (!notshow_jobs)) std::cout << "Job: "<<i->get_id();
        if(!notshow_jobs) {
          if (!long_list) {
            std::cout<<" : "<<states_all[new_state].name<<" : "<<job_desc.DN<<" : "<<job_time.str()<<std::endl;
            continue;
          }
          std::cout<<std::endl;
          std::cout<<"\tState: "<<states_all[new_state].name;
          if (pending)
            std::cout<<" (PENDING)";
          std::cout<<std::endl;
          std::cout<<"\tModified: "<<job_time.str()<<std::endl;
          std::cout<<"\tUser: "<<job_desc.DN<<std::endl;
          if (!job_desc.localid.empty())
            std::cout<<"\tLRMS id: "<<job_desc.localid<<std::endl;
          if (!job_desc.jobname.empty())
            std::cout<<"\tName: "<<job_desc.jobname<<std::endl;
          if (!job_desc.clientname.empty())
            std::cout<<"\tFrom: "<<job_desc.clientname<<std::endl;
        }
      }
    }
    if(show_service) {
      if(PingFIFO(*user)) service_alive = true;
    }
  }
  
  if(show_share) {
    std::cout<<"\n Preparing/Pending "<<(jobs_cfg.GetNewDataStaging() ? "files" : "jobs ")<<"\tTransfer share"<<std::endl;
    for (std::map<std::string, int>::iterator i = share_preparing.begin(); i != share_preparing.end(); i++) {
      std::cout<<"         "<<i->second<<"/"<<share_preparing_pending[i->first]<<"\t\t\t"<<i->first<<std::endl;
    }
    for (std::map<std::string, int>::iterator i = share_preparing_pending.begin(); i != share_preparing_pending.end(); i++) {
      if (share_preparing[i->first] == 0)
        std::cout<<"         0/"<<share_preparing_pending[i->first]<<"\t\t\t"<<i->first<<std::endl;
    }
    std::cout<<"\n Finishing/Pending "<<(jobs_cfg.GetNewDataStaging() ? "files" : "jobs ")<<"\tTransfer share"<<std::endl;
    for (std::map<std::string, int>::iterator i = share_finishing.begin(); i != share_finishing.end(); i++) {
      std::cout<<"         "<<i->second<<"/"<<share_finishing_pending[i->first]<<"\t\t\t"<<i->first<<std::endl;
    }
    for (std::map<std::string, int>::iterator i = share_finishing_pending.begin(); i != share_finishing_pending.end(); i++) {
      if (share_finishing[i->first] == 0)
        std::cout<<"         0/"<<share_finishing_pending[i->first]<<"\t\t\t"<<i->first<<std::endl;
    }
    std::cout<<std::endl;
  }
  
  if(!notshow_states) {
    std::cout<<"Jobs total: "<<jobs_total<<std::endl;

    for (int i=0; i<JOB_STATE_UNDEFINED; i++) {
      std::cout<<" "<<states_all[i].name<<": "<<counters[i]<<" ("<<counters_pending[i]<<")"<<std::endl;
    }
    int max;
    int max_running;
    int max_total;
    int max_processing;
    int max_processing_emergency;
    int max_down;
    int max_per_dn;

    env.jobs_cfg().GetMaxJobs(max, max_running, max_per_dn, max_total);
    env.jobs_cfg().GetMaxJobsLoad(max_processing, max_processing_emergency, max_down);

//    #undef jobs_pending
//    #define jobs_pending 0
//    #undef jobs_num
//    #define jobs_num counters
    #undef jcfg
    #define jcfg counters
    int accepted = JOB_NUM_ACCEPTED;
    int running = JOB_NUM_RUNNING;
//    #undef jobs_num
//    #define jobs_num counters_pending
    #undef jcfg
    #define jcfg counters_pending
    running-=JOB_NUM_RUNNING;
    #undef jcfg
  
    std::cout<<" Accepted: "<<accepted<<"/"<<max<<std::endl;
    std::cout<<" Running: "<<running<<"/"<<max_running<<std::endl;
    std::cout<<" Total: "<<jobs_total<<"/"<<max_total<<std::endl;
    std::cout<<" Processing: "<<
    counters[JOB_STATE_PREPARING]-counters_pending[JOB_STATE_PREPARING]<<"+"<<
    counters[JOB_STATE_FINISHING]-counters_pending[JOB_STATE_FINISHING]<<"/"<<
    max_processing<<"+"<<max_processing_emergency<<std::endl;
  }
  if(show_service) {
    std::cout<<" Service state: "<<(service_alive?"alive":"not detected")<<std::endl;
  }
  
  if(cancel_jobs_list.size() > 0) {
    for(std::list<std::pair<JobDescription*,JobUser*> >::iterator job = cancel_jobs_list.begin();
                            job != cancel_jobs_list.end(); ++job) {
      if(!job_cancel_mark_put(*(job->first),*(job->second))) {
        std::cout<<"Job: "<<job->first->get_id()<<" : ERROR : Failed to put cancel mark"<<std::endl;
      } else {
        std::cout<<"Job: "<<job->first->get_id()<<" : Cancel request put"<<std::endl;
      }
    }
  }
  if(clean_jobs_list.size() > 0) {
    for(std::list<std::pair<JobDescription*,JobUser*> >::iterator job = clean_jobs_list.begin();
                            job != clean_jobs_list.end(); ++job) {
      bool pending;
      job_state_t new_state = job_state_read_file(job->first->get_id(), *(job->second), pending);
      if((new_state == JOB_STATE_FINISHED) || (new_state == JOB_STATE_DELETED)) {
        if(!job_clean_final(*(job->first),*(job->second))) {
          std::cout<<"Job: "<<job->first->get_id()<<" : ERROR : Failed to clean"<<std::endl;
        } else {
          std::cout<<"Job: "<<job->first->get_id()<<" : Cleaned"<<std::endl;
        }
      } else {
        if(!job_clean_mark_put(*(job->first),*(job->second))) {
          std::cout<<"Job: "<<job->first->get_id()<<" : ERROR : Failed to put clean mark"<<std::endl;
        } else {
          std::cout<<"Job: "<<job->first->get_id()<<" : Clean request put"<<std::endl;
        }
      }
    }
  }

  for (JobUsers::iterator user = users.begin(); user != users.end(); ++user) {
    JobsList* jobs = user->get_jobs();
    if(jobs) delete jobs;
  }

  return 0;
}
