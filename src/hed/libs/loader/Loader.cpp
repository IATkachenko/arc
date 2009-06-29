#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/Logger.h>
#include <arc/StringConv.h>

#include "Loader.h"

namespace Arc {

  Logger Loader::logger(Logger::rootLogger, "Loader");

  Loader::~Loader(void) {
    if(factory_) delete factory_;
  }

  Loader::Loader(const Config& cfg) {
    factory_    = new PluginsFactory(cfg);
    for(int n = 0;; ++n) {
      XMLNode cn = cfg.Child(n);
      if(!cn) break;
      Config cfg_(cn, cfg.getFileName());

      if(MatchXMLName(cn, "ModuleManager")) {
        continue;
      }

      if(MatchXMLName(cn, "Plugins")) {
        std::string name = cn["Name"];
        if(name.empty()) {
          logger.msg(ERROR, "Plugins element has no Name defined");
          continue;
        }
        factory_->load(name);
        continue;
      }

      // Configuration processing is split to multiple functions - hence
      // ignoring all unknown elements.
      //logger.msg(WARNING, "Unknown element \"%s\" - ignoring", cn.Name());
    }

  }

} // namespace Arc
