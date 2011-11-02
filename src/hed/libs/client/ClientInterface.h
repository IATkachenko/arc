// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_CLIENTINTERFACE_H__
#define __ARC_CLIENTINTERFACE_H__

#include <string>
#include <list>

#include <inttypes.h>

#include <arc/ArcConfig.h>
#include <arc/DateTime.h>
#include <arc/URL.h>
#include <arc/message/Message.h>
#include <arc/message/MCC_Status.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/PayloadStream.h>
#include <arc/message/PayloadSOAP.h>

namespace Arc {

  class MCCLoader;
  class Logger;
  class MCC;
   
  //! Utility base class for MCC
  /** The ClientInterface class is a utility base class used for
   *  configuring a client side Message Chain Component (MCC) chain
   *  and loading it into memory. It has several specializations of
   *  increasing complexity of the MCC chains.
   **/
  class ClientInterface {
  public:
    ClientInterface()
      : loader(NULL) {}
    ClientInterface(const BaseConfig& cfg);
    virtual ~ClientInterface();
    void Overlay(XMLNode cfg);
    const Config& GetConfig() const {
      return xmlcfg;
    }
    MessageContext& GetContext() {
      return context;
    }
    virtual bool Load();
  protected:
    Config xmlcfg;
    XMLNode overlay;
    MCCLoader *loader;
    MessageContext context;
    static Logger logger;
    static void AddSecHandler(XMLNode mcccfg, XMLNode handlercfg);
    static void AddPlugin(XMLNode mcccfg, const std::string& libname, const std::string& libpath = "");
  };

  enum SecurityLayer {
    NoSec, TLSSec, GSISec, SSL3Sec, GSIIOSec
  };

  //! Class for setting up a MCC chain for TCP communication
  /** The ClientTCP class is a specialization of the ClientInterface
   * which sets up a client MCC chain for TCP communication, and
   * optionally with a security layer on top which can be either TLS,
   * GSI or SSL3.
   **/
  class ClientTCP
    : public ClientInterface {
  public:
    ClientTCP()
      : tcp_entry(NULL),
        tls_entry(NULL) {}
    ClientTCP(const BaseConfig& cfg, const std::string& host, int port,
              SecurityLayer sec, int timeout = -1, bool no_delay = false);
    virtual ~ClientTCP();
    MCC_Status process(PayloadRawInterface *request,
                       PayloadStreamInterface **response, bool tls);
    MCC* GetEntry() {
      return tls_entry ? tls_entry : tcp_entry;
    }
    virtual bool Load();
    void AddSecHandler(XMLNode handlercfg, SecurityLayer sec, const std::string& libanme = "", const std::string& libpath = "");
  protected:
    MCC *tcp_entry;
    MCC *tls_entry;
  };

  struct HTTPClientInfo {
    int code; /// HTTP response code
    std::string reason; /// HTTP response reason
    uint64_t size; /// Size of body (content-length)
    Time lastModified; /// Reported modification time
    std::string type; /// Content-type
    std::list<std::string> cookies; /// All collected cookies
    std::string location; /// Value of location attribute in HTTP response
  };

  //! Class for setting up a MCC chain for HTTP communication
  /** The ClientHTTP class inherits from the ClientTCP class and adds
   * an HTTP MCC to the chain.
   **/
  class ClientHTTP
    : public ClientTCP {
  public:
    ClientHTTP()
      : http_entry(NULL), relative_uri(false), sec(NoSec) {}
    ClientHTTP(const BaseConfig& cfg, const URL& url, int timeout = -1, const std::string& proxy_host = "", int proxy_port = 0);
    virtual ~ClientHTTP();
    MCC_Status process(const std::string& method, PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC_Status process(const std::string& method,
                       std::multimap<std::string, std::string>& attributes,
                       PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC_Status process(const std::string& method, const std::string& path,
                       PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC_Status process(const std::string& method, const std::string& path,
                       std::multimap<std::string, std::string>& attributes,
                       PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC_Status process(const std::string& method, const std::string& path,
                       uint64_t range_start, uint64_t range_end,
                       PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC_Status process(const std::string& method, const std::string& path,
                       std::multimap<std::string, std::string>& attributes,
                       uint64_t range_start, uint64_t range_end,
                       PayloadRawInterface *request,
                       HTTPClientInfo *info, PayloadRawInterface **response);
    MCC* GetEntry() {
      return http_entry;
    }
    void AddSecHandler(XMLNode handlercfg, const std::string& libanme = "", const std::string& libpath = "");
    virtual bool Load();
    void RelativeURI(bool val) { relative_uri=val; };
  protected:
    MCC *http_entry;
    URL default_url;
    bool relative_uri;
    SecurityLayer sec;
  };

  /** Class with easy interface for sending/receiving SOAP messages
      over HTTP(S/G).
      It takes care of configuring MCC chain and making an entry point. */
  class ClientSOAP
    : public ClientHTTP {
  public:
    /** Constructor creates MCC chain and connects to server. */
    ClientSOAP()
      : soap_entry(NULL) {}
    ClientSOAP(const BaseConfig& cfg, const URL& url, int timeout = -1);
    virtual ~ClientSOAP();
    /** Send SOAP request and receive response. */
    MCC_Status process(PayloadSOAP *request, PayloadSOAP **response);
    /** Send SOAP request with specified SOAP action and receive response. */
    MCC_Status process(const std::string& action, PayloadSOAP *request,
                       PayloadSOAP **response);
    /** Returns entry point to SOAP MCC in configured chain.
       To initialize entry point Load() method must be called. */
    MCC* GetEntry() {
      return soap_entry;
    }
    /** Adds security handler to configuration of SOAP MCC */
    void AddSecHandler(XMLNode handlercfg, const std::string& libanme = "", const std::string& libpath = "");
    /** Instantiates pluggable elements according to generated configuration */
    virtual bool Load();
  protected:
    MCC *soap_entry;
  };

  // Convenience base class for creating configuration
  // security handlers.
  class SecHandlerConfig
    : public XMLNode {
  public:
    SecHandlerConfig(const std::string& name, const std::string& event = "incoming");
  };

  class DNListHandlerConfig
    : public SecHandlerConfig {
  public:
    DNListHandlerConfig(const std::list<std::string>& dns, const std::string& event = "incoming");
    void AddDN(const std::string& dn);
  };

  class ARCPolicyHandlerConfig
    : public SecHandlerConfig {
  public:
    ARCPolicyHandlerConfig(const std::string& event = "incoming");
    ARCPolicyHandlerConfig(XMLNode policy, const std::string& event = "incoming");
    void AddPolicy(XMLNode policy);
    void AddPolicy(const std::string& policy);
  };

} // namespace Arc

#endif // __ARC_CLIENTINTERFACE_H__
