// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_SUBMITTERPLUGIN_H__
#define __ARC_SUBMITTERPLUGIN_H__

#include <list>
#include <map>
#include <string>

#include <arc/URL.h>
#include <arc/loader/Loader.h>
#include <arc/loader/Plugin.h>
#include <arc/compute/EntityRetriever.h>
#include <arc/compute/Job.h>
#include <arc/compute/JobDescription.h>
#include <arc/compute/SubmissionStatus.h>
#include <arc/data/DataHandle.h>

namespace Arc {

  class Config;
  class ExecutionTarget;
  class JobDescription;
  class Logger;
  class UserConfig;

  /// Base class for the SubmitterPlugins
  /**
   * SubmitterPlugin is the base class for Grid middleware specialized
   * SubmitterPlugin objects. The class submits job(s) to the computing
   * resource it represents and uploads (needed by the job) local
   * input files.
   */
  class SubmitterPlugin : public Plugin {
  protected:
    SubmitterPlugin(const UserConfig& usercfg, PluginArgument* parg)
      : Plugin(parg), usercfg(usercfg), dest_handle(NULL) {}
  public:
    virtual ~SubmitterPlugin() { delete dest_handle; }

    /// Submit a single job description
    /**
     * Convenience method for submitting single job description, it simply calls
     * the SubmitterPlugin::Submit method taking a list of job descriptions.
     * \param j JobDescription object to be submitted.
     * \param et ExecutionTarget to submit the job description to.
     * \param jc callback object used to add Job object of newly submitted job
     *        to.
     * \return a bool indicating whether job submission suceeded or not.
     **/
    virtual SubmissionStatus Submit(const JobDescription& j, const ExecutionTarget& et, EntityConsumer<Job>& jc) { std::list<const JobDescription*> ns; return Submit(std::list<JobDescription>(1, j), et, jc, ns); }

    /// Submit job
    /**
     * This virtual method should be overridden by plugins which should
     * be capable of submitting jobs, defined in the JobDescription
     * jobdesc, to the ExecutionTarget et. The protected convenience
     * method AddJob can be used to save job information.
     * This method should return the URL of the submitted job. In case
     * submission fails an empty URL should be returned.
     */
    virtual SubmissionStatus Submit(const std::list<JobDescription>& jobdesc,
                                    const ExecutionTarget& et,
                                    EntityConsumer<Job>& jc,
                                    std::list<const JobDescription*>& notSubmitted) = 0;
    virtual SubmissionStatus Submit(const std::list<JobDescription>& jobdesc,
                                    const std::string& endpoint,
                                    EntityConsumer<Job>& jc,
                                    std::list<const JobDescription*>& notSubmitted);


    /// Migrate job
    /**
     * This virtual method should be overridden by plugins which should
     * be capable of migrating jobs. The active job which should be
     * migrated is pointed to by the URL jobid, and is represented by
     * the JobDescription jobdesc. The forcemigration boolean specifies
     * if the migration should succeed if the active job cannot be
     * terminated. The protected method AddJob can be used to save job
     * information.
     * This method should return the URL of the migrated job. In case
     * migration fails an empty URL should be returned.
     */
    virtual bool Migrate(const std::string& jobid, const JobDescription& jobdesc,
                         const ExecutionTarget& et,
                         bool forcemigration, Job& job);

    virtual const std::list<std::string>& SupportedInterfaces() const { return supportedInterfaces; };

  protected:
    bool PutFiles(const JobDescription& jobdesc, const URL& url) const;
    void AddJobDetails(const JobDescription& jobdesc, Job& job) const;

    const UserConfig& usercfg;
    std::list<std::string> supportedInterfaces;
    DataHandle* dest_handle;

    static Logger logger;
  };

  /** Class responsible for loading SubmitterPlugin plugins
   * The SubmitterPlugin objects returned by a SubmitterPluginLoader
   * must not be used after the SubmitterPluginLoader is destroyed.
   */
  class SubmitterPluginLoader : public Loader {
  public:
    /** Constructor
     * Creates a new SubmitterPluginLoader.
     */
    SubmitterPluginLoader();

    /** Destructor
     * Calling the destructor destroys all SubmitterPlugins loaded
     * by the SubmitterPluginLoader instance.
     */
    ~SubmitterPluginLoader();

    /** Load a new SubmitterPlugin
     * \param name    The name of the SubmitterPlugin to load.
     * \param usercfg The UserConfig object for the new SubmitterPlugin.
     * \return A pointer to the new SubmitterPlugin (NULL on error).
     */
    SubmitterPlugin* load(const std::string& name, const UserConfig& usercfg);

    SubmitterPlugin* loadByInterfaceName(const std::string& name, const UserConfig& usercfg);

    void initialiseInterfacePluginMap(const UserConfig& uc);
    const std::map<std::string, std::string>& getInterfacePluginMap() const { return interfacePluginMap; }

  private:
    std::multimap<std::string, SubmitterPlugin*> submitters;
    static std::map<std::string, std::string> interfacePluginMap;
  };

  class SubmitterPluginArgument
    : public PluginArgument {
  public:
    SubmitterPluginArgument(const UserConfig& usercfg)
      : usercfg(usercfg) {}
    ~SubmitterPluginArgument() {}
    operator const UserConfig&() {
      return usercfg;
    }
  private:
    const UserConfig& usercfg;
  };

} // namespace Arc

#endif // __ARC_SUBMITTERPLUGIN_H__
