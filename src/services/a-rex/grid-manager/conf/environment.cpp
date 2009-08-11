#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arc/ArcLocation.h>
#include <arc/Utils.h>
#define olog std::cerr
#include "environment.h"

// Globus installation path - $GLOBUS_LOCATION
std::string globus_loc(""); 
// Various Globus scripts - $GLOBUS_LOCATION/libexec
std::string globus_scripts_loc;
// ARC installation path - $ARC_LOCATION, executable path
std::string nordugrid_loc("");
// ARC system tools - $ARC_LOCATION/libexec/arc
std::string nordugrid_libexec_loc;
// ARC libraries and plugins - $ARC_LOCATION/lib
std::string nordugrid_lib_loc;
// ARC configuration file
std::string nordugrid_config_loc("");
// Email address of person responsible for this ARC installation
std::string support_mail_address;
// Global gridmap files with welcomed users' DNs and UNIX names
std::string globus_gridmap;

static bool file_exists(const char* name) {
  struct stat st;
  if(lstat(name,&st) != 0) return false;
  if(!S_ISREG(st.st_mode)) return false;
  return true;
}

static bool dir_exists(const char* name) {
  struct stat st;
  if(lstat(name,&st) != 0) return false;
  if(!S_ISDIR(st.st_mode)) return false;
  return true;
}

bool read_env_vars(bool guess) {
  if(globus_loc.length() == 0) {
    globus_loc=Arc::GetEnv("GLOBUS_LOCATION");
    if(globus_loc.empty()) {
      if(!guess) {
        olog<<"Warning: GLOBUS_LOCATION environment variable not defined"<<std::endl;
        //return false;
      }
      else {
        globus_loc="/opt/globus";
      };
    };
    Arc::SetEnv("GLOBUS_LOCATION",globus_loc);
  };
  globus_scripts_loc=globus_loc+"/libexec";

  if(nordugrid_loc.empty()) {
    nordugrid_loc=Arc::GetEnv("ARC_LOCATION");
    if(nordugrid_loc.empty()) {
      nordugrid_loc=Arc::ArcLocation::Get();
    };
    nordugrid_lib_loc=nordugrid_loc+"/"+PKGLIBSUBDIR;
    nordugrid_libexec_loc=nordugrid_loc+"/"+PKGLIBEXECSUBDIR;
  };

  if(nordugrid_config_loc.empty()) {
    std::string tmp = Arc::GetEnv("ARC_CONFIG");
    if(tmp.empty()) {
      tmp=Arc::GetEnv("NORDUGRID_CONFIG");
      if(tmp.empty()) {
        nordugrid_config_loc="/etc/arc.conf";
        if(!file_exists(nordugrid_config_loc.c_str())) {
          olog<<"Central configuration file is missing at guessed location:\n"
              <<"  /etc/arc.conf\n"
              <<"Use ARC_CONFIG variable for non-standard location"
              <<std::endl;
          return false;
        };
      };
    };
    if(!tmp.empty()) nordugrid_config_loc=tmp;
  };
  // Set all environement variables for other tools
  Arc::SetEnv("ARC_CONFIG",nordugrid_config_loc);
  if(support_mail_address.length() == 0) {
    char hn[100];
    support_mail_address="grid.manager@";
    if(gethostname(hn,99) == 0) {
      support_mail_address+=hn;
    }
    else {
      support_mail_address+="localhost";
    };
  };
  std::string tmp=Arc::GetEnv("GRIDMAP");
  if(tmp.empty()) { globus_gridmap="/etc/grid-security/grid-mapfile"; }
  else { globus_gridmap=tmp; };
  return true;
}

