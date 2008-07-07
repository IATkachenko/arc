#ifndef __ARC_X509TOKEN_H__
#define __ARC_X509TOKEN_H__

#include <arc/XMLNode.h>
#include <arc/message/SOAPEnvelope.h>

// WS-Security X509 Token Profile v1.1
// wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd"

namespace Arc {

/// Interface for manipulation of WS-Security according to X.509 Token Profile. 
class X509Token : public SOAPEnvelope {
public:
  typedef enum {
    Signature,
    Encryption
  } X509TokenType;

  /** Link to existing SOAP header and parse X509 Token information.
    X509 Token related information is extracted from SOAP header and
    stored in class variables. */
  X509Token(SOAPEnvelope& soap, X509TokenType tokentype = Signature);

  /** Add X509 Token information into the SOAP header.
     Generated token contains elements X509 token and signature, and is
    meant to be used for authentication.
    @param soap  the SOAP message
    @param certfile
    @param keyfile
    @param tokentype
  */
  X509Token(SOAPEnvelope& soap, const std::string& certfile, const std::string& keyfile, X509TokenType tokentype = Signature);

  ~X509Token(void);

  /** Returns true of constructor succeeded */
  operator bool(void);

  /**Check signature by using the trusted certificates
    @param cafile ca file
    @param capath ca directory
  */
  bool Authenticate(const std::string& cafile, const std::string& capath);

  /** Check signature by using the cert information in soap message
  */
  bool Authenticate(void);

private:
  /** Tells if specified SOAP header has WSSE element and X509Token inside the WSSE element */
  static bool Check(SOAPEnvelope& soap);

private:
  xmlNodePtr signature_nd;
  std::string key_str;
};

} // namespace Arc

#endif /* __ARC_X509TOKEN_H__ */

