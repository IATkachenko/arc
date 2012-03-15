#ifndef __ARC_SEC_XACMLEVALUATOR_H__
#define __ARC_SEC_XACMLEVALUATOR_H__

#include <list>
#include <fstream>

#include <arc/XMLNode.h>
#include <arc/Logger.h>
#include <arc/security/ArcPDP/Evaluator.h>
#include <arc/security/ArcPDP/fn/FnFactory.h>
#include <arc/security/ArcPDP/attr/AttributeFactory.h>
#include <arc/security/ArcPDP/alg/AlgFactory.h>
#include <arc/security/ArcPDP/Request.h>
#include <arc/security/ArcPDP/Response.h>
#include <arc/security/ArcPDP/PolicyStore.h>

namespace ArcSec {

///Execute the policy evaluation, based on the request and policy
class XACMLEvaluator : public Evaluator {
friend class EvaluatorContext;
private:
  static Arc::Logger logger;
  PolicyStore *plstore;
  FnFactory* fnfactory;
  AttributeFactory* attrfactory;  
  AlgFactory* algfactory;
  
  EvaluatorContext* context;

  Arc::XMLNode* m_cfg;
  std::string request_classname;

  EvaluatorCombiningAlg combining_alg;

  CombiningAlg* combining_alg_ex;

public:
  XACMLEvaluator (Arc::XMLNode* cfg, Arc::PluginArgument* parg);
  XACMLEvaluator (const char * cfgfile, Arc::PluginArgument* parg);
  virtual ~XACMLEvaluator();

  /**Evaluate the request based on the policy information inside PolicyStore*/
  virtual Response* evaluate(Request* request);
  virtual Response* evaluate(const Source& request);

  virtual Response* evaluate(Request* request, const Source& policy);
  virtual Response* evaluate(const Source& request, const Source& policy);
  virtual Response* evaluate(Request* request, Policy* policyobj);
  virtual Response* evaluate(const Source& request, Policy* policyobj);

  virtual AttributeFactory* getAttrFactory () { return attrfactory;};
  virtual FnFactory* getFnFactory () { return fnfactory; };
  virtual AlgFactory* getAlgFactory () { return algfactory; };

  virtual void addPolicy(const Source& policy,const std::string& id = "") {
    plstore->addPolicy(policy, context, id);
  };

  virtual void addPolicy(Policy* policy,const std::string& id = "") {
    plstore->addPolicy(policy, context, id);
  };

  virtual void removePolicies(void) { plstore->removePolicies(); };

  virtual void setCombiningAlg(EvaluatorCombiningAlg alg);

  virtual void setCombiningAlg(CombiningAlg* alg);

  virtual const char* getName(void) const;

  static Arc::Plugin* get_evaluator(Arc::PluginArgument* arg);

protected:
  virtual Response* evaluate(EvaluationCtx* ctx);

private:
  virtual void parsecfg(Arc::XMLNode& cfg);
  virtual Request* make_reqobj(Arc::XMLNode& reqnode);
};

} // namespace ArcSec

#endif /* __ARC_SEC_XACMLEVALUATOR_H__ */
