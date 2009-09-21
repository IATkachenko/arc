// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_DATABROKER_H__
#define __ARC_DATABROKER_H__

#include <map>
#include <arc/client/Broker.h>

namespace Arc {

  class DataBroker
    : public Broker {

  public:
    DataBroker(const UserConfig& usercfg);
    ~DataBroker();
    static Plugin* Instance(PluginArgument *arg);
    bool CacheCheck(void);

  protected:
    void SortTargets();
  };

} // namespace Arc

#endif // __ARC_DATABROKER_H__
