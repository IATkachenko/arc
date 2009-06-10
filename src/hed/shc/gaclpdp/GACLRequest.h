#ifndef __ARC_SEC_GACLREQUEST_H__
#define __ARC_SEC_GACLREQUEST_H__

#include <arc/XMLNode.h>
#include <arc/security/ArcPDP/Request.h>


namespace ArcSec {

class GACLRequest : public Request {

public:
  virtual ReqItemList getRequestItems () const { return rlist; };
  
  virtual void setRequestItems (ReqItemList sl) { };

  virtual void addRequestItem(Attrs& sub, Attrs& res, Attrs& act, Attrs& ctx) { };

  virtual void setAttributeFactory(AttributeFactory* attributefactory) { };

  virtual void make_request() { };

  GACLRequest ();

  GACLRequest (const Source& source);

  virtual ~GACLRequest();

  Arc::XMLNode getXML(void) { return reqnode; };

  virtual const char* getEvalName() const { return "gacl.evaluator"; };

  virtual const char* getName() const { return "gacl.request"; };

  static Arc::Plugin* get_request(Arc::PluginArgument* arg);

  virtual Arc::XMLNode& getReqNode() { return reqnode; };

private:
  Arc::XMLNode reqnode;

};

} // namespace ArcSec

#endif /* __ARC_SEC_GACLREQUEST_H__ */
