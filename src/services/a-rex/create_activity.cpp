#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/message/SOAPEnvelope.h>
#include <arc/ws-addressing/WSA.h>
#include "grid-manager/jobs/job_config.h"
#include "job.h"

#include "arex.h"

namespace ARex {


static bool max_jobs_reached(const JobsListConfig& jobs_cfg, unsigned int all_jobs) {
  // Apply limit on total number of jobs.
  //  Using collected glue states to evaluate number of jobs. - Not anymore
//!!  glue_states_lock_.lock();
//!!  int jobs_total = glue_states_.size();
//!!  glue_states_lock_.unlock();
  int jobs_total = all_jobs;
  int max_active;
  int max_running;
  int max_per_dn;
  int max_total;
  jobs_cfg.GetMaxJobs(max_active,max_running,max_per_dn,max_total);
  if(max_total > 0 && jobs_total >= max_total) return true;
  return false;
};
 

Arc::MCC_Status ARexService::CreateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid) {
  /*
  CreateActivity
    ActivityDocument
      jsdl:JobDefinition

  CreateActivityResponse
    ActivityIdentifier (wsa:EndpointReferenceType)
    ActivityDocument
      jsdl:JobDefinition

  NotAuthorizedFault
  NotAcceptingNewActivitiesFault
  UnsupportedFeatureFault
  InvalidRequestMessageFault
  */
  if(Arc::VERBOSE >= logger_.getThreshold()) {
    std::string s;
    in.GetXML(s);
    logger_.msg(Arc::VERBOSE, "CreateActivity: request = \n%s", s);
  };
  Arc::XMLNode jsdl = in["ActivityDocument"]["JobDefinition"];
  if(!jsdl) {
    // Wrongly formated request
    logger_.msg(Arc::ERROR, "CreateActivity: no job description found");
    Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Can't find JobDefinition element in request");
    InvalidRequestMessageFault(fault,"jsdl:JobDefinition","Element is missing");
    out.Destroy();
    return Arc::MCC_Status();
  };

  if(max_jobs_reached(*jobs_cfg_,all_jobs_count_)) {
    logger_.msg(Arc::ERROR, "CreateActivity: max jobs total limit reached");
    Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Reached limit of total allowed jobs");
    GenericFault(fault);
    out.Destroy();
    return Arc::MCC_Status();
  };

  // HPC Basic Profile 1.0 comply (these fault handlings are defined in the KnowARC standards 
  // conformance roadmap 2nd release)

 // End of the HPC BP 1.0 fault handling part

  std::string delegation;
  Arc::XMLNode delegated_token = in["arcdeleg:DelegatedToken"];
  if(delegated_token) {
    // Client wants to delegate credentials
    if(!delegation_stores_.DelegatedToken(config.User()->DelegationDir(),delegated_token,config.GridName(),delegation)) {
      // Failed to accept delegation (report as bad request)
      logger_.msg(Arc::ERROR, "CreateActivity: Failed to accept delegation");
      Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Failed to accept delegation");
      InvalidRequestMessageFault(fault,"arcdeleg:DelegatedToken","This token does not exist");
      out.Destroy();
      return Arc::MCC_Status();
    };
  };
  JobIDGeneratorARC idgenerator(config.Endpoint());
  ARexJob job(jsdl,config,delegation,clientid,logger_,idgenerator);
  if(!job) {
    ARexJobFailure failure_type = job;
    std::string failure = job.Failure();
    switch(failure_type) {
      case ARexJobDescriptionUnsupportedError: {
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Unsupported feature in job description");
        UnsupportedFeatureFault(fault,failure);
      }; break;
      case ARexJobDescriptionMissingError: {
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Missing needed element in job description");
        UnsupportedFeatureFault(fault,failure);
      }; break;
      case ARexJobDescriptionLogicalError: {
        std::string element;
        std::string::size_type pos = failure.find(' ');
        if(pos != std::string::npos) {
          element=failure.substr(0,pos);
          failure=failure.substr(pos+1);
        };
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Logical error in job description");
        InvalidRequestMessageFault(fault,element,failure);
      }; break;
      default: {
        logger_.msg(Arc::ERROR, "CreateActivity: Failed to create new job: %s",failure);
        // Failed to create new job (no corresponding BES fault defined - using generic SOAP error)
        logger_.msg(Arc::ERROR, "CreateActivity: Failed to create new job");
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,("Failed to create new activity: "+failure).c_str());
        GenericFault(fault);
      };
    };
    out.Destroy();
    return Arc::MCC_Status();
  };
  // Make SOAP response
  Arc::WSAEndpointReference identifier(out.NewChild("bes-factory:ActivityIdentifier"));
  // Make job's ID
  identifier.Address(config.Endpoint()); // address of service
  identifier.ReferenceParameters().NewChild("a-rex:JobID")=job.ID();
  identifier.ReferenceParameters().NewChild("a-rex:JobSessionDir")=config.Endpoint()+"/"+job.ID();
  out.NewChild(in["ActivityDocument"]);
  logger_.msg(Arc::VERBOSE, "CreateActivity finished successfully");
  if(Arc::VERBOSE >= logger_.getThreshold()) {
    std::string s;
    out.GetXML(s);
    logger_.msg(Arc::VERBOSE, "CreateActivity: response = \n%s", s);
  };
  return Arc::MCC_Status(Arc::STATUS_OK);
}

Arc::MCC_Status ARexService::ESCreateActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid) {
  /*
    CreateActivities
      adl:ActivityDescription - http://www.eu-emi.eu/es/2010/12/adl 1-unbounded

    CreateActivitiesResponse
      ActivityCreationResponse 1-
        types:ActivityID
        types:ActivityManagerURI
        types:ActivityStatus
        types:ETNSC 0-1
        types:StageInDirectory 0-1
        types:SessionDirectory 0-1
        types:StageOutDirectory 0-1
        or types:InternalBaseFault
        (UnsupportedCapabilityFault)
        (InvalidActivityDescriptionSemanticFault)
        (InvalidActivityDescriptionFault)

    types:InternalBaseFault
    types:AccessControlFault
    types:VectorLimitExceededFault
  */

  if(Arc::VERBOSE >= logger_.getThreshold()) {
    std::string s;
    in.GetXML(s);
    logger_.msg(Arc::VERBOSE, "EMIES:CreateActivities: request = \n%s", s);
  };
  Arc::XMLNode adl = in["ActivityDescription"];
  if(!adl) {
    // Wrongly formated request
    logger_.msg(Arc::ERROR, "EMIES:CreateActivities: no job description found");
    Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"ActivityDescription element is missing");
    ESInternalBaseFault(fault,"ActivityDescription element is missing");
    out.Destroy();
    return Arc::MCC_Status();
  };
  Arc::XMLNode adl2 = adl; ++adl2;
  if((bool)adl2) {
    logger_.msg(Arc::ERROR, "EMIES:CreateActivities: too many job descriptions found");
    Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Too many ActivityDescription elements");
    ESVectorLimitExceededFault(fault,1,"Too many ActivityDescription elements");
    out.Destroy();
    return Arc::MCC_Status();
  };
  if(max_jobs_reached(*jobs_cfg_,all_jobs_count_)) {
    logger_.msg(Arc::ERROR, "EMIES:CreateActivities: max jobs total limit reached");
    Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Reached limit of total allowed jobs");
    ESInternalBaseFault(fault,"Reached limit of total allowed jobs");
    out.Destroy();
    return Arc::MCC_Status();
  };
  JobIDGeneratorES idgenerator(config.Endpoint());
  ARexJob job(adl,config,"",clientid,logger_,idgenerator);
  if(!job) {
    ARexJobFailure failure_type = job;
    std::string failure = job.Failure();
    switch(failure_type) {
      case ARexJobDescriptionUnsupportedError: {
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Unsupported feature in job description");
        ESUnsupportedCapabilityFault(fault,failure);
      }; break;
      case ARexJobDescriptionMissingError: {
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Missing needed element in job description");
        ESInvalidActivityDescriptionSemanticFault(fault,failure);
      }; break;
      case ARexJobDescriptionLogicalError: {
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Logical error in job description");
        ESInvalidActivityDescriptionFault(fault,failure);
      }; break;
      default: {
        logger_.msg(Arc::ERROR, "ES:CreateActivities: Failed to create new job: %s",failure);
        Arc::SOAPFault fault(out.Parent(),Arc::SOAPFault::Sender,"Failed to create new activity");
        ESInternalBaseFault(fault,failure);
      };
    };
    out.Destroy();
    return Arc::MCC_Status();
  };
  // Make SOAP response
  Arc::XMLNode resp = out.NewChild("escreate:ActivityCreationResponse");
  resp.NewChild("estypes:ActivityID")=job.ID();
  resp.NewChild("estypes:ActivityManagerURI")=config.Endpoint();
  Arc::XMLNode rstatus = addActivityStatusES(resp,"ACCEPTED",Arc::XMLNode(),false,false);
  //resp.NewChild("estypes:ETNSC");
  resp.NewChild("escreate:StageInDirectory").NewChild("escreate:URL")=config.Endpoint()+"/"+job.ID();
  resp.NewChild("escreate:SessionDirectory").NewChild("escreate:URL")=config.Endpoint()+"/"+job.ID();
  resp.NewChild("escreate:StageOutDirectory").NewChild("escreate:URL")=config.Endpoint()+"/"+job.ID();
  rstatus.NewChild("estypes:Timestamp")=Arc::Time().str(Arc::ISOTime);
  //rstatus.NewChild("estypes:Description")=;
  logger_.msg(Arc::VERBOSE, "EMIES:CreateActivities finished successfully");
  if(Arc::VERBOSE >= logger_.getThreshold()) {
    std::string s;
    out.GetXML(s);
    logger_.msg(Arc::VERBOSE, "EMIES:CreateActivities: response = \n%s", s);
  };
  return Arc::MCC_Status(Arc::STATUS_OK);
}

} // namespace ARex

