#ifndef __ARC_SEC_ARCAUTHZ_H__
#define __ARC_SEC_ARCAUTHZ_H__

#include <stdlib.h>

#include <arc/ArcConfig.h>
#include <arc/message/Message.h>
#include <arc/message/SecHandler.h>
#include <arc/security/PDP.h>
#include <arc/loader/Plugin.h>

namespace ArcSec {

/// Tests message against list of PDPs
/** This class implements SecHandler interface. It's Handle() method runs provided 
  Message instance against all PDPs specified in configuration. If any of PDPs 
  returns positive result Handle() return true, otherwise false. 
  This class is the main entry for configuring authorization, and could include 
  different PDP configured inside.
*/


class ArcAuthZ : public SecHandler {
 private:
  class PDPDesc {
   public:
    PDP* pdp;
    enum {
      breakOnAllow,
      breakOnDeny,
      breakAlways,
      breakNever
    } action;
    std::string id;
    PDPDesc(const std::string& action,const std::string& id,PDP* pdp);
  };
  typedef std::list<PDPDesc> pdp_container_t;

  /** Link to Factory responsible for loading and creation of PDP objects */
  Arc::PluginsFactory *pdp_factory;
  /** One Handler can include few PDP */
  pdp_container_t pdps_;
  bool valid_;

 protected:
  /** Create PDP according to conf info */
  bool MakePDPs(Arc::XMLNode cfg);

 public:
  ArcAuthZ(Arc::Config *cfg, Arc::ChainContext* ctx, Arc::PluginArgument* parg);
  virtual ~ArcAuthZ(void);
  static Plugin* get_sechandler(Arc::PluginArgument* arg);  
  /** Get authorization decision*/
  virtual bool Handle(Arc::Message* msg) const;
  operator bool(void) { return valid_; };
  bool operator!(void) { return !valid_; };
};

} // namespace ArcSec

#endif /* __ARC_SEC_ARCAUTHZ_H__ */

