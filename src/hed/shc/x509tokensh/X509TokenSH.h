#ifndef __ARC_SEC_X509TOKENSH_H__
#define __ARC_SEC_X509TOKENSH_H__

#include <stdlib.h>

#include <arc/ArcConfig.h>
#include <arc/message/Message.h>
#include <arc/message/SecHandler.h>

namespace ArcSec {

/// Adds WS-Security X509 Token into SOAP Header

class X509TokenSH : public SecHandler {
 private:
  enum {
    process_none,
    process_extract,
    process_generate
  } process_type_;
  std::string cert_file_;
  std::string key_file_;
  std::string ca_file_;
  std::string ca_dir_;
  bool valid_;

 public:
  X509TokenSH(Arc::Config *cfg, Arc::ChainContext* ctx, Arc::PluginArgument* parg);
  virtual ~X509TokenSH(void);
  static Arc::Plugin* get_sechandler(Arc::PluginArgument* arg);
  virtual SecHandlerStatus Handle(Arc::Message* msg) const;
  operator bool(void) { return valid_; };
  bool operator!(void) { return !valid_; };
};

} // namespace ArcSec

#endif /* __ARC_SEC_X509TOKENSH_H__ */

