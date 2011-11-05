#ifndef __ARC_AREX_JOB_H__
#define __ARC_AREX_JOB_H__

#include <string>
#include <list>

#include <arc/XMLNode.h>
#include <arc/FileAccess.h>
#include <arc/message/MessageAuth.h>
#include "grid-manager/jobs/users.h"
#include "grid-manager/files/info_types.h"
#include "grid-manager/jobs/job.h"
#include "grid-manager/jobs/plugins.h"

namespace ARex {

#define JOB_POLICY_OPERATION_URN "http://www.nordugrid.org/schemas/policy-arc/types/a-rex/joboperation"
#define JOB_POLICY_OPERATION_CREATE "Create"
#define JOB_POLICY_OPERATION_MODIFY "Modify"
#define JOB_POLICY_OPERATION_READ   "Read"

class ARexGMConfig {
 private:
  JobUser *user_;
  bool readonly_;
  std::list<std::string> queues_;
  std::string grid_name_; // temporary solution
  std::string service_endpoint_; // temporary solution
  std::list<Arc::MessageAuth*> auths_;
  ContinuationPlugins cont_plugins_;
  std::vector<std::string> session_roots_non_draining_;
 public:
  ARexGMConfig(const GMEnvironment& env,const std::string& uname,const std::string& grid_name,const std::string& service_endpoint);
  ~ARexGMConfig(void);
  operator bool(void) const { return (user_ != NULL); };
  bool operator!(void) const { return (user_ == NULL); };
  JobUser* User(void) { return user_; };
  bool ReadOnly(void) const { return readonly_; };
  const std::string& GridName(void) const { return grid_name_; };
  const std::string& Endpoint(void) const { return service_endpoint_; };
  const std::list<std::string>& Queues(void) const { return queues_; };
  //static bool InitEnvironment(const std::string& configfile);
  void AddAuth(Arc::MessageAuth* auth) { auths_.push_back(auth); };
  void ClearAuths(void) { auths_.clear(); };
  std::list<Arc::MessageAuth*>::iterator beginAuth(void) { return auths_.begin(); };
  std::list<Arc::MessageAuth*>::iterator endAuth(void) { return auths_.end(); };
  ContinuationPlugins& Plugins(void) { return cont_plugins_; };
  std::vector<std::string> SessionRootsNonDraining(void) { return session_roots_non_draining_; };
};


typedef enum {
  ARexJobNoError,
  ARexJobInternalError, // Failed during some internal operation - like writing some file
  ARexJobConfigurationError, // Problem detected which can be fixed by adjusting configuration of service
  ARexJobDescriptionUnsupportedError, // Job asks for feature or combination not supported by service
  ARexJobDescriptionMissingError, // Job is missing optional but needed for this service element
  ARexJobDescriptionSyntaxError, // Job description is malformed - missing elements, wrong names, etc.
  ARexJobDescriptionLogicalError // Job request otherwise corect has some values out of scope of service
} ARexJobFailure;

/** This class represents convenience interface to manage jobs 
  handled by Grid Manager. It works mostly through corresponding
  classes and functions of Grid Manager. */
class ARexJob {
 private:
  std::string id_;
  std::string failure_;
  ARexJobFailure failure_type_;
  bool allowed_to_see_;
  bool allowed_to_maintain_;
  Arc::Logger& logger_;
  /** Returns true if job exists and authorization was checked 
    without errors. Fills information about authorization in 
    this instance. */ 
  bool is_allowed(bool fast = false);
  ARexGMConfig& config_;
  JobLocalDescription job_;
  bool make_job_id(void);
  bool delete_job_id(void);
  bool update_credentials(const std::string& credentials);
 public:
  /** Create instance which is an interface to existing job */
  ARexJob(const std::string& id,ARexGMConfig& config,Arc::Logger& logger,bool fast_auth_check = false);
  /** Create new job with provided JSDL description */
  ARexJob(Arc::XMLNode jsdl,ARexGMConfig& config,const std::string& credentials,const std::string& clientid,Arc::Logger& logger,Arc::XMLNode migration = Arc::XMLNode());
  operator bool(void) { return !id_.empty(); };
  bool operator!(void) { return id_.empty(); };
  /** Returns textual description of failure of last operation */
  std::string Failure(void) { std::string r=failure_; failure_=""; failure_type_=ARexJobNoError; return r; };
  operator ARexJobFailure(void) { return failure_type_; };
  /** Return ID assigned to job */
  std::string ID(void) { return id_; };
  /** Fills provided jsdl with job description */
  bool GetDescription(Arc::XMLNode& jsdl);
  /** Cancel processing/execution of job */
  bool Cancel(void);
  /** Remove job from local pool */
  bool Clean(void);
  /** Resume execution of job after error */
  bool Resume(void);
  /** Returns current state of job */
  std::string State(void);
  /** Returns current state of job and sets job_pending to 
     true if job is pending due to external limits */
  std::string State(bool& job_pending);
  /** Returns true if job has failed */
  bool Failed(void);
  /** Returns state at which job failed and sets cause to 
     information what caused job failure: "internal" for server
     initiated and "client" for canceled on client request. */
  std::string FailedState(std::string& cause);
  /** Returns path to session directory */
  std::string SessionDir(void);
  /** Returns name of virtual log directory */
  std::string LogDir(void);
  /** Return number of jobs associated with this configuration.
      TODO: total for all user configurations. */
  static int TotalJobs(ARexGMConfig& config,Arc::Logger& logger);
  /** Returns list of user's jobs. Fine-grained ACL is ignored. */
  static std::list<std::string> Jobs(ARexGMConfig& config,Arc::Logger& logger);
  /** Creates file in job's session directory and returns handler */
  Arc::FileAccess* CreateFile(const std::string& filename);
  /** Opens file in job's session directory and returns handler */
  Arc::FileAccess* OpenFile(const std::string& filename,bool for_read,bool for_write);
  std::string GetFilePath(const std::string& filename);
  bool ReportFileComplete(const std::string& filename);
  /** Opens log file in control directory */
  int OpenLogFile(const std::string& name);
  std::string GetLogFilePath(const std::string& name);
  /** Opens directory inside session directory */
  Arc::FileAccess* OpenDir(const std::string& dirname);
  /** Returns list of existing log files */
  std::list<std::string> LogFiles(void);
  /** Updates job credentials */
  bool UpdateCredentials(const std::string& credentials);
  /** Select a session dir to use for this job */
  bool ChooseSessionDir(const std::string& jobid, std::string& sessiondir);
};

} // namespace ARex

#endif
