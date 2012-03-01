// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <sstream>

#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/client/ExecutionTarget.h>
#include <arc/client/Job.h>
#include <arc/client/JobDescription.h>
#include <arc/message/MCC.h>

#include "SubmitterBES.h"
#include "AREXClient.h"

namespace Arc {

  Logger SubmitterBES::logger(Logger::getRootLogger(), "Submitter.BES");

  static std::string char_to_hex(char v) {
    std::string s;
    unsigned int n;
    n = (unsigned int)((v & 0xf0) >> 4);
    if(n < 10) { s+=(char)('0'+n); } else { s+=(char)('a'+n-10); }
    n = (unsigned int)(v & 0x0f);
    if(n < 10) { s+=(char)('0'+n); } else { s+=(char)('a'+n-10); }
    return s;
  }

  static std::string disguise_id_into_url(const URL& u,const std::string& id) {
    std::string jobid = u.str();
    jobid+="#";
    for(std::string::size_type p = 0;p < id.length();++p) {
      jobid+=char_to_hex(id[p]);
    }
    return jobid;
  }

  SubmitterBES::SubmitterBES(const UserConfig& usercfg)
    : Submitter(usercfg, "BES") {}

  SubmitterBES::~SubmitterBES() {}

  Plugin* SubmitterBES::Instance(PluginArgument *arg) {
    SubmitterPluginArgument *subarg =
      dynamic_cast<SubmitterPluginArgument*>(arg);
    if (!subarg) return NULL;
    return new SubmitterBES(*subarg);
  }

  bool SubmitterBES::Submit(const JobDescription& jobdesc,
                            const ExecutionTarget& et, Job& job) {
    URL url(et.ComputingEndpoint.URLString);
    
    MCCConfig cfg;
    usercfg.ApplyToConfig(cfg);
    AREXClient ac(url, cfg, usercfg.Timeout(), false);

    // !! TODO: ordinary JSDL is needed - keeping nordugrid:jsdl so far
    std::string jobdescstring;
    if (!jobdesc.UnParse(jobdescstring, "nordugrid:jsdl")) {
      logger.msg(INFO, "Unable to submit job. Job description is not valid in the %s format", "nordugrid:jsdl");
      return false;
    }

    std::string jobid;
    if (!ac.submit(jobdescstring, jobid, url.Protocol() == "https")) return false;

    if (jobid.empty()) {
      logger.msg(INFO, "No job identifier returned by BES service");
      return false;
    }

    //XMLNode xJobid(jobid);

    // Unfortunately Job handling framework somewhy want to have job
    // URL instead of identifier we have to invent one. So we disguise
    // XML blob inside URL path.
    AddJobDetails(jobdesc, URL(disguise_id_into_url(url,jobid)), et.Cluster, url, job);

    return true;
  }

  bool SubmitterBES::Migrate(const URL& /* jobid */, const JobDescription& /* jobdesc */,
                             const ExecutionTarget& et, bool /* forcemigration */,
                             Job& /* job */) {
    logger.msg(INFO, "Trying to migrate to %s: Migration to a BES resource is not supported.", et.ComputingEndpoint.URLString);
    return false;
  }

  bool SubmitterBES::ModifyJobDescription(JobDescription& /* jobdesc */, const ExecutionTarget& /* et */) const {
    return true;
  }

} // namespace Arc
