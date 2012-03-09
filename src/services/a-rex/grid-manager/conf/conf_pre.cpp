#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <unistd.h>

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
                std::string &control_dir,std::vector<std::string> &session_roots,
                std::vector<std::string> &session_roots_non_draining,
                std::string &default_lrms,std::string &default_queue,
                std::list<std::string>& queues,
                ContinuationPlugins &plugins,RunPlugin &cred_plugin,
                std::string& allow_submit,bool& strict_session,
                std::string& gridftp_endpoint,
                std::string& arex_endpoint,
                bool& enable_arc_interface,
                bool& enable_emies_interface,
                const GMEnvironment& env) {
  std::ifstream cfile;
  int gm_port = 0;
  std::string gm_mount_point;
  std::string gm_hostname;
  std::string arex_mount_point;
  bool enable_arc = true;
  bool enable_emies = false;
  //read_env_vars(true);
  bool configured = false;
  std::string central_control_dir("");
  ConfigSections* cf = NULL;
  std::list<std::string>::iterator last_queue = queues.end();

  strict_session = false;
  if(!config_open(cfile,env)) {
    logger.msg(Arc::ERROR,"Can't open configuration file"); return false;
  };
  switch(config_detect(cfile)) {
    case config_file_XML: {
      Arc::XMLNode cfg;
      if(!cfg.ReadFromStream(cfile)) {
        logger.msg(Arc::ERROR,"Can't interpret configuration file as XML");
        config_close(cfile);
        return false;
      };
      // Unsupported elements:
      //   std::string& allow_submit
      allow_submit=true;
      Arc::XMLNode tmp_node;
      tmp_node = cfg["LRMS"];
      if(tmp_node) {
        default_lrms = (std::string)(tmp_node["type"]);
        default_queue = (std::string)(tmp_node["defaultShare"]);
      };
      tmp_node = cfg["authPlugin"];
      for(;tmp_node;++tmp_node) {
        std::string state_name = tmp_node["state"];
        if(state_name.empty()) continue;
        std::string command = tmp_node["command"];
        if(command.empty()) continue;
        std::string options;
        Arc::XMLNode onode;
        onode = tmp_node.Attribute("timeout");
        if(onode) options+="timeout="+(std::string)onode;
        onode = tmp_node.Attribute("onSuccess");
        if(onode) options+="onsuccess="+Arc::lower((std::string)onode);
        onode = tmp_node.Attribute("onFailure");
        if(onode) options+="onfailure="+Arc::lower((std::string)onode);
        onode = tmp_node.Attribute("onTimeout");
        if(onode) options+="ontimeout="+Arc::lower((std::string)onode);
        if(!plugins.add(state_name.c_str(),options.c_str(),command.c_str())) {
          config_close(cfile);
          return false;
        };
      };
      tmp_node = cfg["localCred"];
      if(tmp_node) {
        std::string command = tmp_node["command"];
        if(command.empty()) return false;
        std::string options;
        Arc::XMLNode onode;
        onode = tmp_node.Attribute("timeout");
        if(!onode) return false;
        int to;
        if(!elementtoint(onode,NULL,to,&logger)) return false;
        cred_plugin = command;
        cred_plugin.timeout(to);
      };
      tmp_node = cfg["InfoProvider"];
      if(tmp_node) {
        Arc::XMLNode shnode = tmp_node["ComputingShare"];
        for(;shnode;++shnode) {
          std::string qname = shnode["MappingQueue"];
          if(qname.empty()) qname=(std::string)(shnode.Attribute("name"));
          if(!qname.empty()) queues.insert(queues.end(),qname);
        };
      }
      tmp_node = cfg["control"];
      for(;tmp_node;++tmp_node) {
        Arc::XMLNode unode = tmp_node["username"];
        bool user_match = false;
        std::string username;
        for(;unode;++unode) {
          username = (std::string)unode;
          if(username.empty()) continue;
          if(username == "*") {  /* add all gridmap users */
            logger.msg(Arc::ERROR,"Gridmap user list feature is not supported anymore. Plase use @filename to specify user list.");
            config_close(cfile);
            return false;
          };
          if(username[0] == '@') {  /* add users from file */
            std::list<std::string> userlist;
            std::string filename = username.substr(1);
            if(!file_user_list(filename,userlist)) {
              logger.msg(Arc::ERROR,"Can't read users in specified file %s",filename);
              config_close(cfile);
              return false;
            };
            for(std::list<std::string>::iterator u = userlist.begin();
                                     u != userlist.end();++u) {
              username = *u;
              if(username == my_username) {
                user_match=true;
                break;
              };
            };
            if(user_match) break;
            continue;
          };
          if(username == ".") {
            user_match=true;
            break;
          };
          if(username == my_username) {
            user_match=true;
            break;
          };
        };
        if(user_match) {
          std::string control_dir_ = tmp_node["controlDir"];
          Arc::XMLNode session_node = tmp_node["sessionRootDir"];
          for (;session_node; ++session_node) {
            std::string session_root = session_node;
            if(session_root.empty()) {
              logger.msg(Arc::ERROR,"sessionRootDir is missing"); return false;
            };
            std::string::size_type i = session_root.find(' ');
            if (i != std::string::npos) {
              if (session_root.substr(i+1) != "drain") {
                session_roots_non_draining.push_back(session_root.substr(0, i));
              }
              session_root = session_root.substr(0, i);
            } else {
              session_roots_non_draining.push_back(session_root);
            }
            session_roots.push_back(session_root);
          }
          if(username == ".") username = "";
          JobUser user(env,username);
          if(!user.is_valid()) { config_close(cfile); return false; };
          strict_session = false;
          elementtobool(tmp_node,"noRootPower",strict_session,&logger);
          user.SetLRMS(default_lrms,default_queue);
          user.substitute(control_dir_);
          user.SetControlDir(control_dir_);
          std::vector<std::string> session_roots_non_draining_;
          for(std::vector<std::string>::iterator i = session_roots_non_draining.begin(); i != session_roots_non_draining.end(); i++) {
            std::string session(*i);
            user.substitute(session);
            session_roots_non_draining_.push_back(session);
          }
          user.SetSessionRoot(session_roots_non_draining_);
          session_roots_non_draining = user.SessionRoots();
          std::vector<std::string> session_roots_;
          for(std::vector<std::string>::iterator i = session_roots.begin(); i != session_roots.end(); i++) {
            std::string session(*i);
            user.substitute(session);
            session_roots_.push_back(session);
          }
          user.SetSessionRoot(session_roots_);
          session_roots=user.SessionRoots();
          control_dir=user.ControlDir();
          configured=true;
          break;
        };
      }; // for(control)
    }; break;
    case config_file_INI: {
      cf=new ConfigSections(cfile);
      cf->AddSection("common");
      cf->AddSection("grid-manager");
      cf->AddSection("queue");
      cf->AddSection("cluster");
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
        else if(cf->SectionNum() == 3) { // cluster
          if(command == "gm_port") {
            char *ep;
            gm_port = strtoul(config_next_arg(rest).c_str(),&ep,10);
            if((*ep != 0) || (gm_port<0)) { config_close(cfile); delete cf; return false; };
          } else if(command == "hostname") {
            gm_hostname = config_next_arg(rest);
          };
        }
        else if(command == "gm_mount_point") {
          gm_mount_point = config_next_arg(rest);
        }
        else if(command == "arex_mount_point") {
          arex_mount_point = config_next_arg(rest);
        }
        else if(command == "enable_arc_interface") {
          enable_arc = (Arc::lower(config_next_arg(rest)) == "yes");
        }
        else if(command == "enable_emies_interface") {
          enable_emies = (Arc::lower(config_next_arg(rest)) == "yes");
        }
        else if(command == "hostname") {
          gm_hostname = config_next_arg(rest);
        }
        else if(command == "lrms") {
          if(configured) continue;
          default_lrms = config_next_arg(rest);
          default_queue = config_next_arg(rest);
          if(default_lrms.length() == 0) { config_close(cfile); delete cf; return false; };
        }
        else if(command == "authplugin") { /* set plugin to be called on
                                              state changes */
          std::string state_name = config_next_arg(rest);
          if(state_name.length() == 0) { config_close(cfile); delete cf; return false; };
          std::string options_s = config_next_arg(rest);
          if(options_s.length() == 0) { config_close(cfile); delete cf; return false; };
          if(!plugins.add(state_name.c_str(),options_s.c_str(),rest.c_str())) {
            config_close(cfile); delete cf; return false;
          };
        }
        else if(command == "localcred") {
          std::string timeout_s = config_next_arg(rest);
          if(timeout_s.length() == 0) { config_close(cfile); delete cf; return false; };
          char *ep;
          int to = strtoul(timeout_s.c_str(),&ep,10);
          if((*ep != 0) || (to<0)) { config_close(cfile); delete cf; return false; };
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
          std::string session_root = config_next_arg(rest);
          if(session_root.length() == 0) { config_close(cfile); delete cf; return false; };
          session_roots.push_back(session_root);
          std::string drain = config_next_arg(rest);
          if (drain.empty() || drain != "drain") {
            session_roots_non_draining.push_back(session_root);
          }
        }
        else if(command == "controldir") {
          central_control_dir=rest;
        }
        else if(command == "control") {
          if(configured) continue;
          control_dir = config_next_arg(rest);
          if(control_dir.length() == 0) { config_close(cfile); delete cf; return false; };
          if(control_dir == "*") control_dir="";
          for(;;) {
            std::string username = config_next_arg(rest);
            if(username.length() == 0) break;
            if(username == "*") {  /* add all gridmap users */
              config_close(cfile); delete cf; return false;
            };
            if(username[0] == '@') {  /* add users from file */
              std::string filename = username.substr(1);
              if(!file_user_list(filename,rest)) { config_close(cfile); delete cf; return false; };
              continue;
            };
            if((username == my_username) || (username == ".")) { 
              if(username == ".") username = "";
              JobUser user(env,username);
              if(!user.is_valid()) { config_close(cfile); delete cf; return false; };
              user.SetLRMS(default_lrms,default_queue);
              user.substitute(control_dir);
              user.SetControlDir(control_dir);
              std::vector<std::string> session_roots_non_draining_;
              for(std::vector<std::string>::iterator i = session_roots_non_draining.begin(); i != session_roots_non_draining.end(); i++) {
                std::string session(*i);
                user.substitute(session);
                session_roots_non_draining_.push_back(session);
              }
              user.SetSessionRoot(session_roots_non_draining_);
              session_roots_non_draining=user.SessionRoots();
              std::vector<std::string> session_roots_;
              for(std::vector<std::string>::iterator i = session_roots.begin(); i != session_roots.end(); i++) {
                std::string session(*i);
                user.substitute(session);
                session_roots_.push_back(session);
              }
              user.SetSessionRoot(session_roots_);
              session_roots=user.SessionRoots();
              control_dir=user.ControlDir();
              configured=true;
              break;
            };
          };
          if (!configured) {
            session_roots.clear();
            session_roots_non_draining.clear();
          };
        };
      };
    }; break;

    default: {
    }; break;
  };
  config_close(cfile);
  if(cf) delete cf;
  if(gm_hostname.empty()) {
    char buf[256];
    buf[0]=0; buf[255]=0;
    ::gethostname(buf,255);
    gm_hostname=buf;
  };
  if(gm_port <= 0) gm_port=2811;
  if(gm_mount_point[0] != '/') gm_mount_point = "/"+gm_mount_point;;
  gridftp_endpoint = "gsiftp://"+gm_hostname+":"+Arc::tostring(gm_port)+gm_mount_point;
  arex_endpoint = arex_mount_point;
  enable_arc_interface = enable_arc;
  enable_emies_interface = enable_emies;
  return true;
}

bool configure_users_dirs(Arc::XMLNode cfg,JobUsers& users,bool& enable_arc_interface,bool& enable_emies_interface) {
  Arc::XMLNode tmp_node;
  tmp_node = cfg["enableARCInterface"];
  if(tmp_node) {
    if (Arc::lower((std::string)tmp_node) == "no") {
      enable_arc_interface=false;
    } else {
      enable_arc_interface=true;
    }
  }
  tmp_node = cfg["enableEMIESInterface"];
  if(tmp_node) {
    if (Arc::lower((std::string)tmp_node) == "no") {
      enable_emies_interface=false;
    } else {
      enable_emies_interface=true;
    }
  }
  tmp_node = cfg["control"];
  for(;tmp_node;++tmp_node) {
    bool strict_session = false;
    Arc::XMLNode unode = tmp_node["username"];
    std::list<std::string> usernames;
    elementtobool(tmp_node,"noRootPower",strict_session,&logger);
    JobUser::fixdir_t fixdir = JobUser::fixdir_always;
    const char* fixdir_opts[] = { "yes", "missing", "no", NULL};
    int n = (int)fixdir;
    elementtoenum(tmp_node,"fixDirectories",n,fixdir_opts,&logger);
    fixdir = (JobUser::fixdir_t)n;
    for(;unode;++unode) {
      std::string username;
      username = (std::string)unode;
      if(username.empty()) continue;
      if(username == "*") {  /* add all gridmap users */
        logger.msg(Arc::ERROR,"Gridmap user list feature is not supported anymore. Plase use @filename to specify user list.");
        return false;
      };
      if(username[0] == '@') {  /* add users from file */
        std::list<std::string> userlist;
        std::string filename = username.substr(1);
        if(!file_user_list(filename,userlist)) {
          logger.msg(Arc::ERROR,"Can't read users in specified file %s",filename);
          return false;
        };
        for(std::list<std::string>::iterator u = userlist.begin();
                                 u != userlist.end();++u) {
          usernames.push_back(*u);
        };
        continue;
      };
      if(username == ".") {
        usernames.push_back("");
        continue;
      };
      usernames.push_back(username);
    };
    for(std::list<std::string>::iterator u = usernames.begin();
                             u != usernames.end();++u) {
      std::string control_dir = tmp_node["controlDir"];
      std::string session_root = tmp_node["sessionRootDir"];
      JobUsers::iterator user=users.AddUser(*u);
      if(user == users.end()) return false;
      user->substitute(control_dir);
      user->substitute(session_root);
      user->SetControlDir(control_dir);
      user->SetSessionRoot(session_root);
      user->SetStrictSession(strict_session);
      user->SetFixDirectories(fixdir);
    };
  }; // for(control)
  return true;
}

bool configure_users_dirs(JobUsers& users,GMEnvironment& env,bool& enable_arc_interface,bool& enable_emies_interface) {
  std::ifstream cfile;
  //read_env_vars(true);
  std::string central_control_dir("");
  ConfigSections* cf = NULL;
  std::string session_root;
  JobUser::fixdir_t fixdir = JobUser::fixdir_always;
  bool strict_session = false;


  if(!config_open(cfile,env)) {
    logger.msg(Arc::ERROR,"Can't open configuration file"); return false;
  };
  switch(config_detect(cfile)) {
    case config_file_XML: {
      Arc::XMLNode cfg;
      if(!cfg.ReadFromStream(cfile)) {
        logger.msg(Arc::ERROR,"Can't interpret configuration file as XML");
        config_close(cfile);
        return false;
      };
      if(!configure_users_dirs(cfg,users,enable_arc_interface,enable_emies_interface)) {
        config_close(cfile);
        return false;
      };
    }; break;
    case config_file_INI: {
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
        else if(command == "fixdirectories") {
          std::string s = config_next_arg(rest);
          if(strcasecmp("yes",s.c_str()) == 0) {
            fixdir=JobUser::fixdir_always;
          }
          else if(strcasecmp("missing",s.c_str()) == 0) {
            fixdir=JobUser::fixdir_missing;
          }
          else if(strcasecmp("no",s.c_str()) == 0) {
            fixdir=JobUser::fixdir_never;
          }
        }
        else if(command == "sessiondir") {
          session_root = config_next_arg(rest);
          if(session_root.length() == 0) { config_close(cfile); delete cf; return false; };
          if(session_root == "*") session_root="";
        }
        else if(command == "controldir") {
          central_control_dir=rest;
        }
        else if(command == "control") {
          std::string control_dir = config_next_arg(rest);
          if(control_dir.length() == 0) { config_close(cfile); delete cf; return false; };
          if(control_dir == "*") control_dir="";
          for(;;) {
            std::string username = config_next_arg(rest);
            if(username.length() == 0) break;
            if(username == "*") {  /* add all gridmap users */
              config_close(cfile); delete cf; return false;
            };
            if(username[0] == '@') {  /* add users from file */
              std::string filename = username.substr(1);
              if(!file_user_list(filename,rest)) { config_close(cfile); delete cf; return false; };
              continue;
            };
            if(username == ".") username = "";
            JobUsers::iterator user=users.AddUser(username);
            if(user == users.end()) { config_close(cfile); delete cf; return false; };
            user->substitute(control_dir);
            user->substitute(session_root);
            user->SetControlDir(control_dir);
            user->SetSessionRoot(session_root);
            user->SetStrictSession(strict_session);
            user->SetFixDirectories(fixdir);
          };
        }
        else if(command == "enable_arc_interface") {
          enable_arc_interface = (Arc::lower(config_next_arg(rest)) == "yes");
        }
        else if(command == "enable_emies_interface") {
          enable_emies_interface = (Arc::lower(config_next_arg(rest)) == "yes");
        };
      };
    }; break;

    default: {
    }; break;
  };
  config_close(cfile);
  if(cf) delete cf;
  return true;
}

