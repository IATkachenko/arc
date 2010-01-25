#ifndef __ARC_SEC_USERNAMETOKENSH_H__
#define __ARC_SEC_USERNAMETOKENSH_H__

#include <stdlib.h>

#include <arc/ArcConfig.h>
#include <arc/message/Message.h>
#include <arc/message/SecHandler.h>

namespace ArcSec {

/// Adds WS-Security Username Token into SOAP Header

class UsernameTokenSH : public SecHandler {
 private:
  enum {
    process_none,
    process_extract,
    process_generate
  } process_type_;
  enum {
    password_text,
    password_digest
  } password_type_;
  std::string username_;
  std::string password_;
  std::string password_source_;

 public:
  UsernameTokenSH(Arc::Config *cfg, Arc::ChainContext* ctx);
  virtual ~UsernameTokenSH(void);
  static Arc::Plugin* get_sechandler(Arc::PluginArgument* arg);
  virtual bool Handle(Arc::Message* msg) const;
};

} // namespace ArcSec

#endif /* __ARC_SEC_USERNAMETOKENSH_H__ */

