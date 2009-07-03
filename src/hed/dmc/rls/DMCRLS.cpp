// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32 
#include <arc/win32.h>
#include <fcntl.h>
#endif

extern "C" {
#include "globus_rls_client.h"
}

#include <arc/Logger.h>
#include <arc/URL.h>
#include <arc/data/DMCLoader.h>
#include <arc/globusutils/GlobusWorkarounds.h>

#include "DataPointRLS.h"
#include "DMCRLS.h"

namespace Arc {

  static bool proxy_initialized = false;

  Logger DMCRLS::logger(DMC::logger, "RLS");

  DMCRLS::DMCRLS(Config *cfg)
    : DMC(cfg) {
    globus_module_activate(GLOBUS_COMMON_MODULE);
    globus_module_activate(GLOBUS_IO_MODULE);
    globus_module_activate(GLOBUS_RLS_CLIENT_MODULE);
    if (!proxy_initialized)
      proxy_initialized = GlobusRecoverProxyOpenSSL();
    Register(this);
  }

  DMCRLS::~DMCRLS() {
    Unregister(this);
    globus_module_deactivate(GLOBUS_RLS_CLIENT_MODULE);
    globus_module_deactivate(GLOBUS_IO_MODULE);
    globus_module_deactivate(GLOBUS_COMMON_MODULE);
  }

  Plugin* DMCRLS::Instance(PluginArgument *arg) {
    DMCPluginArgument *dmcarg =
      arg ? dynamic_cast<DMCPluginArgument*>(arg) : NULL;
    if (!dmcarg)
      return NULL;
    // Make this code non-unloadable because Globus
    // may have problems with unloading
    Glib::Module* module = dmcarg->get_module();
    PluginsFactory* factory = dmcarg->get_factory();
    if(factory && module) factory->makePersistent(module);
    return new DMCRLS((Config*)(*dmcarg));
  }

  DataPoint* DMCRLS::iGetDataPoint(const URL& url) {
    if (url.Protocol() != "rls")
      return NULL;
    return new DataPointRLS(url);
  }

} // namespace Arc

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
  { "rls", "HED:DMC", 0, &Arc::DMCRLS::Instance },
  { NULL, NULL, 0, NULL }
};
