#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>

#include <arc/Logger.h>

#include "ConfigSections.h"
#include "ConfigUtils.h"

#include "UrlMapConfig.h"

namespace ARex {

static Arc::Logger& glogger = Arc::Logger::getRootLogger();

UrlMapConfig::UrlMapConfig(const GMConfig& config) {
  std::ifstream cfile;
  ConfigSections* cf = NULL;
  //if(nordugrid_config_loc().empty()) read_env_vars(true);
  if(!config_open(cfile,config.ConfigFile())) {
    glogger.msg(Arc::ERROR,"Can't open configuration file");
    return;
  };
  switch(config_detect(cfile)) {
    case config_file_XML: {
      Arc::XMLNode cfg;
      if(!cfg.ReadFromStream(cfile)) {
        glogger.msg(Arc::ERROR,"Can't interpret configuration file as XML");
      } else {
        /*
         dataTransfer
           mapURL (link)
            from
            to
            at
         */
        Arc::XMLNode datanode = cfg["dataTransfer"];
        if(datanode) {
          Arc::XMLNode mapnode = datanode["mapURL"];
          for(;mapnode;++mapnode) {
            bool is_link = false;
            if(!elementtobool(mapnode,"link",is_link,&glogger)) continue;
            std::string initial = mapnode["from"];
            std::string replacement = mapnode["to"];
            if(initial.empty()) {
              glogger.msg(Arc::ERROR,"Missing 'from' element in mapURL");
              continue;
            };
            if(replacement.empty()) {
              glogger.msg(Arc::ERROR,"Missing 'to' element in mapURL");
              continue;
            };
            if(is_link) {
              std::string access = mapnode["at"];
              if(access.empty()) access = replacement;
              add(initial,replacement,access);
            } else {
              add(initial,replacement);
            };
          };
        };
      };
    }; break;
    case config_file_INI: {
      cf=new ConfigSections(cfile);
      cf->AddSection("common");
      cf->AddSection("grid-manager");
      for(;;) {
        std::string rest;
        std::string command;
        cf->ReadNext(command,rest);
        if(command.length() == 0) break;
        else if(command == "copyurl") {
          std::string initial = config_next_arg(rest);
          std::string replacement = config_next_arg(rest);
          if((initial.length() == 0) || (replacement.length() == 0)) {
            glogger.msg(Arc::ERROR,"Not enough parameters in copyurl");
            continue;
          };
          add(initial,replacement);
        }
        else if(command == "linkurl") {
          std::string initial = config_next_arg(rest);
          std::string replacement = config_next_arg(rest);
          if((initial.length() == 0) || (replacement.length() == 0)) {
            glogger.msg(Arc::ERROR,"Not enough parameters in linkurl");
            continue;
          };
          std::string access = config_next_arg(rest);
          if(access.length() == 0) access = replacement;
          add(initial,replacement,access);
        };
      };
    }; break;
    default: {
      glogger.msg(Arc::ERROR,"Can't recognize type of configuration file");
    }; break;
  };
  config_close(cfile);
  if(cf) delete cf;
}

UrlMapConfig::~UrlMapConfig(void) {
}

} // namespace ARex