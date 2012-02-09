#ifndef __ARC_SEC_DELEGATIONCOLLECTOR_H__
#define __ARC_SEC_DELEGATIONCOLLECTOR_H__

#include <stdlib.h>

#include <arc/message/SecHandler.h>
#include <arc/ArcConfig.h>
#include <arc/loader/Plugin.h>

namespace ArcMCCTLSSec {

using namespace ArcSec;

class DelegationCollector : public SecHandler {
 public:
  DelegationCollector(Arc::Config *cfg);
  virtual ~DelegationCollector(void);
  virtual bool Handle(Arc::Message* msg) const;
  static Arc::Plugin* get_sechandler(Arc::PluginArgument* arg);
};

} // namespace ArcSec

#endif /* __ARC_SEC_DELEGATIONCOLLECTOR_H__ */

