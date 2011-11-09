#include <fstream>
#include <iostream>

#include <arc/Logger.h>

#include "../run/run_plugin.h"
#include "../misc/escaped.h"
#include "simplemap.h"

#include "unixmap.h"

static Arc::Logger logger(Arc::Logger::getRootLogger(),"UnixMap");

UnixMap::source_t UnixMap::sources[] = {
  { "mapfile", &UnixMap::map_mapfile, NULL },
  { "simplepool", &UnixMap::map_simplepool, NULL },
  { "unixuser", &UnixMap::map_unixuser, NULL },
  { "lcmaps", &UnixMap::map_lcmaps, NULL },
  { "mapplugin", &UnixMap::map_mapplugin, NULL },
  { NULL, NULL, NULL }
};

UnixMap::UnixMap(AuthUser& user,const std::string& id):
  user_(user),map_id_(id),mapped_(false) {
}

UnixMap::~UnixMap(void) {
}

void split_unixname(std::string& unixname,std::string& unixgroup) {
  std::string::size_type p = unixname.find(':');
  if(p != std::string::npos) {
    unixgroup=unixname.c_str()+p+1;
    unixname.resize(p);
  };
  if(unixname[0] == '*') unixname.resize(0);
  if(unixgroup[0] == '*') unixgroup.resize(0);
}

bool UnixMap::mapgroup(const char* line) {
  mapped_=false;
  if(!line) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  if(*line == 0) return false;
  const char* groupname = line;
  for(;*line;line++) if(isspace(*line)) break;
  int groupname_len = line-groupname;
  if(groupname_len == 0) return false;
  if(!user_.check_group(std::string(groupname,groupname_len))) return false;
  unix_user_.name.resize(0);
  unix_user_.group.resize(0);
  for(;*line;line++) if(!isspace(*line)) break;
  const char* command = line;
  for(;*line;line++) if(isspace(*line)) break;
  size_t command_len = line-command;
  if(command_len == 0) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  for(source_t* s = sources;s->cmd;s++) {
    if((strncmp(s->cmd,command,command_len) == 0) &&
       (strlen(s->cmd) == command_len)) {
      bool res=(this->*(s->map))(user_,unix_user_,line);
      if(res) {
        mapped_=true;
        return true;
      };
    };
  };
  return false;
}

bool UnixMap::mapvo(const char* line) {
  mapped_=false;
  if(!line) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  if(*line == 0) return false;
  const char* voname = line;
  for(;*line;line++) if(isspace(*line)) break;
  int voname_len = line-voname;
  if(voname_len == 0) return false;
  if(!user_.check_vo(std::string(voname,voname_len))) return false;
  unix_user_.name.resize(0);
  unix_user_.group.resize(0);
  for(;*line;line++) if(!isspace(*line)) break;
  const char* command = line;
  for(;*line;line++) if(isspace(*line)) break;
  size_t command_len = line-command;
  if(command_len == 0) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  for(source_t* s = sources;s->cmd;s++) {
    if((strncmp(s->cmd,command,command_len) == 0) &&
       (strlen(s->cmd) == command_len)) {
      bool res=(this->*(s->map))(user_,unix_user_,line);
      if(res) {
        mapped_=true;
        return true;
      };
    };
  };
  return false;
}

bool UnixMap::mapname(const char* line) {
  mapped_=false;
  if(!line) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  if(*line == 0) return false;
  const char* unixname = line;
  for(;*line;line++) if(isspace(*line)) break;
  int unixname_len = line-unixname;
  if(unixname_len == 0) return false;
  unix_user_.name.assign(unixname,unixname_len);
  split_unixname(unix_user_.name,unix_user_.group);
  for(;*line;line++) if(!isspace(*line)) break;
  const char* command = line;
  for(;*line;line++) if(isspace(*line)) break;
  size_t command_len = line-command;
  if(command_len == 0) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  for(source_t* s = sources;s->cmd;s++) {
    if((strncmp(s->cmd,command,command_len) == 0) &&
       (strlen(s->cmd) == command_len)) {
      bool res=(this->*(s->map))(user_,unix_user_,line);
      if(res) {
        mapped_=true;
        return true;
      };
    };
  };
  if(unix_user_.name.length() != 0) {
    // Try authorization rules if username is predefined
    int decision = user_.evaluate(command);
    if(decision == AAA_POSITIVE_MATCH) {
      mapped_=true;
      return true;
    };
  };
  return false;
}

bool UnixMap::unmap(void) const {
  if(!mapped_) return true;
  // Not functioning yet
  return true;
}

// -----------------------------------------------------------

static void subst_arg(std::string& str,void* arg) {
  AuthUser* it = (AuthUser*)arg;
  if(!it) return;
  AuthUserSubst(str,*it);
}

bool UnixMap::map_mapplugin(const AuthUser& /* user */ ,unix_user_t& unix_user,const char* line) {
  // timeout path arg ...
  if(!line) return false;
  for(;*line;line++) if(!isspace(*line)) break;
  if(*line == 0) return false;
  char* p;
  long int to = strtol(line,&p,0);
  if(p == line) return false;
  if(to < 0) return false;
  line=p;
  for(;*line;line++) if(!isspace(*line)) break;
  if(*line == 0) return false;
  std::string s = line;
  gridftpd::RunPlugin run(line);
  run.timeout(to);
  if(run.run(subst_arg,&user_)) {
    if(run.result() == 0) {
      if(run.stdout_channel().length() <= 512) { // sane name
        // Plugin should print user[:group] at stdout
        unix_user.name = run.stdout_channel();
        split_unixname(unix_user.name,unix_user.group);
        return true;
      } else {
        logger.msg(Arc::ERROR,"Plugin %s returned too much: %s",run.cmd(),run.stdout_channel());
      };
    } else {
      logger.msg(Arc::ERROR,"Plugin %s returned: %u",run.cmd(),(unsigned int)run.result());
    };
  } else {
    logger.msg(Arc::ERROR,"Plugin %s failed to run",run.cmd());
  };
  logger.msg(Arc::INFO,"Plugin %s printed: %u",run.cmd(),run.stdout_channel());
  logger.msg(Arc::ERROR,"Plugin %s error: %u",run.cmd(),run.stderr_channel());
  return false;
}

bool UnixMap::map_mapfile(const AuthUser& user,unix_user_t& unix_user,const char* line) {
  // This is just grid-mapfile
  std::ifstream f(line);
  if(user.DN()[0] == 0) return false;
  if(!f.is_open() ) {
    logger.msg(Arc::ERROR, "Mapfile at %s can't be opened.", line);
    return false;
  };
  for(;!f.eof();) {
    std::string buf; //char buf[512]; // must be enough for DN + name
    getline(f,buf);
    char* p = &buf[0];
    for(;*p;p++) if(((*p) != ' ') && ((*p) != '\t')) break;
    if((*p) == '#') continue;
    if((*p) == 0) continue;
    std::string val;
    int n = gridftpd::input_escaped_string(p,val);
    if(strcmp(val.c_str(),user.DN()) != 0) continue;
    p+=n;
    gridftpd::input_escaped_string(p,unix_user.name);
    f.close();
    return true;
  };
  f.close();
  return false;
}

bool UnixMap::map_simplepool(const AuthUser& user,unix_user_t& unix_user,const char* line) {
  if(user.DN()[0] == 0) return false;
  SimpleMap pool(line);
  if(!pool) {
    logger.msg(Arc::ERROR, "User pool at %s can't be opened.", line);
    return false;
  };
  unix_user.name=pool.map(user.DN());
  if(unix_user.name.empty()) return false;
  split_unixname(unix_user.name,unix_user.group);
  return true;
}

bool UnixMap::map_unixuser(const AuthUser& /* user */,unix_user_t& unix_user,const char* line) {
  // Maping is always positive - just fill specified username
  std::string unixname(line);
  std::string unixgroup;
  std::string::size_type p = unixname.find(':');
  if(p != std::string::npos) {
    unixgroup=unixname.c_str()+p+1;
    unixname.resize(p);
  };
  if(unixname.empty()) return false;
  unix_user.name=unixname;
  unix_user.group=unixgroup;
  return true;
}

