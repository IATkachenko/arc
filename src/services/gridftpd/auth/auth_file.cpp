#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>
#include <iostream>

#include <arc/Logger.h>
#include <arc/ArcConfigIni.h>
#include <arc/StringConv.h>

#include "auth.h"

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AuthUserFile");

AuthResult AuthUser::match_file(const char* line) {
  std::string s = Arc::trim(line);
  if(!s.empty()) {
    std::ifstream f(s.c_str());
    if(!f.is_open()) {
      logger.msg(Arc::ERROR, "Failed to read file %s", s);
      return AAA_FAILURE;
    };
    for(;f.good();) {
      std::string buf;
      getline(f,buf);
      buf = Arc::trim(buf);
      if(buf.empty()) continue;
      AuthResult res = match_subject(buf.c_str());
      if(res != AAA_NO_MATCH) {
        f.close();
        return res;
      };
    };
    f.close();
  };
  return AAA_NO_MATCH;
}
