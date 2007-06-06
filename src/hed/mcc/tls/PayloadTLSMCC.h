#ifndef __ARC_PAYLOADTLSMCC_H__
#define __ARC_PAYLOADTLSMCC_H__

#include <vector>
#include <openssl/ssl.h>

#include "PayloadTLSStream.h"
#include "../../libs/message/PayloadStream.h"
#include "../../libs/message/MCC.h"
#include "BIOMCC.h"

namespace Arc {
// This class extends PayloadTLSStream with initialization procedure to 
// connect it to next MCC. So far it woks only for client side.
class PayloadTLSMCC: public PayloadTLSStream {
 private:
  bool master_;
  SSL_CTX* sslctx_;
 public:
  /** Constructor - creat ssl object which is bound to next MCC */
  PayloadTLSMCC(MCCInterface* mcc, SSL_CTX* ctx);
  PayloadTLSMCC(PayloadTLSMCC& stream);
  virtual ~PayloadTLSMCC(void);
};

} // namespace Arc 

#endif /* __ARC_PAYLOADTLSMCC_H__ */

