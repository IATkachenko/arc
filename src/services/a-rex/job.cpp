#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
// NOTE: On Solaris errno is not working properly if cerrno is included first
#include <cerrno>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <arc/DateTime.h>
#include <arc/Thread.h>
#include <arc/StringConv.h>
#include <arc/FileUtils.h>
#include <arc/security/ArcPDP/Evaluator.h>
#include <arc/security/ArcPDP/EvaluatorLoader.h>
#include <arc/message/SecAttr.h>
#include <arc/credential/Credential.h>
#include <arc/ws-addressing/WSA.h>

#include "grid-manager/conf/environment.h"
#include "grid-manager/conf/conf_pre.h"
#include "grid-manager/jobs/job.h"
#include "grid-manager/jobs/plugins.h"
#include "grid-manager/jobs/job_request.h"
#include "grid-manager/jobs/commfifo.h"
#include "grid-manager/run/run_plugin.h"
#include "grid-manager/files/info_files.h"

#include "job.h"

using namespace ARex;

ARexGMConfig::~ARexGMConfig(void) {
  if(user_) delete user_;
}

ARexGMConfig::ARexGMConfig(const GMEnvironment& env,const std::string& uname,const std::string& grid_name,const std::string& service_endpoint):user_(NULL),readonly_(false),grid_name_(grid_name),service_endpoint_(service_endpoint) {
  //if(!InitEnvironment(configfile)) return;
  // const char* uname = user_s.get_uname();
  //if((bool)job_map) uname=job_map.unix_name();
  user_=new JobUser(env,uname);
  if(!user_->is_valid()) { delete user_; user_=NULL; return; };
  if(env.nordugrid_loc().empty()) { delete user_; user_=NULL; return; };
  /* read configuration */
  std::vector<std::string> session_roots;
  std::string control_dir;
  std::string default_lrms;
  std::string default_queue;
  RunPlugin* cred_plugin = new RunPlugin;
  std::string allowsubmit;
  bool strict_session;
  std::string gridftp_endpoint; // for gridftp interface
  std::string arex_endpoint; // our interface
  if(!configure_user_dirs(uname,control_dir,session_roots,
                          session_roots_non_draining_,
                          default_lrms,default_queue,queues_,
                          cont_plugins_,*cred_plugin,
                          allowsubmit,strict_session,
                          gridftp_endpoint,arex_endpoint,env)) {
    // olog<<"Failed processing grid-manager configuration"<<std::endl;
    delete user_; user_=NULL; delete cred_plugin; return;
  };
  delete cred_plugin;
  if(default_queue.empty() && (queues_.size() == 1)) {
    default_queue=*(queues_.begin());
  };
  if(!arex_endpoint.empty()) service_endpoint_ = arex_endpoint;
  user_->SetControlDir(control_dir);
  user_->SetSessionRoot(session_roots);
  user_->SetLRMS(default_lrms,default_queue);
  user_->SetStrictSession(strict_session);
  //for(;allowsubmit.length();) {
  //  std::string group = config_next_arg(allowsubmit);
  //  if(group.length()) readonly=true;
  //  if(user_a.check_group(group)) { readonly=false; break; };
  //};
  //if(readonly) olog<<"This user is denied to submit new jobs"<<std::endl;
  /*
          * link to the class for direct file access *
          std::string direct_config = "mount "+session_root+"\n";
          direct_config+="dir / nouser read cd dirlist delete append overwrite";          direct_config+=" create "+
             inttostring(user->get_uid())+":"+inttostring(user->get_gid())+
             " 600:600";
          direct_config+=" mkdir "+
             inttostring(user->get_uid())+":"+inttostring(user->get_gid())+
             " 700:700\n";
          direct_config+="end\n";
#ifdef HAVE_SSTREAM
          std::stringstream fake_cfile(direct_config);
#else
          std::strstream fake_cfile;
          fake_cfile<<direct_config;
#endif
          direct_fs = new DirectFilePlugin(fake_cfile,user_s);
          if((bool)job_map) {
            olog<<"Job submission user: "<<uname<<
                  " ("<<user->get_uid()<<":"<<user->get_gid()<<")"<<std::endl;
  */
}

template <typename T> class AutoPointer {
 private:
  T* object;
  void operator=(const AutoPointer<T>&) { };
  void operator=(T*) { };
  AutoPointer(const AutoPointer&):object(NULL) { };
 public:
  AutoPointer(void):object(NULL) { };
  AutoPointer(T* o):object(o) { }
  ~AutoPointer(void) { if(object) delete object; };
  T& operator*(void) const { return *object; };
  T* operator->(void) const { return object; };
  operator bool(void) const { return (object!=NULL); };
  bool operator!(void) const { return (object==NULL); };
  operator T*(void) const { return object; };
};

static ARexJobFailure setfail(JobReqResult res) {
  switch(res) {
    case JobReqSuccess: return ARexJobNoError;
    case JobReqInternalFailure: return ARexJobInternalError;
    case JobReqSyntaxFailure: return ARexJobDescriptionSyntaxError;
    case JobReqUnsupportedFailure: return ARexJobDescriptionUnsupportedError;
    case JobReqMissingFailure: return ARexJobDescriptionMissingError;
    case JobReqLogicalFailure: return ARexJobDescriptionLogicalError;
  };
  return ARexJobInternalError;
}

bool ARexJob::is_allowed(bool fast) {
  allowed_to_see_=false;
  allowed_to_maintain_=false;
  // Checking user's grid name against owner
  if(config_.GridName() == job_.DN) {
    allowed_to_see_=true;
    allowed_to_maintain_=true;
    return true;
  };
  if(fast) return true;
  // Do fine-grained authorization requested by job's owner
  if(config_.beginAuth() == config_.endAuth()) return true;
  std::string acl;
  if(!job_acl_read_file(id_,*config_.User(),acl)) return true; // safe to ignore
  if(acl.empty()) return true; // No policy defiled - only owner allowed
  // Identify and parse policy
  ArcSec::EvaluatorLoader eval_loader;
  AutoPointer<ArcSec::Policy> policy(eval_loader.getPolicy(ArcSec::Source(acl)));
  if(!policy) {
    logger_.msg(Arc::VERBOSE, "%s: Failed to parse user policy", id_);
    return true;
  };
  AutoPointer<ArcSec::Evaluator> eval(eval_loader.getEvaluator(policy));
  if(!eval) {
    logger_.msg(Arc::VERBOSE, "%s: Failed to load evaluator for user policy ", id_);
    return true;
  };
  std::string policyname = policy->getName();
  if((policyname.length() > 7) &&
     (policyname.substr(policyname.length()-7) == ".policy")) {
    policyname.resize(policyname.length()-7);
  };
  if(policyname == "arc") {
    // Creating request - directly with XML
    // Creating top of request document
    Arc::NS ns;
    ns["ra"]="http://www.nordugrid.org/schemas/request-arc";
    Arc::XMLNode request(ns,"ra:Request");
    // Collect all security attributes
    for(std::list<Arc::MessageAuth*>::iterator a = config_.beginAuth();a!=config_.endAuth();++a) {
      if(*a) (*a)->Export(Arc::SecAttr::ARCAuth,request);
    };
    // Leave only client identities
    for(Arc::XMLNode item = request["RequestItem"];(bool)item;++item) {
      for(Arc::XMLNode a = item["Action"];(bool)a;a=item["Action"]) a.Destroy();
      for(Arc::XMLNode r = item["Resource"];(bool)r;r=item["Resource"]) r.Destroy();
    };
    // Fix namespace
    request.Namespaces(ns);
    // Create A-Rex specific action
    // TODO: make helper classes for such operations
    Arc::XMLNode item = request["ra:RequestItem"];
    if(!item) item=request.NewChild("ra:RequestItem");
    // Possible operations are Modify and Read
    Arc::XMLNode action;
    action=item.NewChild("ra:Action");
    action=JOB_POLICY_OPERATION_READ; action.NewAttribute("Type")="string";
    action.NewAttribute("AttributeId")=JOB_POLICY_OPERATION_URN;
    action=item.NewChild("ra:Action");
    action=JOB_POLICY_OPERATION_MODIFY; action.NewAttribute("Type")="string";
    action.NewAttribute("AttributeId")=JOB_POLICY_OPERATION_URN;
    // Evaluating policy
    ArcSec::Response *resp = eval->evaluate(request,policy);
    // Analyzing response in order to understand which operations are allowed
    if(!resp) return true; // Not authorized
    // Following should be somehow made easier
    ArcSec::ResponseList& rlist = resp->getResponseItems();
    for(int n = 0; n<rlist.size(); ++n) {
      ArcSec::ResponseItem* ritem = rlist[n];
      if(!ritem) continue;
      if(ritem->res != ArcSec::DECISION_PERMIT) continue;
      if(!(ritem->reqtp)) continue;
      for(ArcSec::Action::iterator a = ritem->reqtp->act.begin();a!=ritem->reqtp->act.end();++a) {
        ArcSec::RequestAttribute* attr = *a;
        if(!attr) continue;
        ArcSec::AttributeValue* value = attr->getAttributeValue();
        if(!value) continue;
        std::string action = value->encode();
        if(action == "Read") allowed_to_see_=true;
        if(action == "Modify") allowed_to_maintain_=true;
      };
    };
  } else if(policyname == "gacl") {
    // Creating request - directly with XML
    Arc::NS ns;
    Arc::XMLNode request(ns,"gacl");
    // Collect all security attributes
    for(std::list<Arc::MessageAuth*>::iterator a = config_.beginAuth();a!=config_.endAuth();++a) {
      if(*a) (*a)->Export(Arc::SecAttr::GACL,request);
    };
    // Leave only client identities
    int entries = 0;
    for(Arc::XMLNode entry = request["entry"];(bool)entry;++entry) {
      for(Arc::XMLNode a = entry["allow"];(bool)a;a=entry["allow"]) a.Destroy();
      for(Arc::XMLNode a = entry["deny"];(bool)a;a=entry["deny"]) a.Destroy();
      ++entries;
    };
    if(!entries) request.NewChild("entry");
    // Evaluate every action separately
    for(Arc::XMLNode entry = request["entry"];(bool)entry;++entry) {
      entry.NewChild("allow").NewChild("read");
    };
    ArcSec::Response *resp;
    resp=eval->evaluate(request,policy);
    if(resp) {
      ArcSec::ResponseList& rlist = resp->getResponseItems();
      for(int n = 0; n<rlist.size(); ++n) {
        ArcSec::ResponseItem* ritem = rlist[n];
        if(!ritem) continue;
        if(ritem->res != ArcSec::DECISION_PERMIT) continue;
        allowed_to_see_=true; break;
      };
    };
    for(Arc::XMLNode entry = request["entry"];(bool)entry;++entry) {
      entry["allow"].Destroy();
      entry.NewChild("allow").NewChild("write");
    };
    resp=eval->evaluate(request,policy);
    if(resp) {
      ArcSec::ResponseList& rlist = resp->getResponseItems();
      for(int n = 0; n<rlist.size(); ++n) {
        ArcSec::ResponseItem* ritem = rlist[n];
        if(!ritem) continue;
        if(ritem->res != ArcSec::DECISION_PERMIT) continue;
        allowed_to_maintain_=true; break;
      };
    };
    // TODO: <list/>, <admin/>
  } else {
    logger_.msg(Arc::VERBOSE, "%s: Unknown user policy '%s'", id_, policyname);
  };
  return true;
}

ARexJob::ARexJob(const std::string& id,ARexGMConfig& config,Arc::Logger& logger,bool fast_auth_check):id_(id),logger_(logger),config_(config) {
  if(id_.empty()) return;
  if(!config_) { id_.clear(); return; };
  // Reading essential information about job
  if(!job_local_read_file(id_,*config_.User(),job_)) { id_.clear(); return; };
  // Checking if user is allowed to do anything with that job
  if(!is_allowed(fast_auth_check)) { id_.clear(); return; };
  if(!(allowed_to_see_ || allowed_to_maintain_)) { id_.clear(); return; };
}

ARexJob::ARexJob(Arc::XMLNode jsdl,ARexGMConfig& config,const std::string& credentials,const std::string& clientid, Arc::Logger& logger, Arc::XMLNode migration):id_(""),logger_(logger),config_(config) {
  if(!config_) return;
  // New job is created here
  // First get and acquire new id
  if(!make_job_id()) return;
  // Turn JSDL into text
  std::string job_desc_str;
  // Make full XML doc out of subtree
  {
    Arc::XMLNode jsdldoc;
    jsdl.New(jsdldoc);
    jsdldoc.GetDoc(job_desc_str);
  };
  // Store description
  std::string fname = config_.User()->ControlDir() + "/job." + id_ + ".description";
  if(!job_description_write_file(fname,job_desc_str)) {
    delete_job_id();
    failure_="Failed to store job RSL description";
    failure_type_=ARexJobInternalError;
    return;
  };
  // Analyze JSDL (checking, substituting, etc)
  std::string acl("");
  if((failure_type_=setfail(parse_job_req(fname.c_str(),job_,&acl))) != ARexJobNoError) {
    if(failure_.empty()) {
      failure_="Failed to parse job description";
      failure_type_=ARexJobInternalError;
    };
    delete_job_id();
    return;
  };
  if((!job_.action.empty()) && (job_.action != "request")) {
    failure_="Wrong action in job request: "+job_.action;
    failure_type_=ARexJobInternalError;
    delete_job_id();
    return;
  };
  // Check for proper LRMS name in request. If there is no LRMS name
  // in user configuration that means service is opaque frontend and
  // accepts any LRMS in request.
  if((!job_.lrms.empty()) && (!config_.User()->DefaultLRMS().empty())) {
    if(job_.lrms != config_.User()->DefaultLRMS()) {
      failure_="Requested LRMS is not supported by this service";
      failure_type_=ARexJobInternalError;
      //failure_type_=ARexJobDescriptionLogicalError;
      delete_job_id();
      return;
    };
  };
  if(job_.lrms.empty()) job_.lrms=config_.User()->DefaultLRMS();
  // Check for proper queue in request.
  if(job_.queue.empty()) job_.queue=config_.User()->DefaultQueue();
  if(job_.queue.empty()) {
    failure_="Request has no queue defined";
    failure_type_=ARexJobDescriptionMissingError;
    delete_job_id();
    return;
  };
  if(config_.Queues().size() > 0) { // If no queues configured - service takes any
    for(std::list<std::string>::const_iterator q = config_.Queues().begin();;++q) {
      if(q == config_.Queues().end()) {
        failure_="Requested queue "+job_.queue+" does not match any of available queues";
        //failure_type_=ARexJobDescriptionLogicalError;
        failure_type_=ARexJobInternalError;
        delete_job_id();
        return;
      };
      if(*q == job_.queue) break;
    };
  };
  // Start local file
  /* !!!!! some parameters are unchecked here - rerun,diskspace !!!!! */
  job_.jobid=id_;
  job_.starttime=Arc::Time();
  job_.DN=config_.GridName();
  job_.clientname=clientid;
  job_.migrateactivityid=(std::string)migration["ActivityIdentifier"];
  job_.forcemigration=(migration["ForceMigration"]=="true");
  // BES ActivityIdentifier is global job ID
  Arc::NS ns_;
  ns_["bes-factory"]="http://schemas.ggf.org/bes/2006/08/bes-factory";
  ns_["a-rex"]="http://www.nordugrid.org/schemas/a-rex";
  Arc::XMLNode node_(ns_,"bes-factory:ActivityIdentifier");
  Arc::WSAEndpointReference identifier(node_);
  identifier.Address(config.Endpoint()); // address of service
  identifier.ReferenceParameters().NewChild("a-rex:JobID")=id_;
  identifier.ReferenceParameters().NewChild("a-rex:JobSessionDir")=config.Endpoint()+"/"+id_;
  std::string globalid;
  ((Arc::XMLNode)identifier).GetDoc(globalid);
  std::string::size_type nlp;
  while ((nlp=globalid.find('\n')) != std::string::npos)
    globalid.replace(nlp,1," "); // squeeze into 1 line
  job_.globalid=globalid;
  // Try to create proxy
  if(!update_credentials(credentials)) {
    failure_="Failed to store credentials";
    failure_type_=ARexJobInternalError;
    delete_job_id();
    return;
  };
  // Choose session directory
  std::string sessiondir;
  if (!ChooseSessionDir(id_, sessiondir)) {
    delete_job_id();
    failure_="Failed to find valid session directory";
    failure_type_=ARexJobInternalError;
    return;
  };
  config_.User()->SetSessionRoot(sessiondir);
  // Write local file
  JobDescription job(id_,config_.User()->SessionRoot()+"/"+id_,JOB_STATE_ACCEPTED);
  job.set_local(&job_); // need this for write_grami
  if(!job_local_write_file(job,*config_.User(),job_)) {
    delete_job_id();
    failure_="Failed to create job description";
    failure_type_=ARexJobInternalError;
    return;
  };
  // Parse job description
   Arc::JobDescription desc;
  {
    std::list<Arc::JobDescription> descs;
    if (!Arc::JobDescription::Parse(job_desc_str, descs, "", "GRIDMANAGER") || descs.size() != 1) {
      delete_job_id();
      failure_="Unable to parse job description";
      failure_type_=ARexJobInternalError;
      return;
    }
    desc = descs.front();
  };
  // Write grami file
  if(!write_grami(desc,job,*config_.User(),NULL)) {
    delete_job_id();
    failure_="Failed to create grami file";
    failure_type_=ARexJobInternalError;
    return;
  };
  // Write ACL file
  if(!acl.empty()) {
    if(!job_acl_write_file(id_,*config.User(),acl)) {
      delete_job_id();
      failure_="Failed to process/store job ACL";
      failure_type_=ARexJobInternalError;
      return;
    };
  };
  // Call authentication/authorization plugin/exec
  {
    // talk to external plugin to ask if we can proceed
    std::list<ContinuationPlugins::result_t> results;
    config_.Plugins().run(job,*config_.User(),results);
    std::list<ContinuationPlugins::result_t>::iterator result = results.begin();
    while(result != results.end()) {
      // analyze results
      if(result->action == ContinuationPlugins::act_fail) {
        delete_job_id();
        failure_="Job is not allowed by external plugin: "+result->response;
        failure_type_=ARexJobInternalError;
        return;
      } else if(result->action == ContinuationPlugins::act_log) {
        // Scream but go ahead
        logger_.msg(Arc::WARNING, "Failed to run external plugin: %s", result->response);
      } else if(result->action == ContinuationPlugins::act_pass) {
        // Just continue
        if(result->response.length()) {
          logger_.msg(Arc::INFO, "Plugin response: %s", result->response);
        };
      } else {
        delete_job_id();
        failure_="Failed to pass external plugin: "+result->response;
        failure_type_=ARexJobInternalError;
        return;
      };
      ++result;
    };
  };
/*@
  // Make access to filesystem on behalf of local user
  if(cred_plugin && (*cred_plugin)) {
    job_subst_t subst_arg;
    subst_arg.user=user;
    subst_arg.job=&job_id;
    subst_arg.reason="new";
    // run external plugin to acquire non-unix local credentials
    if(!cred_plugin->run(job_subst,&subst_arg)) {
      olog << "Failed to run plugin" << std::endl;
      delete_job_id();
      failure_type_=ARexJobInternalError;
      error_description="Failed to obtain external credentials";
      return 1;
    };
    if(cred_plugin->result() != 0) {
      olog << "Plugin failed: " << cred_plugin->result() << std::endl;
      delete_job_id();
      error_description="Failed to obtain external credentials";
      failure_type_=ARexJobInternalError;
      return 1;
    };
  };
*/
  // Create session directory
  if(!job_session_create(job,*config_.User())) {
    delete_job_id();
    failure_="Failed to create session directory";
    failure_type_=ARexJobInternalError;
    return;
  };
  // Create input status file to tell downloader we
  // are hadling input in lever way.
  job_input_status_add_file(job,*config_.User());
  // Create status file (do it last so GM picks job up here)
  if(!job_state_write_file(job,*config_.User(),JOB_STATE_ACCEPTED)) {
    delete_job_id();
    failure_="Failed registering job in grid-manager";
    failure_type_=ARexJobInternalError;
    return;
  };
  SignalFIFO(*config_.User());
  return;
}

bool ARexJob::GetDescription(Arc::XMLNode& jsdl) {
  if(id_.empty()) return false;
  std::string sdesc;
  if(!job_description_read_file(id_,*config_.User(),sdesc)) return false;
  Arc::XMLNode xdesc(sdesc);
  if(!xdesc) return false;
  jsdl.Replace(xdesc);
  return true;
}

bool ARexJob::Cancel(void) {
  if(id_.empty()) return false;
  JobDescription job_desc(id_,"");
  if(!job_cancel_mark_put(job_desc,*config_.User())) return false;
  return true;
}

bool ARexJob::Clean(void) {
  if(id_.empty()) return false;
  JobDescription job_desc(id_,"");
  if(!job_clean_mark_put(job_desc,*config_.User())) return false;
  return true;
}

bool ARexJob::Resume(void) {
  if(id_.empty()) return false;
  if(job_.failedstate.length() == 0) {
    // Job can't be restarted.
    return false;
  };
  if(job_.reruns <= 0) {
    // Job run out of number of allowed retries.
    return false;
  };
  if(!job_restart_mark_put(JobDescription(id_,""),*config_.User())) {
    // Failed to report restart request.
    return false;
  };
  return true;
}

std::string ARexJob::State(void) {
  bool job_pending;
  return State(job_pending);
}

std::string ARexJob::State(bool& job_pending) {
  if(id_.empty()) return "";
  job_state_t state = job_state_read_file(id_,*config_.User(),job_pending);
  if(state > JOB_STATE_UNDEFINED) state=JOB_STATE_UNDEFINED;
  return states_all[state].name;
}

bool ARexJob::Failed(void) {
  if(id_.empty()) return false;
  return job_failed_mark_check(id_,*config_.User());
}

bool ARexJob::UpdateCredentials(const std::string& credentials) {
  if(id_.empty()) return false;
  if(!update_credentials(credentials)) return false;
  JobDescription job(id_,config_.User()->SessionRoot(id_)+"/"+id_,JOB_STATE_ACCEPTED);
  if(!job_local_write_file(job,*config_.User(),job_)) return false;
  return true;
}

bool ARexJob::update_credentials(const std::string& credentials) {
  if(credentials.empty()) return true;
  std::string fname=config_.User()->ControlDir()+"/job."+id_+".proxy";
  ::unlink(fname.c_str());
  int h=Arc::FileOpen(fname,O_WRONLY | O_CREAT | O_EXCL,0600);
  if(h == -1) return false;
  fix_file_owner(fname,*config_.User());
  const char* s = credentials.c_str();
  int ll = credentials.length();
  int l = 0;
  for(;(ll>0) && (l!=-1);s+=l,ll-=l) l=::write(h,s,ll);
  ::close(h);
  if(l==-1) return false;
  job_.expiretime = Arc::Credential(fname,"","","").GetEndTime();
  return true;
}

/*
bool ARexJob::make_job_id(const std::string &id) {
  if((id.find('/') != std::string::npos) || (id.find('\n') != std::string::npos)) {
    olog<<"ID contains forbidden characters"<<std::endl;
    return false;
  };
  if((id == "new") || (id == "info")) return false;
  // claim id by creating empty description file
  std::string fname=user->ControlDir()+"/job."+id+".description";
  struct stat st;
  if(stat(fname.c_str(),&st) == 0) return false;
  int h = Arc::FileOpen(fname,O_RDWR | O_CREAT | O_EXCL,S_IRWXU);
  // So far assume control directory is on local fs.
  // TODO: add locks or links for NFS
  if(h == -1) return false;
  fix_file_owner(fname,*user);
  close(h);
  delete_job_id();
  job_id=id;
  return true;
}
*/

bool ARexJob::make_job_id(void) {
  if(!config_) return false;
  int i;
  //@ delete_job_id();
  for(i=0;i<100;i++) {
    id_=Arc::tostring((unsigned int)getpid())+
        Arc::tostring((unsigned int)time(NULL))+
        Arc::tostring(rand(),1);
    std::string fname=config_.User()->ControlDir()+"/job."+id_+".description";
    struct stat st;
    if(stat(fname.c_str(),&st) == 0) continue;
    int h = Arc::FileOpen(fname,O_RDWR | O_CREAT | O_EXCL,0600);
    // So far assume control directory is on local fs.
    // TODO: add locks or links for NFS
    int err = errno;
    if(h == -1) {
      if(err == EEXIST) continue;
      logger_.msg(Arc::ERROR, "Failed to create file in %s", config_.User()->ControlDir());
      id_="";
      return false;
    };
    fix_file_owner(fname,*config_.User());
    close(h);
    return true;
  };
  logger_.msg(Arc::ERROR, "Out of tries while allocating new job ID in %s", config_.User()->ControlDir());
  id_="";
  return false;
}

bool ARexJob::delete_job_id(void) {
  if(!config_) return true;
  if(!id_.empty()) {
    job_clean_final(JobDescription(id_,
                config_.User()->SessionRoot(id_)+"/"+id_),*config_.User());
    id_="";
  };
  return true;
}

int ARexJob::TotalJobs(ARexGMConfig& config,Arc::Logger& /* logger */) {
  ContinuationPlugins plugins;
  JobsList jobs(*config.User(),plugins);
  jobs.ScanAllJobs();
  return jobs.size();
}

std::list<std::string> ARexJob::Jobs(ARexGMConfig& config,Arc::Logger& logger) {
  std::list<std::string> jlist;
  ContinuationPlugins plugins;
  JobsList jobs(*config.User(),plugins);
  jobs.ScanAllJobs();
  JobsList::iterator i = jobs.begin();
  for(;i!=jobs.end();++i) {
    ARexJob job(i->get_id(),config,logger,true);
    if(job) jlist.push_back(i->get_id());
  };
  return jlist;
}

std::string ARexJob::SessionDir(void) {
  if(id_.empty()) return "";
  return config_.User()->SessionRoot(id_)+"/"+id_;
}

std::string ARexJob::LogDir(void) {
  return job_.stdlog;
}

static bool normalize_filename(std::string& filename) {
  std::string::size_type p = 0;
  if(filename[0] != G_DIR_SEPARATOR) filename.insert(0,G_DIR_SEPARATOR_S);
  for(;p != std::string::npos;) {
    if((filename[p+1] == '.') &&
       (filename[p+2] == '.') &&
       ((filename[p+3] == 0) || (filename[p+3] == G_DIR_SEPARATOR))
      ) {
      std::string::size_type pr = std::string::npos;
      if(p > 0) pr = filename.rfind(G_DIR_SEPARATOR,p-1);
      if(pr == std::string::npos) return false;
      filename.erase(pr,p-pr+3);
      p=pr;
    } else if((filename[p+1] == '.') && (filename[p+2] == G_DIR_SEPARATOR)) {
      filename.erase(p,2);
    } else if(filename[p+1] == G_DIR_SEPARATOR) {
      filename.erase(p,1);
    };
    p = filename.find(G_DIR_SEPARATOR,p+1);
  };
  if(!filename.empty()) filename.erase(0,1); // removing leading separator
  return true;
}

int ARexJob::CreateFile(const std::string& filename) {
  if(id_.empty()) return -1;
  std::string fname = filename;
  if((!normalize_filename(fname)) || (fname.empty())) {
    failure_="File name is not acceptable";
    failure_type_=ARexJobInternalError;
    return -1;
  };
  int lname = fname.length();
  fname = config_.User()->SessionRoot(id_)+"/"+id_+"/"+fname;
  // First try to create/open file
  int h = Arc::FileOpen(fname,O_WRONLY | O_CREAT,config_.User()->get_uid(),config_.User()->get_gid(),S_IRUSR | S_IWUSR);
  if(h != -1) return h;
  if(errno != ENOENT) return -1;
  // If open reports missing directory - try to create all sudirectories
  std::string::size_type n = fname.length()-lname;
  for(;;) {
    n=fname.find('/',n);
    if(n == std::string::npos) break;
    std::string dname = fname.substr(0,n);
    ++n;
    if(Arc::DirCreate(dname,config_.User()->get_uid(),config_.User()->get_gid(),S_IRUSR | S_IWUSR | S_IXUSR)) continue;
    if(errno == EEXIST) continue;
  };
  return Arc::FileOpen(fname,O_WRONLY | O_CREAT,config_.User()->get_uid(),config_.User()->get_gid(),S_IRUSR | S_IWUSR);
}

int ARexJob::OpenFile(const std::string& filename,bool for_read,bool for_write) {
  if(id_.empty()) return -1;
  std::string fname = filename;
  if((!normalize_filename(fname)) || (fname.empty())) {
    failure_="File name is not acceptable";
    failure_type_=ARexJobInternalError;
    return -1;
  };
  fname = config_.User()->SessionRoot(id_)+"/"+id_+"/"+fname;
  int flags = 0;
  if(for_read && for_write) { flags=O_RDWR; }
  else if(for_read) { flags=O_RDONLY; }
  else if(for_write) { flags=O_WRONLY; }
  return Arc::FileOpen(fname,flags,config_.User()->get_uid(),config_.User()->get_gid(),0);
}

Glib::Dir* ARexJob::OpenDir(const std::string& dirname) {
  if(id_.empty()) return NULL;
  std::string dname = dirname;
  if(!normalize_filename(dname)) return NULL;
  //if(dname.empty()) return NULL;
  dname = config_.User()->SessionRoot(id_)+"/"+id_+"/"+dname;
  Glib::Dir* dir = Arc::DirOpen(dname,config_.User()->get_uid(),config_.User()->get_gid());
  return dir;
}

int ARexJob::OpenLogFile(const std::string& name) {
  if(id_.empty()) return -1;
  if(strchr(name.c_str(),'/')) return -1;
  std::string fname = config_.User()->ControlDir() + "/job." + id_ + "." + name;
  return Arc::FileOpen(fname,O_RDONLY,0,0,0);
}

std::list<std::string> ARexJob::LogFiles(void) {
  std::list<std::string> logs;
  if(id_.empty()) return logs;
  std::string dname = config_.User()->ControlDir();
  std::string prefix = "job." + id_ + ".";
  Glib::Dir* dir = Arc::DirOpen(dname,config_.User()->get_uid(),config_.User()->get_gid());
  if(!dir) return logs;
  for(;;) {
    std::string name = dir->read_name();
    if(name.empty()) break;
    if(strncmp(prefix.c_str(),name.c_str(),prefix.length()) != 0) continue;
    logs.push_back(name.substr(prefix.length()));
  };
  return logs;
}

std::string ARexJob::GetFilePath(const std::string& filename) {
  if(id_.empty()) return "";
  std::string fname = filename;
  if(!normalize_filename(fname)) return "";
  if(fname.empty()) config_.User()->SessionRoot(id_)+"/"+id_;
  return config_.User()->SessionRoot(id_)+"/"+id_+"/"+fname;
}

bool ARexJob::ReportFileComplete(const std::string& filename) {
  if(id_.empty()) return "";
  std::string fname = filename;
  if(!normalize_filename(fname)) return false;
  return job_input_status_add_file(JobDescription(id_,""),*config_.User(),"/"+fname);
}

std::string ARexJob::GetLogFilePath(const std::string& name) {
  if(id_.empty()) return "";
  return config_.User()->ControlDir() + "/job." + id_ + "." + name;
}

bool ARexJob::ChooseSessionDir(const std::string& /* jobid */, std::string& sessiondir) {
  if (config_.SessionRootsNonDraining().size() == 0) {
    // no active session dirs available
    logger_.msg(Arc::ERROR, "No non-draining session dirs available");
    return false;
  }
  // choose randomly from non-draining session dirs
  sessiondir = config_.SessionRootsNonDraining().at(rand() % config_.SessionRootsNonDraining().size());
  return true;
}
