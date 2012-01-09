// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOBCONTROLLERARC0_H__
#define __ARC_JOBCONTROLLERARC0_H__

#include <arc/client/JobController.h>

namespace Arc {

  class URL;

  class JobControllerARC0
    : public JobController {

  private:
    JobControllerARC0(const UserConfig& usercfg);
  public:
    ~JobControllerARC0();

    void GetJobInformation();
    static Plugin* Instance(PluginArgument *arg);

  private:
    bool RetrieveJob(const Job& job, const std::string& downloaddir, bool usejobname, bool force);
    bool CleanJob(const Job& job);
    bool CancelJob(const Job& job);
    bool RenewJob(const Job& job);
    bool ResumeJob(const Job& job);
    URL GetFileUrlForJob(const Job& job, const std::string& whichfile) const;
    bool GetJobDescription(const Job& job, std::string& desc_str);
    URL CreateURL(std::string service, ServiceType st);

    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_JOBCONTROLLERARC0_H__
