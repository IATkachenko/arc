#ifndef __ARC_AREX_H__
#define __ARC_AREX_H__

#include <arc/message/PayloadRaw.h>
#include <arc/delegation/DelegationInterface.h>
#include <arc/infosys/InformationInterface.h>
#include <arc/infosys/InfoRegister.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>

#include "FileChunks.h"
#include "grid-manager/GridManager.h"
//#include "delegation/DelegationStore.h"
#include "delegation/DelegationStores.h"
#include "grid-manager/conf/GMConfig.h"

namespace ARex {

class ARexGMConfig;
class ARexConfigContext;
class CountedResourceLock;
class DelegationStores;

class CountedResource {
 friend class CountedResourceLock;
 public:
  CountedResource(int maxconsumers = -1);
  ~CountedResource(void);
  void MaxConsumers(int maxconsumers);
 private:
  Glib::Cond cond_;
  Glib::Mutex lock_;
  int limit_;
  int count_;
  void Acquire(void);
  void Release(void);
};

class OptimizedInformationContainer: public Arc::InformationContainer {
 private:
  bool parse_xml_;
  std::string filename_;
  int handle_;
  Arc::XMLNode doc_;
  Glib::Mutex olock_;
 public:
  OptimizedInformationContainer(bool parse_xml = true);
  ~OptimizedInformationContainer(void);
  int OpenDocument(void);
  Arc::MessagePayload* Process(Arc::SOAPEnvelope& in);
  void AssignFile(const std::string& filename);
  void Assign(const std::string& xml);
};

#define AREXOP(NAME) Arc::MCC_Status NAME(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out)
class ARexService: public Arc::Service {
 private:
  static void gm_threads_starter(void* arg);
  void gm_threads_starter();
 protected:
  Arc::ThreadRegistry thread_count_;
  Arc::NS ns_;
  Arc::Logger logger_;
  DelegationStores delegation_stores_;
  OptimizedInformationContainer infodoc_;
  Arc::InfoRegisters* inforeg_;
  CountedResource infolimit_;
  CountedResource beslimit_;
  CountedResource datalimit_;
  std::string endpoint_;
  bool publishstaticinfo_;
  std::string uname_;
  std::string common_name_;
  std::string long_description_;
  std::string lrms_name_;
  std::string os_name_;
  std::string gmrun_;
  unsigned int infoprovider_wakeup_period_;
  unsigned int all_jobs_count_;
  //Glib::Mutex glue_states_lock_;
  //std::map<std::string,std::string> glue_states_;
  FileChunksList files_chunks_;
  GMConfig config_;
  GridManager* gm_;
  ARexConfigContext* get_configuration(Arc::Message& inmsg);

  // A-REX operations
  Arc::MCC_Status CreateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  AREXOP(GetActivityStatuses);
  AREXOP(TerminateActivities);
  AREXOP(GetActivityDocuments);
  AREXOP(GetFactoryAttributesDocument);

  AREXOP(StopAcceptingNewActivities);
  AREXOP(StartAcceptingNewActivities);

  AREXOP(ChangeActivityStatus);
  Arc::MCC_Status MigrateActivity(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  AREXOP(CacheCheck);
  Arc::MCC_Status UpdateCredentials(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& credentials);

  // EMI ES operations
  Arc::MCC_Status ESCreateActivities(ARexGMConfig& config,Arc::XMLNode in,Arc::XMLNode out,const std::string& clientid);
  AREXOP(ESGetResourceInfo);
  AREXOP(ESQueryResourceInfo);
  AREXOP(ESPauseActivity);
  AREXOP(ESResumeActivity);
  AREXOP(ESNotifyService);
  AREXOP(ESCancelActivity);
  AREXOP(ESWipeActivity);
  AREXOP(ESRestartActivity);
  AREXOP(ESListActivities);
  AREXOP(ESGetActivityStatus);
  AREXOP(ESGetActivityInfo);

  // Convenience methods
  Arc::MCC_Status make_empty_response(Arc::Message& outmsg);
  Arc::MCC_Status make_fault(Arc::Message& outmsg);
  Arc::MCC_Status make_http_fault(Arc::Message& outmsg,int code,const char* resp);
  Arc::MCC_Status make_soap_fault(Arc::Message& outmsg,const char* resp = NULL);

  // HTTP operations
  Arc::MCC_Status Get(Arc::Message& inmsg,Arc::Message& outmsg,ARexGMConfig& config,std::string id,std::string subpath);
  Arc::MCC_Status Head(Arc::Message& inmsg,Arc::Message& outmsg,ARexGMConfig& config,std::string id,std::string subpath);
  Arc::MCC_Status Put(Arc::Message& inmsg,Arc::Message& outmsg,ARexGMConfig& config,std::string id,std::string subpath);

  // A-REX faults
  void GenericFault(Arc::SOAPFault& fault);
  void NotAuthorizedFault(Arc::XMLNode fault);
  void NotAuthorizedFault(Arc::SOAPFault& fault);
  void NotAcceptingNewActivitiesFault(Arc::XMLNode fault);
  void NotAcceptingNewActivitiesFault(Arc::SOAPFault& fault);
  void UnsupportedFeatureFault(Arc::XMLNode fault,const std::string& feature);
  void UnsupportedFeatureFault(Arc::SOAPFault& fault,const std::string& feature);
  void CantApplyOperationToCurrentStateFault(Arc::XMLNode fault,const std::string& gm_state,bool failed,const std::string& message);
  void CantApplyOperationToCurrentStateFault(Arc::SOAPFault& fault,const std::string& gm_state,bool failed,const std::string& message);
  void OperationWillBeAppliedEventuallyFault(Arc::XMLNode fault,const std::string& gm_state,bool failed,const std::string& message);
  void OperationWillBeAppliedEventuallyFault(Arc::SOAPFault& fault,const std::string& gm_state,bool failed,const std::string& message);
  void UnknownActivityIdentifierFault(Arc::XMLNode fault,const std::string& message);
  void UnknownActivityIdentifierFault(Arc::SOAPFault& fault,const std::string& message);
  void InvalidRequestMessageFault(Arc::XMLNode fault,const std::string& element,const std::string& message);
  void InvalidRequestMessageFault(Arc::SOAPFault& fault,const std::string& element,const std::string& message);


  // EMI ES faults
#define ES_MSG_FAULT_HEAD(NAME) \
  void NAME(Arc::XMLNode fault,const std::string& message,const std::string& desc = ""); \
  void NAME(Arc::SOAPFault& fault,const std::string& message,const std::string& desc = "");
#define ES_SIMPLE_FAULT_HEAD(NAME) \
  void NAME(Arc::XMLNode fault,const std::string& message = "",const std::string& desc = ""); \
  void NAME(Arc::SOAPFault& fault,const std::string& message = "",const std::string& desc = "");

  ES_MSG_FAULT_HEAD(ESInternalBaseFault)
  void ESVectorLimitExceededFault(Arc::XMLNode fault,unsigned long limit,const std::string& message = "",const std::string& desc = "");
  void ESVectorLimitExceededFault(Arc::SOAPFault& fault,unsigned long limit,const std::string& message = "",const std::string& desc = "");
  ES_SIMPLE_FAULT_HEAD(ESAccessControlFault);

  ES_SIMPLE_FAULT_HEAD(ESUnsupportedCapabilityFault)
  ES_SIMPLE_FAULT_HEAD(ESInvalidActivityDescriptionSemanticFault)
  ES_SIMPLE_FAULT_HEAD(ESInvalidActivityDescriptionFault)

  ES_SIMPLE_FAULT_HEAD(ESNotSupportedQueryDialectFault)
  ES_SIMPLE_FAULT_HEAD(ESNotValidQueryStatementFault)
  ES_SIMPLE_FAULT_HEAD(ESUnknownQueryFault)
  ES_SIMPLE_FAULT_HEAD(ESInternalResourceInfoFault)
  ES_SIMPLE_FAULT_HEAD(ESResourceInfoNotFoundFault)

  ES_SIMPLE_FAULT_HEAD(ESUnableToRetrieveStatusFault)
  ES_SIMPLE_FAULT_HEAD(ESUnknownAttributeFault)
  ES_SIMPLE_FAULT_HEAD(ESOperationNotAllowedFault)
  ES_SIMPLE_FAULT_HEAD(ESActivityNotFoundFault)
  ES_SIMPLE_FAULT_HEAD(ESInternalNotificationFault)
  ES_SIMPLE_FAULT_HEAD(ESOperationNotPossibleFault)
  ES_SIMPLE_FAULT_HEAD(ESInvalidActivityStateFault)
  ES_SIMPLE_FAULT_HEAD(ESInvalidActivityLimitFault)
  ES_SIMPLE_FAULT_HEAD(ESInvalidParameterFault)

 public:
  ARexService(Arc::Config *cfg,Arc::PluginArgument *parg);
  virtual ~ARexService(void);
  virtual Arc::MCC_Status process(Arc::Message& inmsg,Arc::Message& outmsg);
  void InformationCollector(void);
  virtual bool RegistrationCollector(Arc::XMLNode &doc);
  virtual std::string getID();
  void StopChildThreads(void);
};

} // namespace ARex

#endif

