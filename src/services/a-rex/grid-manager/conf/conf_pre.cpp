#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>
#include <iostream>
#include <pwd.h>

#include <arc/StringConv.h>
#include <arc/Logger.h>
#include "conf.h"
#include "conf_sections.h"
#include "environment.h"
#include "gridmap.h"
#include "../jobs/job.h"
#include "../jobs/users.h"
#include "../jobs/plugins.h"
#include "../run/run_plugin.h"
#include "conf_pre.h"

Arc::Logger& logger = Arc::Logger::getRootLogger();

bool configure_user_dirs(const std::string &my_username,
                std::string &control_dir,std::string &session_root,
                std::string &default_lrms,std::string &default_queue,
                std::list<std::string>& queues,
                ContinuationPlugins &plugins,RunPlugin &cred_plugin,
                std::string& allow_submit,bool& strict_session) {
  std::ifstream cfile;
  read_env_vars(true);
  bool configured = false;
  std::string central_control_dir("");
  ConfigSections* cf = NULL;
  std::list<std::string>::iterator last_queue = queues.end();

  strict_session = false;
  if(!config_open(cfile)) {
    logger.msg(Arc::ERROR,"Can't open configuration file"); return false;
  };
  cf=new ConfigSections(cfile);
  cf->AddSection("common");
  cf->AddSection("grid-manager");
  cf->AddSection("queue");
  for(;;) {
    std::string rest;
    std::string command;
    cf->ReadNext(command,rest);
    if(command.length() == 0) {
      if(central_control_dir.length() != 0) {
        command="control"; rest=central_control_dir+" .";
        central_control_dir="";
      } else {
        break;
      };
    };
    if(cf->SectionNum() == 2) { // queue
      if(cf->SectionNew()) {
        const char* name = cf->SubSection();
        if(!name) name="";
        last_queue=queues.insert(queues.end(),std::string(name));
      };
      if(command == "name") {
        if(last_queue != queues.end()) *last_queue=config_next_arg(rest);
      };
    }
    else if(command == "lrms") {
      if(configured) continue;
      default_lrms = config_next_arg(rest);
      default_queue = config_next_arg(rest);
      if(default_lrms.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
    }
    else if(command == "authplugin") { /* set plugin to be called on
                                          state changes */
      std::string state_name = config_next_arg(rest);
      if(state_name.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
      std::string options_s = config_next_arg(rest);
      if(options_s.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
      if(!plugins.add(state_name.c_str(),options_s.c_str(),rest.c_str())) {
        config_close(cfile); if(cf) delete cf; return false;
      };
    }
    else if(command == "localcred") {
      std::string timeout_s = config_next_arg(rest);
      if(timeout_s.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
      char *ep;
      int to = strtoul(timeout_s.c_str(),&ep,10);
      if((*ep != 0) || (to<0)) { config_close(cfile); if(cf) delete cf; return false; };
      cred_plugin = rest;
      cred_plugin.timeout(to);
    }
    else if(command == "allowsubmit") {
      allow_submit+=" "; allow_submit+=rest;
    }
    else if(command == "norootpower") {
      std::string s = config_next_arg(rest);
      if(strcasecmp("yes",s.c_str()) == 0) {
        strict_session=true;
      }
      else if(strcasecmp("no",s.c_str()) == 0) {
        strict_session=false;
      };
    }
    else if(command == "sessiondir") {
      if(configured) continue;
      session_root = config_next_arg(rest);
      if(session_root.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
      if(session_root == "*") session_root="";
    }
    else if(command == "controldir") {
      central_control_dir=rest;
    }
    else if(command == "control") {
      if(configured) continue;
      control_dir = config_next_arg(rest);
      if(control_dir.length() == 0) { config_close(cfile); if(cf) delete cf; return false; };
      if(control_dir == "*") control_dir="";
      for(;;) {
        std::string username = config_next_arg(rest);
        if(username.length() == 0) break;
        if(username == "*") {  /* add all gridmap users */
          if(!gridmap_user_list(rest)) { config_close(cfile); if(cf) delete cf; return false; };
          continue;
        };
        if((username == my_username) || (username == ".")) { 
          if(username == ".") username = "";
          JobUser user(username);
          if(!user.is_valid()) { config_close(cfile); if(cf) delete cf; return false; };
          user.SetLRMS(default_lrms,default_queue);
          user.substitute(control_dir);
          user.substitute(session_root);
          user.SetSessionRoot(session_root);
          user.SetControlDir(control_dir);
          session_root=user.SessionRoot();
          control_dir=user.ControlDir();
          configured=true;
        };
      };
    };
  };
  config_close(cfile);
  if(cf) delete cf;
  return configured;
}
