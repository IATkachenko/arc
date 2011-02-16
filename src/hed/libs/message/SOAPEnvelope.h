#ifndef __ARC_SOAPENVELOP_H__
#define __ARC_SOAPENVELOP_H__

#include <string>

#include <arc/XMLNode.h>

namespace Arc {

class SOAPEnvelope;

  /// Interface to SOAP Fault message.
  /** SOAPFault class provides a convenience interface for accessing elements 
    of SOAP faults.  It also tries to expose single interface for both version 
    1.0 and 1.2 faults.  This class is not intended to 'own' any information
    stored. It's purpose is to manipulate information which is kept under 
    control of XMLNode or SOAPEnvelope classes. If instance does not refer to 
    valid SOAP Fault structure all manipulation methods will have no effect. */
class SOAPFault {
 friend class SOAPEnvelope;
 private:
  bool ver12;        /** true if SOAP version is 1.2 */
  XMLNode fault;     /** Fault element of SOAP */
  XMLNode code;      /** Code element of SOAP Fault */
  XMLNode reason;    /** Reason element of SOAP Fault */
  XMLNode node;      /** Node element of SOAP Fault */
  XMLNode role;      /** Role element of SOAP Fault */
  XMLNode detail;    /** Detail element of SOAP Fault */
 public:
  /** Fault codes of SOAP specs */
  typedef enum {
    undefined,
    unknown,
    VersionMismatch,
    MustUnderstand,
    Sender,   /* Client in SOAP 1.0 */
    Receiver, /* Server in SOAP 1.0 */
    DataEncodingUnknown
  } SOAPFaultCode;
  /** Parse Fault elements of SOAP Body or any other XML tree with Fault element */
  SOAPFault(XMLNode body);
  /** Creates Fault element inside @body SOAP Body node with specified @code and @reason.
     Version of Fault element is picked from SOAP Body version. */
  SOAPFault(XMLNode body,SOAPFaultCode code,const char* reason);
  /** Creates Fault element inside @body SOAP Body node with specified @code and @reason.
     SOAP version of Fault element must be specified explicitely. */
  SOAPFault(XMLNode body,SOAPFaultCode code,const char* reason,bool ver12);
  /** Returns true if instance refers to SOAP Fault */
  operator bool(void) { return (bool)fault; };
  /** Returns top level Fault element.
     This element is not automatically created. */
  operator XMLNode(void) { return fault; };
  /** Returns Fault Code element */
  SOAPFaultCode Code(void);
  /** Set Fault Code element */
  void Code(SOAPFaultCode code);
  /** Returns Fault Subcode element at various levels (0 is for Code) */
  std::string Subcode(int level);
  /** Set Fault Subcode element at various levels (0 is for Code) to 'subcode' */
  void Subcode(int level,const char* subcode);
  /** Returns content of Fault Reason element at various levels */
  std::string Reason(int num = 0);
  /** Set Fault Reason content at various levels to 'reason' */
  void Reason(int num,const char* reason);
  /** Set Fault Reason element at top level */
  void Reason(const char* reason) { Reason(0,reason); };
  /** Set Fault Reason element at top level */
  void Reason(const std::string &reason) { Reason(0, reason.c_str()); };
  /** Returns content of Fault Node element */
  std::string Node(void);
  /** Set content of Fault Node element to 'node' */
  void Node(const char* node);
  /** Returns content of Fault Role element */
  std::string Role(void);
  /** Set content of Fault Role element to 'role' */
  void Role(const char* role);
  /** Access Fault Detail element. If create is set to true this element is created if not present. */
  XMLNode Detail(bool create = false);
  /** Convenience method for creating SOAP Fault message.
     Returns full SOAP message representing Fault with
    specified code and reason. */
  static SOAPEnvelope* MakeSOAPFault(SOAPFaultCode code,const std::string& reason = "");
};

/// Extends XMLNode class to support structures of SOAP message.
/** All XMLNode methods are exposed by inheriting from XMLNode and if 
   used are applied to Body part of SOAP. Direct access to whole SOAP
   message/Envelope is not provided in order to protect internal 
   variables - although  full protection is not possible. */
class SOAPEnvelope: public XMLNode {
 public:
  typedef enum {
    Version_1_1,
    Version_1_2
  } SOAPVersion;
  /** Create new SOAP message from textual representation of XML document.
    Created XML structure is owned by this instance.
    This constructor also sets default namespaces to default prefixes 
    as specified below. */
  SOAPEnvelope(const std::string& xml);
  /** Same as previous */
  SOAPEnvelope(const char* xml,int len = -1);
  /** Create new SOAP message with specified namespaces.
    Created XML structure is owned by this instance.
    If argument fault is set to true created message is fault.  */
  SOAPEnvelope(const NS& ns,bool fault = false);
  /** Acquire XML document as SOAP message.
    Created XML structure is NOT owned by this instance. */
  SOAPEnvelope(XMLNode root);
  /** Create a copy of another SOAPEnvelope object. */
  SOAPEnvelope(const SOAPEnvelope& soap);
  ~SOAPEnvelope(void);
  /** Creates complete copy of SOAP.
    Do not use New() method of XMLNode for copying whole SOAP - use this one. */
  SOAPEnvelope* New(void);
  /** Swaps internals of two SOAPEnvelope instances.
    This method is identical to XMLNode::Swap() but also takes into account
    all internals of SOAPEnvelope class. */
  void Swap(SOAPEnvelope& soap);
  /** Swaps SOAP message and generic XML tree.
    XMLNode variable gets whole SOAP message and this instance is filled
    with content of analyzed XMLNode like in case of SOAPEnvelope(XMLNode)
    constructor. Ownerships are swapped too. */
  void Swap(Arc::XMLNode& soap);
  /** Modify assigned namespaces. 
    Default namespaces and prefixes are
     soap-enc http://schemas.xmlsoap.org/soap/encoding/
     soap-env http://schemas.xmlsoap.org/soap/envelope/
     xsi      http://www.w3.org/2001/XMLSchema-instance
     xsd      http://www.w3.org/2001/XMLSchema
  */
  void Namespaces(const NS& namespaces);
  NS Namespaces(void);
  // Setialize SOAP message into XML document
  void GetXML(std::string& out_xml_str,bool user_friendly = false) const;
  /** Get SOAP header as XML node. */
  XMLNode Header(void) { return header; };
  /** Get SOAP body as XML node.
     It is not necessary to use this method because instance of this class
    itself represents SOAP Body. */
  XMLNode Body(void) { return body; };
  /** Returns true if message is Fault. */
  bool IsFault(void) { return fault != NULL; };
  /** Get Fault part of message. Returns NULL if message is not Fault. */
  SOAPFault* Fault(void) { return fault; };
  /** Makes this object a copy of another SOAPEnvelope object. */
  SOAPEnvelope& operator=(const SOAPEnvelope& soap);
  SOAPVersion Version(void) { return ver12?Version_1_2:Version_1_1; };
 protected:
  XMLNode envelope; /** Envelope element of SOAP and owner of XML tree */
  XMLNode header;   /** Header element of SOAP */
  XMLNode body;     /** Body element of SOAP */
 private:
  bool ver12;       /** Is true if SOAP version 1.2 is used */
  SOAPFault* fault; /**Fault element of SOAP, NULL if message is not a fault. */
  /** Fill instance variables parent XMLNode class. 
    This method is called from constructors. */
  void set(void);
};

} // namespace Arc 

#endif /* __ARC_SOAPENVELOP_H__ */

