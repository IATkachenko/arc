#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/loader/Plugin.h>

#include "simplelistpdp/SimpleListPDP.h"
#include "pdpserviceinvoker/PDPServiceInvoker.h"
#include "delegationpdp/DelegationPDP.h"
#include "arcpdp/ArcPDP.h"
#include "xacmlpdp/XACMLPDP.h"
#include "allowpdp/AllowPDP.h"
#include "denypdp/DenyPDP.h"

#include "arcauthzsh/ArcAuthZ.h"
#include "usernametokensh/UsernameTokenSH.h"
#include "x509tokensh/X509TokenSH.h"
#include "samltokensh/SAMLTokenSH.h"
#include "saml2sso_assertionconsumersh/SAML2SSO_AssertionConsumerSH.h"
#include "delegationsh/DelegationSH.h"

#include "arcpdp/ArcPolicy.h"
#include "xacmlpdp/XACMLPolicy.h"
#include "gaclpdp/GACLPolicy.h"

#include "arcpdp/ArcEvaluator.h"
#include "xacmlpdp/XACMLEvaluator.h"
#include "gaclpdp/GACLEvaluator.h"

#include "arcpdp/ArcRequest.h"
#include "xacmlpdp/XACMLRequest.h"
#include "gaclpdp/GACLRequest.h"

#include "arcpdp/ArcAttributeFactory.h"
#include "arcpdp/ArcAlgFactory.h"
#include "arcpdp/ArcFnFactory.h"

#include "xacmlpdp/XACMLAttributeFactory.h"
#include "xacmlpdp/XACMLAlgFactory.h"
#include "xacmlpdp/XACMLFnFactory.h"

using namespace ArcSec;

extern Arc::PluginDescriptor const ARC_PLUGINS_TABLE_NAME[] = {
    { "simplelist.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::SimpleListPDP::get_simplelist_pdp},
    { "arc.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::ArcPDP::get_arc_pdp},
    { "xacml.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::XACMLPDP::get_xacml_pdp},
    { "pdpservice.invoker", "HED:PDP", NULL, 0,
                  &ArcSec::PDPServiceInvoker::get_pdpservice_invoker},
    { "delegation.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::DelegationPDP::get_delegation_pdp},
    { "allow.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::AllowPDP::get_allow_pdp},
    { "deny.pdp", "HED:PDP", NULL, 0,
                  &ArcSec::DenyPDP::get_deny_pdp},
    { "arc.authz", "HED:SHC", NULL, 0,
                  &ArcSec::ArcAuthZ::get_sechandler},
    { "usernametoken.handler",           "HED:SHC", NULL, 0,
                  &ArcSec::UsernameTokenSH::get_sechandler},
#ifdef HAVE_XMLSEC
    { "x509token.handler", "HED:SHC", NULL, 0,
                  &ArcSec::X509TokenSH::get_sechandler},
    { "samltoken.handler", "HED:SHC", NULL, 0,
                  &ArcSec::SAMLTokenSH::get_sechandler},
    { "saml2ssoassertionconsumer.handler", "HED:SHC", NULL, 0,
                  &ArcSec::SAML2SSO_AssertionConsumerSH::get_sechandler},
#endif
    { "delegation.handler", "HED:SHC", NULL, 0,
                  &ArcSec::DelegationSH::get_sechandler},
    { "arc.policy", "__arc_policy_modules__", NULL, 0,
                  &ArcSec::ArcPolicy::get_policy },
    { "xacml.policy", "__arc_policy_modules__", NULL, 0,
                 &ArcSec::XACMLPolicy::get_policy },
    { "gacl.policy", "__arc_policy_modules__", NULL, 0,     //__gacl_policy_modules__  --> __arc_policy_modules__
                  &ArcSec::GACLPolicy::get_policy },
    { "arc.evaluator", "__arc_evaluator_modules__", NULL, 0,
                  &ArcSec::ArcEvaluator::get_evaluator },
    { "xacml.evaluator", "__arc_evaluator_modules__", NULL, 0,
                  &ArcSec::XACMLEvaluator::get_evaluator },
    { "gacl.evaluator", "__arc_evaluator_modules__", NULL, 0,
                 &ArcSec::GACLEvaluator::get_evaluator },
    { "arc.request", "__arc_request_modules__", NULL, 0,
                 &ArcSec::ArcRequest::get_request },
    { "xacml.request", "__arc_request_modules__", NULL, 0,
                 &ArcSec::XACMLRequest::get_request },
    { "gacl.request", "__arc_request_modules__", NULL, 0,
                 &ArcSec::GACLRequest::get_request },
    { "arc.attrfactory", "__arc_attrfactory_modules__", NULL, 0,
                 &get_arcpdp_attr_factory },
    { "arc.algfactory", "__arc_algfactory_modules__", NULL, 0,
                 &get_arcpdp_alg_factory },
    { "arc.fnfactory", "__arc_fnfactory_modules__", NULL, 0,
                 &get_arcpdp_fn_factory },
    { "xacml.attrfactory", "__arc_attrfactory_modules__", NULL, 0,
                 &get_xacmlpdp_attr_factory },
    { "xacml.algfactory", "__arc_algfactory_modules__", NULL, 0,
                 &get_xacmlpdp_alg_factory },
    { "xacml.fnfactory", "__arc_fnfactory_modules__", NULL, 0,
                 &get_xacmlpdp_fn_factory },
    { NULL, NULL, NULL, 0, NULL }
};

