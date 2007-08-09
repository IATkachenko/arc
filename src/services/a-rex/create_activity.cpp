#include "message/SOAPEnvelope.h"
#include "ws-addressing/WSA.h"
#include "job.h"

#include "arex.h"

namespace ARex {


Arc::MCC_Status ARexService::CreateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out) {
  /*
  CreateActivity
    ActivityDocument
      jsdl:JobDefinition

  CreateActivityResponse
    ActivityIdentifier (wsa:EndpointReferenceType)
    ActivityDocument
      jsdl:JobDefinition

  InvalidRequestMessageFault
    InvalidElement
    Message
  */
  {
    std::string s;
    in.GetXML(s);
    logger.msg(Arc::DEBUG, "CreateActivity: request = \n%s", s.c_str());
  };
  Arc::XMLNode jsdl = in["ActivityDocument"]["JobDefinition"];
  if(!jsdl) {
    // Wrongly formated request
    logger.msg(Arc::ERROR, "CreateActivity: no job description found");
    Arc::SOAPEnvelope fault(ns_,true);
    if(fault) {
      fault.Fault()->Code(Arc::SOAPFault::Sender);
      fault.Fault()->Reason("Can't find JobDefinition element in request");
      Arc::XMLNode f = fault.Fault()->Detail(true).NewChild("bes-factory:InvalidRequestMessageFault");
      f.NewChild("bes-factory:InvalidElement")="jsdl:JobDefinition";
      f.NewChild("bes-factory:Message")="Element is missing";
      out.Replace(fault.Child());
    };
    return Arc::MCC_Status();
  };
  ARexJob job(jsdl,config);
  if(!job) {
    logger.msg(Arc::ERROR, "CreateActivity: Failed to create new job");
    // Failed to create new job (generic SOAP error)
    Arc::SOAPEnvelope fault(ns_,true);
    if(fault) {
      fault.Fault()->Code(Arc::SOAPFault::Receiver);
      fault.Fault()->Reason("Can't create new activity");
      out.Replace(fault.Child());
    };
    return Arc::MCC_Status();
  };
  // Make SOAP response
  Arc::WSAEndpointReference identifier(out.NewChild("bes-factory:ActivityIdentifier"));
  // Make job's ID
  identifier.Address(config.Endpoint()); // address of service
  identifier.ReferenceParameters().NewChild("a-rex:JobID")=job.ID();
  identifier.ReferenceParameters().NewChild("a-rex:JobSessionDir")=config.Endpoint()+"/"+job.ID();
  out.NewChild(in["ActivityDocument"]);
  logger.msg(Arc::DEBUG, "CreateActivity finished successfully");
  {
    std::string s;
    out.GetXML(s);
    logger.msg(Arc::DEBUG, "CreateActivity: response = \n%s", s.c_str());
  };
  return Arc::MCC_Status(Arc::STATUS_OK);
}

}; // namespace ARex

