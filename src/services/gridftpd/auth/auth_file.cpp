#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <fstream>
#include <iostream>

#include <arc/Logger.h>

#include "../misc/escaped.h"
#include "auth.h"

static Arc::Logger logger(Arc::Logger::getRootLogger(),"AuthUserFile");

AuthResult AuthUser::match_file(const char* line) {
  for(;;) {
    std::string s("");
    int n = gridftpd::input_escaped_string(line,s,' ','"');
    if(n == 0) break;
    line+=n;
    std::ifstream f(s.c_str());
    if(!f.is_open()) {
      logger.msg(Arc::ERROR, "Failed to read file %s", s);
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
