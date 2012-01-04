// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOBCONTROLLER_H__
#define __ARC_JOBCONTROLLER_H__

#include <list>
#include <vector>
#include <string>

#include <arc/client/TargetGenerator.h>
#include <arc/ArcConfig.h>
#include <arc/URL.h>
#include <arc/client/Job.h>
#include <arc/loader/Plugin.h>
#include <arc/loader/Loader.h>
#include <arc/data/DataHandle.h>

namespace Arc {

  class Broker;
  class Logger;
  class UserConfig;

  //! Base class for the JobControllers
  /** The JobController is the base class for middleware specialized
   *  derived classes. The JobController base class is also the
   *  implementer of all public functionality that should be offered
   *  by the middleware specific specializations. In other words all
   *  virtual functions of the JobController are private. The
   *  initialization of a (specialized) JobController object takes two
   *  steps. First the JobController specialization for the required
   *  grid flavour must be loaded by the JobControllerLoader, which
   *  sees to that the JobController receives information about its
   *  Grid flavour and the local joblist file containing information
   *  about all active jobs (flavour independent). The next step is
   *  the filling of the JobController job pool (JobStore) which is
   *  the pool of jobs that the JobController can manage.
  **/
  /// Must be specialiced for each supported middleware flavour.
  class JobController
    : public Plugin {
    friend class JobSupervisor;
  protected:
    JobController(const UserConfig& usercfg,
                  const std::string& flavour);
  public:
    virtual ~JobController();

    /// Fill jobstore
    bool FillJobStore(const Job& job);

    bool Get(const std::list<std::string>& status,
             const std::string& downloaddir,
             bool keep,
             bool usejobname,
             bool force = true);

    /// Catenate a output log-file to a std::ostream object
    /**
     * The method catenates one of the log-files standard out or error, or the
     * job log file from the CE for each of the jobs contained in this object.
     * A file can only be catenated if the location
     * relative to the session directory are set in Job::StdOut, Job::StdErr and
     * Job::LogDir respectively, and if supported so in the specialised ACC
     * module. If the status parameter is non-empty only jobs having a job
     * status specified in this list will considered. The whichfile parameter
     * specifies what log-file to catenate. Possible values are "stdout",
     * "stderr" and "joblog" respectively specifying standard out, error and job
     * log file.
     *
     * This method is not supposed to be overloaded by extending
     * classes.
     *
     * @param status a list of strings representing states to be
     *        considered.
     * @param longlist a boolean indicating whether verbose job
     *        information should be printed.
     * @return This method always returns true.
     * @see SaveJobStatusToStream
     * @see GetJobInformation
     * @see JobState
     **/
    bool Cat(std::ostream& out,
             const std::list<std::string>& status,
             const std::string& whichfile);

    /// Print job status to a std::ostream object
    /**
     * The job status is printed to a std::ostream object when calling this
     * method. More specifically the Job::SaveToStream method is called on each
     * of the  Job objects stored in this object, and the boolean argument
     * \a longlist is passed directly to the method indicating whether
     * verbose job status should be printed. The \a status argument is
     * a list of strings each representing a job state (JobState) which
     * is used to indicate that only jobs with a job state in the list
     * should be considered. If the list \a status is empty all jobs
     * will be considered.
     *
     * This method is not supposed to be overloaded by extending
     * classes.
     *
     * @param out a std::ostream object to direct job status information to.
     * @param status a list of strings representing states to be
     *        considered.
     * @param longlist a boolean indicating whether verbose job
     *        information should be printed.
     * @return This method always returns true.
     * @see GetJobInformation
     * @see Job::SaveToStream
     * @see JobState
     **/
    bool SaveJobStatusToStream(std::ostream& out,
                               const std::list<std::string>& status,
                               bool longlist);

    bool Renew(const std::list<std::string>& status);

    bool Resume(const std::list<std::string>& status);

    std::list<std::string> GetDownloadFiles(const URL& dir);
    bool ARCCopyFile(const URL& src, const URL& dst);

    std::list<Job> GetJobDescriptions(const std::list<std::string>& status,
                                      bool getlocal);

    void FetchJobs(const std::list<std::string>& status, std::vector<const Job*>& jobs);

    const std::list<Job>& GetJobs() const {
      return jobstore;
    }

    // Implemented by specialized classes
    virtual void GetJobInformation() = 0;
    virtual bool GetJob(const Job& job, const std::string& downloaddir,
                        bool usejobname, bool force) = 0;
    virtual bool CleanJob(const Job& job) = 0;
    virtual bool CancelJob(const Job& job) = 0;
    virtual bool RenewJob(const Job& job) = 0;
    virtual bool ResumeJob(const Job& job) = 0;
    virtual URL GetFileUrlForJob(const Job& job,
                                 const std::string& whichfile) = 0;
    virtual bool GetJobDescription(const Job& job, std::string& desc_str) = 0;
    virtual URL CreateURL(std::string service, ServiceType st) = 0;

  protected:
    const std::string flavour;
    const UserConfig& usercfg;
    std::list<Job> jobstore;
    DataHandle* data_source;
    DataHandle* data_destination;
    static Logger logger;
  };

  //! Class responsible for loading JobController plugins
  /// The JobController objects returned by a JobControllerLoader
  /// must not be used after the JobControllerLoader goes out of scope.
  class JobControllerLoader
    : public Loader {

  public:
    //! Constructor
    /// Creates a new JobControllerLoader.
    JobControllerLoader();

    //! Destructor
    /// Calling the destructor destroys all JobControllers loaded
    /// by the JobControllerLoader instance.
    ~JobControllerLoader();

    //! Load a new JobController
    /// \param name    The name of the JobController to load.
    /// \param usercfg The UserConfig object for the new JobController.
    /// \returns       A pointer to the new JobController (NULL on error).
    JobController* load(const std::string& name, const UserConfig& usercfg);

    //! Retrieve the list of loaded JobControllers.
    /// \returns A reference to the list of JobControllers.
    const std::list<JobController*>& GetJobControllers() const {
      return jobcontrollers;
    }

  private:
    std::list<JobController*> jobcontrollers;
  };

  class JobControllerPluginArgument
    : public PluginArgument {
  public:
    JobControllerPluginArgument(const UserConfig& usercfg)
      : usercfg(usercfg) {}
    ~JobControllerPluginArgument() {}
    operator const UserConfig&() {
      return usercfg;
    }
  private:
    const UserConfig& usercfg;
  };

} // namespace Arc

#endif // __ARC_JOBCONTROLLER_H__
