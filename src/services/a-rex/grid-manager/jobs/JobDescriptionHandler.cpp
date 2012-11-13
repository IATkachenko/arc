#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <fstream>

#include <glibmm.h>

#include <arc/FileUtils.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/Utils.h>
#include <arc/XMLNode.h>

#include "../files/ControlFileHandling.h"
#include "../conf/GMConfig.h"
#include "../../delegation/DelegationStore.h"
#include "../../delegation/DelegationStores.h"

#include "JobDescriptionHandler.h"

// TODO: move to using process_job_req as much as possible

namespace ARex {

Arc::Logger JobDescriptionHandler::logger(Arc::Logger::getRootLogger(), "JobDescriptionHandler");
const std::string JobDescriptionHandler::NG_RSL_DEFAULT_STDIN("/dev/null");
const std::string JobDescriptionHandler::NG_RSL_DEFAULT_STDOUT("/dev/null");
const std::string JobDescriptionHandler::NG_RSL_DEFAULT_STDERR("/dev/null");

bool JobDescriptionHandler::process_job_req(const GMJob &job,JobLocalDescription &job_desc) const {
  /* read local first to get some additional info pushed here by script */
  job_local_read_file(job.get_id(),config,job_desc);
  /* some default values */
  job_desc.lrms=config.DefaultLRMS();
  job_desc.queue=config.DefaultQueue();
  job_desc.lifetime=Arc::tostring(config.KeepFinished());
  std::string filename;
  filename = config.ControlDir() + "/job." + job.get_id() + ".description";
  if(parse_job_req(job.get_id(),job_desc) != JobReqSuccess) return false;
  if(job_desc.reruns>config.Reruns()) job_desc.reruns=config.Reruns();
  if(!job_local_write_file(job,config,job_desc)) return false;
  // Convert delegation ids to credential paths.
  // Add default credentials for file which have no own assigned.
  std::string default_cred = config.ControlDir() + "/job." + job.get_id() + ".proxy";
  for(std::list<FileData>::iterator f = job_desc.inputdata.begin();
                                   f != job_desc.inputdata.end(); ++f) {
    if(f->has_lfn()) {
      if(f->cred.empty()) {
        f->cred = default_cred;
      } else {
        std::string path;
        ARex::DelegationStores* delegs = config.Delegations();
        if(delegs) path = (*delegs)[config.DelegationDir()].FindCred(f->cred,job_desc.DN);
        f->cred = path;
      };
    };
  };
  for(std::list<FileData>::iterator f = job_desc.outputdata.begin();
                                   f != job_desc.outputdata.end(); ++f) {
    if(f->has_lfn()) {
      if(f->cred.empty()) {
        f->cred = default_cred;
      } else {
        std::string path;
        ARex::DelegationStores* delegs = config.Delegations();
        if(delegs) path = (*delegs)[config.DelegationDir()].FindCred(f->cred,job_desc.DN);
        f->cred = path;
      };
    };
  };
  if(!job_input_write_file(job,config,job_desc.inputdata)) return false;
  if(!job_output_write_file(job,config,job_desc.outputdata,job_output_success)) return false;
  return true;
}

JobReqResult JobDescriptionHandler::parse_job_req(JobLocalDescription &job_desc,Arc::JobDescription& arc_job_desc,const std::string &fname,bool check_acl) const {
  Arc::JobDescriptionResult arc_job_res = get_arc_job_description(fname, arc_job_desc);
  if (!arc_job_res) {
    std::string failure = arc_job_res.str();
    if(failure.empty()) failure = "Unable to read or parse job description.";
    return JobReqResult(JobReqInternalFailure, "", failure);
  }

  if (!arc_job_desc.Resources.RunTimeEnvironment.isResolved()) {
    return JobReqResult(JobReqInternalFailure, "", "Runtime environments have not been resolved.");
  }

  job_desc = arc_job_desc;

  if (check_acl) return get_acl(arc_job_desc);
  return JobReqSuccess;
}

JobReqResult JobDescriptionHandler::parse_job_req(const JobId &job_id,JobLocalDescription &job_desc,bool check_acl) const {
  Arc::JobDescription arc_job_desc;
  return parse_job_req(job_id,job_desc,arc_job_desc,check_acl);
}

JobReqResult JobDescriptionHandler::parse_job_req(const JobId &job_id,JobLocalDescription &job_desc,Arc::JobDescription& arc_job_desc,bool check_acl) const {
  std::string fname = config.ControlDir() + "/job." + job_id + ".description";
  return parse_job_req(job_desc,arc_job_desc,fname,check_acl);
}

std::string JobDescriptionHandler::get_local_id(const JobId &job_id) const {
  const char* local_id_param = "joboption_jobid=";
  int l = strlen(local_id_param);
  std::string id = "";
  std::string fgrami = config.ControlDir() + "/job." + job_id + ".grami";
  std::ifstream f(fgrami.c_str());
  if(!f.is_open()) return id;
  for(;!(f.eof() || f.fail());) {
    std::string buf;
    std::getline(f,buf);
    Arc::trim(buf," \t\r\n");
    if(strncmp(local_id_param,buf.c_str(),l)) continue;
    if(buf[l]=='\'') {
      l++; int ll = buf.length();
      if(buf[ll-1]=='\'') buf.resize(ll-1);
    };
    id=buf.substr(l); break;
  };
  f.close();
  return id;
}

bool JobDescriptionHandler::write_grami_executable(std::ofstream& f, const std::string& name, const Arc::ExecutableType& exec) const {
  std::string executable = Arc::trim(exec.Path);
  if (executable[0] != '/' && executable[0] != '$' && !(executable[0] == '.' && executable[1] == '/')) executable = "./"+executable;
  f<<"joboption_"<<name<<"_0"<<"="<<value_for_shell(executable.c_str(),true)<<std::endl;
  int i = 1;
  for (std::list<std::string>::const_iterator it = exec.Argument.begin();
       it != exec.Argument.end(); it++, i++) {
    f<<"joboption_"<<name<<"_"<<i<<"="<<value_for_shell(it->c_str(),true)<<std::endl;
  }
  if(exec.SuccessExitCode.first) {
    f<<"joboption_"<<name<<"_code"<<"="<<Arc::tostring(exec.SuccessExitCode.second)<<std::endl;
  }
  return true;
}

bool JobDescriptionHandler::write_grami(const GMJob &job,const char *opt_add) const {
  const std::string fname = config.ControlDir() + "/job." + job.get_id() + ".description";

  Arc::JobDescription arc_job_desc;
  if (!get_arc_job_description(fname, arc_job_desc)) return false;

  return write_grami(arc_job_desc, job, opt_add);
}

bool JobDescriptionHandler::write_grami(const Arc::JobDescription& arc_job_desc, const GMJob& job, const char* opt_add) const {
  if(job.get_local() == NULL) return false;
  const std::string session_dir = job.SessionDir();
  const std::string control_dir = config.ControlDir();
  JobLocalDescription& job_local_desc = *(job.get_local());
  const std::string fgrami = control_dir + "/job." + job.get_id() + ".grami";
  std::ofstream f(fgrami.c_str(),std::ios::out | std::ios::trunc);
  if(!f.is_open()) return false;
  if(!fix_file_permissions(fgrami,job,config)) return false;
  if(!fix_file_owner(fgrami,job)) return false;

  f<<"joboption_directory='"<<session_dir<<"'"<<std::endl;
  f<<"joboption_controldir='"<<control_dir<<"'"<<std::endl;

  if(!write_grami_executable(f,"arg",arc_job_desc.Application.Executable)) return false;
  int n = 0;
  for(std::list<Arc::ExecutableType>::const_iterator e =
                     arc_job_desc.Application.PreExecutable.begin();
                     e != arc_job_desc.Application.PreExecutable.end(); ++e) {
    if(!write_grami_executable(f,"pre_"+Arc::tostring(n),*e)) return false;
    ++n;
  }
  for(std::list<Arc::ExecutableType>::const_iterator e =
                     arc_job_desc.Application.PostExecutable.begin();
                     e != arc_job_desc.Application.PostExecutable.end(); ++e) {
    if(!write_grami_executable(f,"post_"+Arc::tostring(n),*e)) return false;
  }

  f<<"joboption_stdin="<<value_for_shell(arc_job_desc.Application.Input.empty()?NG_RSL_DEFAULT_STDIN:arc_job_desc.Application.Input,true)<<std::endl;

  if (!arc_job_desc.Application.Output.empty()) {
    std::string output = arc_job_desc.Application.Output;
    if (!Arc::CanonicalDir(output)) {
      logger.msg(Arc::ERROR,"Bad name for stdout: %s", output);
      return false;
    }
  }
  f<<"joboption_stdout="<<value_for_shell(arc_job_desc.Application.Output.empty()?NG_RSL_DEFAULT_STDOUT:session_dir+"/"+arc_job_desc.Application.Output,true)<<std::endl;
  if (!arc_job_desc.Application.Error.empty()) {
    std::string error = arc_job_desc.Application.Error;
    if (!Arc::CanonicalDir(error)) {
      logger.msg(Arc::ERROR,"Bad name for stderr: %s", error);
      return false;
    }
  }
  f<<"joboption_stderr="<<value_for_shell(arc_job_desc.Application.Error.empty()?NG_RSL_DEFAULT_STDERR:session_dir+"/"+arc_job_desc.Application.Error,true)<<std::endl;

  {
    int i = 0;
    for (std::list< std::pair<std::string, std::string> >::const_iterator it = arc_job_desc.Application.Environment.begin();
         it != arc_job_desc.Application.Environment.end(); it++, i++) {
        f<<"joboption_env_"<<i<<"="<<value_for_shell(it->first+"="+it->second,true)<<std::endl;
    }
    value_for_shell globalid=value_for_shell(job_local_desc.globalid,true);
    f<<"joboption_env_"<<i<<"=GRID_GLOBAL_JOBID="<<globalid<<std::endl;
  }


  f<<"joboption_cputime="<<(arc_job_desc.Resources.TotalCPUTime.range.max != -1 ? Arc::tostring(arc_job_desc.Resources.TotalCPUTime.range.max):"")<<std::endl;
  f<<"joboption_walltime="<<(arc_job_desc.Resources.TotalWallTime.range.max != -1 ? Arc::tostring(arc_job_desc.Resources.TotalWallTime.range.max):"")<<std::endl;
  f<<"joboption_memory="<<(arc_job_desc.Resources.IndividualPhysicalMemory.max != -1 ? Arc::tostring(arc_job_desc.Resources.IndividualPhysicalMemory.max):"")<<std::endl;
  f<<"joboption_count="<<(arc_job_desc.Resources.SlotRequirement.NumberOfSlots != -1 ? Arc::tostring(arc_job_desc.Resources.SlotRequirement.NumberOfSlots):"1")<<std::endl;
  f<<"joboption_countpernode="<<Arc::tostring(arc_job_desc.Resources.SlotRequirement.SlotsPerHost)<<std::endl;
  f<<"joboption_exclusivenode="<<Arc::tostring(arc_job_desc.Resources.SlotRequirement.ExclusiveExecution)<<std::endl;


  {
    int i = 0;
    for (std::list<Arc::Software>::const_iterator itSW = arc_job_desc.Resources.RunTimeEnvironment.getSoftwareList().begin();
         itSW != arc_job_desc.Resources.RunTimeEnvironment.getSoftwareList().end(); itSW++) {
      if (itSW->empty()) continue;
      std::string rte = Arc::upper(*itSW);
      if (!Arc::CanonicalDir(rte)) {
        logger.msg(Arc::ERROR, "Bad name for runtime environment: %s", (std::string)*itSW);
        return false;
      }
      f<<"joboption_runtime_"<<i<<"="<<value_for_shell((std::string)*itSW,true)<<std::endl;
      const std::list<std::string>& opts = itSW->getOptions();
      int n = 1;
      for(std::list<std::string>::const_iterator opt = opts.begin();
                            opt != opts.end();++opt) {
        f<<"joboption_runtime_"<<i<<"_"<<n<<"="<<value_for_shell(*opt,true)<<std::endl;
        ++n;
      }
      ++i;
    }
  }
  f<<"joboption_jobname="<<value_for_shell(job_local_desc.jobname,true)<<std::endl;
  f<<"joboption_queue="<<value_for_shell(job_local_desc.queue,true)<<std::endl;
  f<<"joboption_starttime="<<(job_local_desc.exectime != -1?job_local_desc.exectime.str(Arc::MDSTime):"")<<std::endl;
  f<<"joboption_gridid="<<value_for_shell(job.get_id(),true)<<std::endl;

  // Here we need another 'local' description because some info is not
  // stored in job.#.local and still we do not want to mix both.
  // TODO: clean this.
  {
    JobLocalDescription stageinfo;
    stageinfo = arc_job_desc;
    int i = 0;
    for(FileData::iterator s=stageinfo.inputdata.begin();
                           s!=stageinfo.inputdata.end(); ++s) {
      f<<"joboption_inputfile_"<<(i++)<<"="<<value_for_shell(s->pfn,true)<<std::endl;
    }
    i = 0;
    for(FileData::iterator s=stageinfo.outputdata.begin();
                           s!=stageinfo.outputdata.end(); ++s) {
      f<<"joboption_outputfile_"<<(i++)<<"="<<value_for_shell(s->pfn,true)<<std::endl;
    }
  }
  if(opt_add) f<<opt_add<<std::endl;

  return true;
}

Arc::JobDescriptionResult JobDescriptionHandler::get_arc_job_description(const std::string& fname, Arc::JobDescription& desc) const {
  std::string job_desc_str;
  if (!job_description_read_file(fname, job_desc_str)) {
    logger.msg(Arc::ERROR, "Job description file could not be read.");
    return false;
  }

  std::list<Arc::JobDescription> descs;
  Arc::JobDescriptionResult r = Arc::JobDescription::Parse(job_desc_str, descs, "", "GRIDMANAGER");
  if (r) {
    if(descs.size() == 1) {
      desc = descs.front();
    } else {
      r = Arc::JobDescriptionResult(false,"Multiple job descriptions not supported");
    }
  }
  return r;
}

JobReqResult JobDescriptionHandler::get_acl(const Arc::JobDescription& arc_job_desc) const {
  if( !arc_job_desc.Application.AccessControl ) return JobReqSuccess;
  Arc::XMLNode typeNode = arc_job_desc.Application.AccessControl["Type"];
  Arc::XMLNode contentNode = arc_job_desc.Application.AccessControl["Content"];
  if( !contentNode ) {
    std::string failure = "acl element wrongly formated - missing Content element";
    logger.msg(Arc::ERROR, failure);
    return JobReqResult(JobReqMissingFailure, "", failure);
  };
  if( (!typeNode) || ( ( (std::string) typeNode ) == "GACL" ) || ( ( (std::string) typeNode ) == "ARC" ) ) {
    std::string str_content;
    if(contentNode.Size() > 0) {
      Arc::XMLNode acl_doc;
      contentNode.Child().New(acl_doc);
      acl_doc.GetDoc(str_content);
    } else {
      str_content = (std::string)contentNode;
    }
    return JobReqResult(JobReqSuccess, str_content);
  }
  std::string failure = "ARC: unsupported ACL type specified: " + (std::string)typeNode;
  logger.msg(Arc::ERROR, "%s", failure);
  return JobReqResult(JobReqUnsupportedFailure, "", failure);
}

/* parse job description and set specified file permissions to executable */
bool JobDescriptionHandler::set_execs(const GMJob &job) const {
  std::string fname = config.ControlDir() + "/job." + job.get_id() + ".description";
  Arc::JobDescription desc;
  if (!get_arc_job_description(fname, desc)) return false;

  std::string session_dir = job.SessionDir();
  if (desc.Application.Executable.Path[0] != '/' && desc.Application.Executable.Path[0] != '$') {
    std::string executable = desc.Application.Executable.Path;
    if(!Arc::CanonicalDir(executable)) {
      logger.msg(Arc::ERROR, "Bad name for executable: ", executable);
      return false;
    }
    fix_file_permissions_in_session(session_dir+"/"+executable,job,config,true);
  }

  // TOOD: Support for PreExecutable and PostExecutable

  for(std::list<Arc::InputFileType>::const_iterator it = desc.DataStaging.InputFiles.begin();
      it!=desc.DataStaging.InputFiles.end();it++) {
    if(it->IsExecutable) {
      std::string executable = it->Name;
      if (executable[0] != '/' && executable[0] != '.' && executable[1] != '/') executable = "./"+executable;
      if(!Arc::CanonicalDir(executable)) {
        logger.msg(Arc::ERROR, "Bad name for executable: %s", executable);
        return false;
      }
      fix_file_permissions_in_session(session_dir+"/"+executable,job,config,true);
    }
  }

  return true;
}

std::ostream& operator<<(std::ostream& o, const JobDescriptionHandler::value_for_shell& s) {
  if(s.str == NULL) return o;
  if(s.quote) o<<"'";
  const char* p = s.str;
  for(;;) {
    const char* pp = strchr(p,'\'');
    if(pp == NULL) { o<<p; if(s.quote) o<<"'"; break; };
    o.write(p,pp-p); o<<"'\\''"; p=pp+1;
  };
  return o;
}

} // namespace ARex
