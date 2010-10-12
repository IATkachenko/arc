#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/XMLNode.h>
#include <arc/StringConv.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/SecAttr.h>
#include <arc/loader/Plugin.h>

#include "PayloadHTTP.h"
#include "MCCHTTP.h"



Arc::Logger Arc::MCC_HTTP::logger(Arc::MCC::logger,"HTTP");

Arc::MCC_HTTP::MCC_HTTP(Arc::Config *cfg) : Arc::MCC(cfg) {
}

static Arc::Plugin* get_mcc_service(Arc::PluginArgument* arg) {
    Arc::MCCPluginArgument* mccarg =
            arg?dynamic_cast<Arc::MCCPluginArgument*>(arg):NULL;
    if(!mccarg) return NULL;
    return new Arc::MCC_HTTP_Service((Arc::Config*)(*mccarg));
}

static Arc::Plugin* get_mcc_client(Arc::PluginArgument* arg) {
    Arc::MCCPluginArgument* mccarg =
            arg?dynamic_cast<Arc::MCCPluginArgument*>(arg):NULL;
    if(!mccarg) return NULL;
    return new Arc::MCC_HTTP_Client((Arc::Config*)(*mccarg));
}

Arc::PluginDescriptor PLUGINS_TABLE_NAME[] = {
    { "http.service", "HED:MCC", 0, &get_mcc_service },
    { "http.client",  "HED:MCC", 0, &get_mcc_client  },
    { NULL, NULL, 0, NULL }
};

namespace Arc {

class HTTPSecAttr: public SecAttr {
 friend class MCC_HTTP_Service;
 friend class MCC_HTTP_Client;
 public:
  HTTPSecAttr(PayloadHTTP& payload);
  virtual ~HTTPSecAttr(void);
  virtual operator bool(void) const;
  virtual bool Export(SecAttrFormat format,XMLNode &val) const;
 protected:
  std::string action_;
  std::string object_;
  virtual bool equal(const SecAttr &b) const;
};

HTTPSecAttr::HTTPSecAttr(PayloadHTTP& payload) {
  action_=payload.Method();
  std::string path = payload.Endpoint();
  // Remove service, port and protocol - those will be provided by
  // another layer
  std::string::size_type p = path.find("://");
  if(p != std::string::npos) {
    p=path.find('/',p+3);
    if(p != std::string::npos) {
      path.erase(0,p);
    };
  };
  object_=path;
}

HTTPSecAttr::~HTTPSecAttr(void) {
}

HTTPSecAttr::operator bool(void) const {
  return true;
}

bool HTTPSecAttr::equal(const SecAttr &b) const {
  try {
    const HTTPSecAttr& a = (const HTTPSecAttr&)b;
    return ((action_ == a.action_) && (object_ == a.object_));
  } catch(std::exception&) { };
  return false;
}

bool HTTPSecAttr::Export(SecAttrFormat format,XMLNode &val) const {
  if(format == UNDEFINED) {
  } else if(format == ARCAuth) {
    NS ns;
    ns["ra"]="http://www.nordugrid.org/schemas/request-arc";
    val.Namespaces(ns); val.Name("ra:Request");
    XMLNode item = val.NewChild("ra:RequestItem");
    if(!object_.empty()) {
      XMLNode object = item.NewChild("ra:Resource");
      object=object_;
      object.NewAttribute("Type")="string";
      object.NewAttribute("AttributeId")="http://www.nordugrid.org/schemas/policy-arc/types/http/path";
    };
    if(!action_.empty()) {
      XMLNode action = item.NewChild("ra:Action");
      action=action_;
      action.NewAttribute("Type")="string";
      action.NewAttribute("AttributeId")="http://www.nordugrid.org/schemas/policy-arc/types/http/method";
    };
    return true;
  } else if(format == XACML) {
    NS ns;
    ns["ra"]="urn:oasis:names:tc:xacml:2.0:context:schema:os";
    val.Namespaces(ns); val.Name("ra:Request");
    if(!object_.empty()) {
      XMLNode object = val.NewChild("ra:Resource");
      XMLNode attr = object.NewChild("ra:Attribute");
      attr.NewChild("ra:AttributeValue") = object_;
      attr.NewAttribute("DataType")="xs:string";
      attr.NewAttribute("AttributeId")="http://www.nordugrid.org/schemas/policy-arc/types/http/path";
    };
    if(!action_.empty()) {
      XMLNode action = val.NewChild("ra:Action");
      XMLNode attr = action.NewChild("ra:Attribute");
      attr.NewChild("ra:AttributeValue") = action_;
      attr.NewAttribute("DataType")="xs:string";
      attr.NewAttribute("AttributeId")="http://www.nordugrid.org/schemas/policy-arc/types/http/method";
    };
    return true;
  } else {
  };
  return false;
}

MCC_HTTP_Service::MCC_HTTP_Service(Config *cfg):MCC_HTTP(cfg) {
}

MCC_HTTP_Service::~MCC_HTTP_Service(void) {
}

static MCC_Status make_http_fault(Logger& logger,PayloadStreamInterface& stream,Message& outmsg,int code,const char* desc = NULL) {
  if((desc == NULL) || (*desc == 0)) {
    switch(code) {
      case HTTP_BAD_REQUEST:  desc="Bad Request"; break;
      case HTTP_NOT_FOUND:    desc="Not Found"; break;
      case HTTP_INTERNAL_ERR: desc="Internal error"; break;
      default: desc="Something went wrong";
    };
  };
  logger.msg(WARNING, "HTTP Error: %d %s",code,desc);
  PayloadHTTP outpayload(code,desc,stream);
  if(!outpayload.Flush()) return MCC_Status();
  // Returning empty payload because response is already sent
  PayloadRaw* outpayload_e = new PayloadRaw;
  outmsg.Payload(outpayload_e);
  return MCC_Status(STATUS_OK);
}

static MCC_Status make_raw_fault(Message& outmsg,const char* desc = NULL) {
  PayloadRaw* outpayload = new PayloadRaw;
  if(desc) outpayload->Insert(desc,0);
  outmsg.Payload(outpayload);
  return MCC_Status();
}

static void parse_http_range(PayloadHTTP& http,Message& msg) {
  std::string http_range = http.Attribute("range");
  if(http_range.empty()) return;
  if(strncasecmp(http_range.c_str(),"bytes=",6) != 0) return;
  std::string::size_type p = http_range.find(',',6);
  if(p != std::string::npos) {
    http_range=http_range.substr(6,p-6);
  } else {
    http_range=http_range.substr(6);
  };
  p=http_range.find('-');
  std::string val;
  if(p != std::string::npos) {
    val=http_range.substr(0,p);
    if(!val.empty()) msg.Attributes()->set("HTTP:RANGESTART",val);
    val=http_range.substr(p+1);
    if(!val.empty()) msg.Attributes()->set("HTTP:RANGEEND",val);
  };
}

MCC_Status MCC_HTTP_Service::process(Message& inmsg,Message& outmsg) {
  // Extracting payload
  if(!inmsg.Payload()) return MCC_Status();
  PayloadStreamInterface* inpayload = NULL;
  try {
    inpayload = dynamic_cast<PayloadStreamInterface*>(inmsg.Payload());
  } catch(std::exception& e) { };
  if(!inpayload) return MCC_Status();
  // Converting stream payload to HTTP which also implements raw interface
  PayloadHTTP nextpayload(*inpayload);
  if(!nextpayload) {
    logger.msg(WARNING, "Cannot create http payload");
    return make_http_fault(logger,*inpayload,outmsg,HTTP_BAD_REQUEST);
  };
  if(nextpayload.Method() == "END") {
    return MCC_Status(SESSION_CLOSE);
  };
  bool keep_alive = nextpayload.KeepAlive();
  // Creating message to pass to next MCC and setting new payload.
  Message nextinmsg = inmsg;
  nextinmsg.Payload(&nextpayload);
  // Creating attributes
  // Endpoints must be URL-like so make sure HTTP path is
  // converted to HTTP URL
  std::string endpoint = nextpayload.Endpoint();
  {
    std::string::size_type p = endpoint.find("://");
    if(p == std::string::npos) {
      // TODO: Use Host attribute of HTTP
      std::string oendpoint = nextinmsg.Attributes()->get("ENDPOINT");
      p=oendpoint.find("://");
      if(p != std::string::npos) {
        oendpoint.erase(0,p+3);
      };
      // Assuming we have host:port here
      if(oendpoint.empty() ||
         (oendpoint[oendpoint.length()-1] != '/')) {
        if(endpoint[0] != '/') oendpoint+="/";
      };
      // TODO: HTTPS detection
      endpoint="http://"+oendpoint+endpoint;
    };
  };
  nextinmsg.Attributes()->set("ENDPOINT",endpoint);
  nextinmsg.Attributes()->set("HTTP:ENDPOINT",nextpayload.Endpoint());
  nextinmsg.Attributes()->set("HTTP:METHOD",nextpayload.Method());
  // Filling security attributes
  HTTPSecAttr* sattr = new HTTPSecAttr(nextpayload);
  nextinmsg.Auth()->set("HTTP",sattr);
  parse_http_range(nextpayload,nextinmsg);
  // Reason ?
  for(std::multimap<std::string,std::string>::const_iterator i =
      nextpayload.Attributes().begin();i!=nextpayload.Attributes().end();++i) {
    nextinmsg.Attributes()->add("HTTP:"+i->first,i->second);
  };
  if(!ProcessSecHandlers(nextinmsg,"incoming")) {
    return make_http_fault(logger,*inpayload,outmsg,HTTP_BAD_REQUEST); // Maybe not 400 ?
  };
  // Call next MCC
  MCCInterface* next = Next(nextpayload.Method());
  if(!next) {
    logger.msg(WARNING, "No next element in the chain");
    return make_http_fault(logger,*inpayload,outmsg,HTTP_NOT_FOUND);
  }
  Message nextoutmsg = outmsg; nextoutmsg.Payload(NULL);
  MCC_Status ret = next->process(nextinmsg,nextoutmsg);
  // Do checks and extract raw response
  if(!ret) {
    if(nextoutmsg.Payload()) delete nextoutmsg.Payload();
    logger.msg(WARNING, "next element of the chain returned error status");
    if(ret.getKind() == UNKNOWN_SERVICE_ERROR) {
      return make_http_fault(logger,*inpayload,outmsg,HTTP_NOT_FOUND);
    } else {
      return make_http_fault(logger,*inpayload,outmsg,HTTP_INTERNAL_ERR);
    }
  }
  if(!nextoutmsg.Payload()) {
    logger.msg(WARNING, "next element of the chain returned empty payload");
    return make_http_fault(logger,*inpayload,outmsg,HTTP_INTERNAL_ERR);
  }
  PayloadRawInterface* retpayload = NULL;
  PayloadStreamInterface* strpayload = NULL;
  try {
    retpayload = dynamic_cast<PayloadRawInterface*>(nextoutmsg.Payload());
  } catch(std::exception& e) { };
  if(!retpayload) try {
    strpayload = dynamic_cast<PayloadStreamInterface*>(nextoutmsg.Payload());
  } catch(std::exception& e) { };
  if((!retpayload) && (!strpayload)) {
    logger.msg(WARNING, "next element of the chain returned invalid/unsupported payload");
    delete nextoutmsg.Payload();
    return make_http_fault(logger,*inpayload,outmsg,HTTP_INTERNAL_ERR);
  };
  if(!ProcessSecHandlers(nextinmsg,"outgoing")) {
    delete nextoutmsg.Payload(); return make_http_fault(logger,*inpayload,outmsg,HTTP_BAD_REQUEST); // Maybe not 400 ?
  };
  // Create HTTP response from raw body content
  // Use stream payload of inmsg to send HTTP response
  int http_code = HTTP_OK;
  const char* http_resp = "OK";
/*
  int l = 0;
  if(retpayload) {
    if(retpayload->BufferPos(0) != 0) {
      http_code=HTTP_PARTIAL;
      http_resp="Partial content";
    } else {
      for(int i = 0;;++i) {
        if(retpayload->Buffer(i) == NULL) break;
        l=retpayload->BufferPos(i) + retpayload->BufferSize(i);
      };
      if(l != retpayload->Size()) {
        http_code=HTTP_PARTIAL;
        http_resp="Partial content";
      };
    };
  } else {
    if((strpayload->Pos() != 0) || (strpayload->Limit() != strpayload->Size())) {
      http_code=HTTP_PARTIAL;
      http_resp="Partial content";
    };
  };
*/
  PayloadHTTP* outpayload = new PayloadHTTP(http_code,http_resp,*inpayload);
  // Use attributes which higher level MCC may have produced for HTTP
  for(AttributeIterator i = nextoutmsg.Attributes()->getAll();i.hasMore();++i) {
    const char* key = i.key().c_str();
    if(strncmp("HTTP:",key,5) == 0) {
      key+=5;
      // TODO: check for special attributes: method, code, reason, endpoint, etc.
      outpayload->Attribute(std::string(key),*i);
    };
  };
  outpayload->KeepAlive(keep_alive);
  if(retpayload) {
    outpayload->Body(*retpayload);
  } else {
    outpayload->Body(*strpayload);
  }
  bool flush_r = outpayload->Flush();
  delete outpayload;
  outmsg = nextoutmsg;
  // Returning empty payload because response is already sent through Flush
  PayloadRaw* outpayload_e = new PayloadRaw;
  outmsg.Payload(outpayload_e);
  if(!flush_r) {
    // If flush failed then we can't know if anything HTTPish was 
    // already sent. Hence we are just making lower level close
    // connection.
    logger.msg(WARNING, "Error to flush output payload");
    return MCC_Status(SESSION_CLOSE);
  };
  if(!keep_alive) return MCC_Status(SESSION_CLOSE);
  return MCC_Status(STATUS_OK);
}

MCC_HTTP_Client::MCC_HTTP_Client(Config *cfg):MCC_HTTP(cfg) {
  endpoint_=(std::string)((*cfg)["Endpoint"]);
  method_=(std::string)((*cfg)["Method"]);
}

MCC_HTTP_Client::~MCC_HTTP_Client(void) {
}

MCC_Status MCC_HTTP_Client::process(Message& inmsg,Message& outmsg) {
  // Take Raw payload, add HTTP stuf by using PayloadHTTP and
  // generate new Raw payload to pass further through chain.
  // TODO: do not create new object - use or acqure same one.
  // Extracting payload
  if(!inmsg.Payload()) return make_raw_fault(outmsg);
  PayloadRawInterface* inpayload = NULL;
  try {
    inpayload = dynamic_cast<PayloadRawInterface*>(inmsg.Payload());
  } catch(std::exception& e) { };
  if(!inpayload) return make_raw_fault(outmsg);
  // Making HTTP request
  // Use attributes which higher level MCC may have produced for HTTP
  std::string http_method = inmsg.Attributes()->get("HTTP:METHOD");
  std::string http_endpoint = inmsg.Attributes()->get("HTTP:ENDPOINT");
  if(http_method.empty()) http_method=method_;
  if(http_endpoint.empty()) http_endpoint=endpoint_;
  PayloadHTTP nextpayload(http_method,http_endpoint);
  for(AttributeIterator i = inmsg.Attributes()->getAll();i.hasMore();++i) {
    const char* key = i.key().c_str();
    if(strncmp("HTTP:",key,5) == 0) {
      key+=5;
      // TODO: check for special attributes: method, code, reason, endpoint, etc.
      if(strcmp(key,"METHOD") == 0) continue;
      if(strcmp(key,"ENDPOINT") == 0) continue;
      nextpayload.Attribute(std::string(key),*i);
    };
  };
  nextpayload.Attribute("User-Agent","ARC");
  nextpayload.Body(*inpayload,false);
  nextpayload.Flush();
  // Creating message to pass to next MCC and setting new payload..
  Message nextinmsg = inmsg;
  nextinmsg.Payload(&nextpayload);

  // Call next MCC
  MCCInterface* next = Next();
  if(!next) return make_raw_fault(outmsg);
  Message nextoutmsg = outmsg; nextoutmsg.Payload(NULL);
  MCC_Status ret = next->process(nextinmsg,nextoutmsg);
  // Do checks and process response - supported response so far is stream
  // Generated result is HTTP payload with Raw and Stream interfaces
  if(!ret) {
    if(nextoutmsg.Payload()) delete nextoutmsg.Payload();
    return make_raw_fault(outmsg);
  };
  if(!nextoutmsg.Payload()) return make_raw_fault(outmsg);
  PayloadStreamInterface* retpayload = NULL;
  try {
    retpayload = dynamic_cast<PayloadStreamInterface*>(nextoutmsg.Payload());
  } catch(std::exception& e) { };
  if(!retpayload) { delete nextoutmsg.Payload(); return make_raw_fault(outmsg); };
  // Stream retpayload becomes owned by outpayload. This is needed because
  // HTTP payload may postpone extracting information from stream till demanded.
  PayloadHTTP* outpayload  = new PayloadHTTP(*retpayload,true);
  if(!outpayload) { delete retpayload; return make_raw_fault(outmsg); };
  if(!(*outpayload)) { delete outpayload; return make_raw_fault(outmsg); };
  // Check for closed connection during response - not suitable in client mode
  if(outpayload->Method() == "END") { delete outpayload; return make_raw_fault(outmsg); };
  outmsg = nextoutmsg;
  // Payload returned by next.process is not destroyed here because
  // it is now owned by outpayload.
  outmsg.Payload(outpayload);
  outmsg.Attributes()->set("HTTP:CODE",tostring(outpayload->Code()));
  outmsg.Attributes()->set("HTTP:REASON",outpayload->Reason());
  for(std::map<std::string,std::string>::const_iterator i =
      outpayload->Attributes().begin();i!=outpayload->Attributes().end();++i) {
    outmsg.Attributes()->add("HTTP:"+i->first,i->second);
  };
  return MCC_Status(STATUS_OK);
}

} // namespace Arc
