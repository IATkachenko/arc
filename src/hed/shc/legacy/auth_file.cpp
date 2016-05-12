#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>
#include <iostream>

#include <arc/StringConv.h>
#include <arc/Logger.h>

#include "auth.h"

namespace ArcSHCLegacy {

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AuthUser");

AuthResult AuthUser::match_file(const char* line) {
  std::list<std::string> tokens;
  Arc::tokenize(line, tokens, " ", "\"", "\"");
  for(std::list<std::string>::iterator s = tokens.begin();s!=tokens.end();++s) {
    std::ifstream f(s->c_str());
    if(!f.is_open()) {
      logger.msg(Arc::ERROR, "Failed to read file %s", *s);
      return AAA_FAILURE;
    };
    for(;f.good();) {
      std::string buf;
      getline(f,buf);
      AuthResult res = evaluate(buf.c_str());
      if(res != AAA_NO_MATCH) {
        f.close();
        return res;
      };
    };
    f.close();
  };
  return AAA_NO_MATCH;
}

} // namespace ArcSHCLegacy

