#ifndef __ARC_SEC_ARCPDP_H__
#define __ARC_SEC_ARCPDP_H__

#include <stdlib.h>

//#include <arc/loader/ClassLoader.h>
#include <arc/ArcConfig.h>
#include <arc/security/ArcPDP/Evaluator.h>
#include <arc/security/PDP.h>

namespace ArcSec {

///ArcPDP - PDP which can handle the Arc specific request and policy schema
class ArcPDP : public PDP {
 public:
  static Arc::Plugin* get_arc_pdp(Arc::PluginArgument* arg);
  ArcPDP(Arc::Config* cfg, Arc::PluginArgument* parg);
  virtual ~ArcPDP();

  /***/
  virtual PDPStatus isPermitted(Arc::Message *msg) const;
 private:
  // Evaluator *eval;
  // Arc::ClassLoader* classloader;
  std::list<std::string> select_attrs;
  std::list<std::string> reject_attrs;
  std::list<std::string> policy_locations;
  Arc::XMLNodeContainer policies;
  std::string policy_combining_alg;
 protected:
  static Arc::Logger logger;
};

} // namespace ArcSec

#endif /* __ARC_SEC_ARCPDP_H__ */

