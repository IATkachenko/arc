#ifndef __ARC_SEC_IDENTITYMAP_H__
#define __ARC_SEC_IDENTITYMAP_H__

#include <arc/ArcConfig.h>
#include <arc/message/Message.h>
#include <arc/message/SecHandler.h>
#include <arc/security/PDP.h>

namespace ArcSec {

class LocalMap {
 public:
  LocalMap(void) {};
  virtual ~LocalMap(void) {};
  virtual std::string ID(Arc::Message* msg) = 0;
};

/// Apply Tests message against list of PDPs
/** This class implements SecHandler interface. It's Handle() method runs provided 
  Message instance against all PDPs specified in configuration. If any of PDPs 
  returns positive result Handle() return true, otherwise false. */
class IdentityMap : public SecHandler {
 private:

  typedef struct {
    PDP* pdp;
    LocalMap* uid;
  } map_pair_t;

  std::list<map_pair_t> maps_;
  bool valid_;

 public:
  IdentityMap(Arc::Config *cfg, Arc::ChainContext* ctx, Arc::PluginArgument* parg);
  virtual ~IdentityMap(void);
  virtual SecHandlerStatus Handle(Arc::Message* msg) const;
  operator bool(void) { return valid_; };
  bool operator!(void) { return !valid_; };
};

} // namespace ArcSec

#endif /* __ARC_SEC_IDENTITYMAP_H__ */

