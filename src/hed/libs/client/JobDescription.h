#ifndef __ARC_JOBDESCRIPTION_H__
#define __ARC_JOBDESCRIPTION_H__

#include <list>
#include <vector>
#include <string>

#include <arc/DateTime.h>
#include <arc/XMLNode.h>
#include <arc/URL.h>
#include <arc/client/Software.h>


namespace Arc {

  class JobDescriptionParserLoader;

  template<class T>
  class Range {
  public:
    Range<T>() : min(0), max(0) {}
    Range<T>(const T& t) : min(t), max(t) {}
    operator T(void) const { return max; }

    Range<T>& operator=(const Range<T>& t) { min = t.min; max = t.max; return *this; };
    Range<T>& operator=(const T& t) { max = t; return *this; };

    T min;
    T max;
  };

  template<class T>
  class ScalableTime {
  public:
    ScalableTime<T>() : benchmark("", -1.) {}
    ScalableTime<T>(const T& t) : range(t) {}

    std::pair<std::string, double> benchmark;
    Range<T> range;
  };

  template<>
  class ScalableTime<int> {
  public:
    ScalableTime<int>() : benchmark("", -1.) {}
    ScalableTime<int>(const int& t) : range(t) {}

    std::pair<std::string, double> benchmark;
    Range<int> range;

    int scaleMin(double s) const { return (int)(range.min*benchmark.second/s); }
    int scaleMax(double s) const { return (int)(range.max*benchmark.second/s); }
  };



  /// Job identification
  /**
   * This class serves to provide human readable information about a job
   * description. Some of this information might also be passed to the
   * execution service for providing information about the job created from
   * this job description. An object of this class is part of the
   * JobDescription class as the Identification public member.
   **/
  class JobIdentificationType {
  public:
    JobIdentificationType() :
      JobName(""), Description(""), Type("") {}
    /// Name of job
    /**
     * The JobName string is used to specify a name of the job description, and
     * it will most likely also be the name given to the job when created at the
     * execution service.
     **/
    std::string JobName;

    /// Human readable description
    /**
     * The Description string can be used to provide a human readable
     * description of e.g. the task which should be performed when processing
     * the job description.
     **/
    std::string Description;

    /// Job type
    /**
     * The Type string specifies a classification of the activity in
     * compliance with GLUE2. The possible values should follow those defined in
     * the ComputingActivityType_t enumeration of GLUE2.
     **/
    std::string Type;

    /// Annotation
    /**
     * The Annotation list is used for human readable comments, tags for free
     * grouping or identifying different activities.
     **/
    std::list<std::string> Annotation;

    /// ID of old activity
    /**
     * The ActivityOldID object is used to store a list of IDs corresponding to
     * activities which were performed from this job description. This
     * information is not intended to used by the execution service, but rather
     * used for keeping track of activities, e.g. when doing a job resubmission
     * the old activity ID is appended to this list.
     **/
    std::list<std::string> ActivityOldID;
  };

  /// Executable
  /**
   * The ExecutableType class is used to specify path to an executable,
   * arguments to pass to it when invoked and the exit code for successful
   * execution.
   *
   * NOTE: The Name string member has been renamed to Path.
   **/
  class ExecutableType {
  public:
    ExecutableType() : Path(""), SuccessExitCode(false, 0) {}

    /// Path to executable
    /**
     * The Path string should specify the path to an executable. Note that some
     * implementations might only accept a relative path, while others might
     * also accept a absolute one.
     **/
    std::string Path;

    /// List of arguments to executable
    /**
     * The Argument list is used to specify arguments which should be passed to
     * the executable upon invocation.
     **/
    std::list<std::string> Argument;

    /// Exit code at successful execution
    /**
     * The SuccessExitCode pair is used to specify the exit code returned by the
     * executable in case of successful execution. For some scenarios the exit
     * code returned by the executable should be ignored, which is specified by
     * setting the first member of this object to false. If the exit code should
     * be used for validation at the execution service, the first member should
     * be set to true, while the second member should be the exit code returned
     * at successful execution.
     **/
    std::pair<bool, int> SuccessExitCode;
  };

  class NotificationType {
  public:
    NotificationType() {}
    std::string Email;
    std::list<std::string> States;
  };

  class ApplicationType {
  public:
    ApplicationType() :
      Rerun(-1),
      ExpiryTime(-1),
      ProcessingStartTime(-1),
      Priority (-1),
      DryRun(false)
      {}
    /// Main executable to be run
    /**
     * The Executable object specifies the main executable which should be run
     * by the job created by the job description enclosing this object. Note
     * that in some job description languages specifying a main executable is
     * not essential.
     **/
    ExecutableType Executable;

    std::string Input;
    std::string Output;
    std::string Error;
    std::list< std::pair<std::string, std::string> > Environment;

    /// Executables to be run before the main executable
    /**
     * The PreExecutable object specifies a number of executables which should
     * be executed before invoking the main application, where the main
     * application is either the main executable (Executable) or the specified
     * run time environment (RunTimeEnvironment in the ResourcesType class).
     **/
    std::list<ExecutableType> PreExecutable;

    /// Executables to be run after the main executable
    /**
     * The PostExecutable object specifies a number of executables which should
     * be executed after invoking the main application, where the main
     * application is either the main executable (Executable) or the specified
     * run time environment (RunTimeEnvironment in the ResourcesType class).
     **/
    std::list<ExecutableType> PostExecutable;

    std::string LogDir;
    std::list<URL> RemoteLogging;
    int Rerun;
    Time ExpiryTime;
    Time ProcessingStartTime;
    int Priority;
    std::list<NotificationType> Notification;
    std::list<URL> CredentialService;
    XMLNode AccessControl;
    bool DryRun;
  };

  class ResourceSlotType {
  public:
    ResourceSlotType() :
      NumberOfSlots(-1),
      ProcessPerHost(-1),
      ThreadsPerProcesses(-1) {}
    Range<int> NumberOfSlots;
    Range<int> ProcessPerHost;
    Range<int> ThreadsPerProcesses;
    std::string SPMDVariation;
  };

  class DiskSpaceRequirementType {
  public:
    DiskSpaceRequirementType() :
      DiskSpace(-1),
      CacheDiskSpace(-1),
      SessionDiskSpace(-1) {}
    /** Specifies the required size of disk space which must be available to
     * the job in mega-bytes (MB). A negative value undefines this attribute
     */
    Range<int> DiskSpace;
    /** Specifies the required size of cache which must be available
     * to the job in mega-bytes (MB). A negative value undefines this
     * attribute
     */
    int CacheDiskSpace;
    /** Specifies the required size of job session disk space which must be
     * available to the job in mega-byte (MB). A negative value undefines
     * this attribute.
     */
    int SessionDiskSpace;
  };

  enum SessionDirectoryAccessMode {
    SDAM_NONE = 0,
    SDAM_RO = 1,
    SDAM_RW = 2
  };

  enum NodeAccessType {
    NAT_NONE = 0,
    NAT_INBOUND = 1,
    NAT_OUTBOUND = 2,
    NAT_INOUTBOUND = 3
  };

  class ResourcesType {
  public:
    ResourcesType() :
      IndividualPhysicalMemory(-1),
      IndividualVirtualMemory(-1),
      SessionLifeTime(-1),
      SessionDirectoryAccess(SDAM_NONE),
      IndividualCPUTime(-1),
      TotalCPUTime(-1),
      IndividualWallTime(-1),
      TotalWallTime(-1),
      NodeAccess(NAT_NONE) {}
    SoftwareRequirement OperatingSystem;
    std::string Platform;
    std::string NetworkInfo;
    Range<int> IndividualPhysicalMemory;
    Range<int> IndividualVirtualMemory;
    DiskSpaceRequirementType DiskSpaceRequirement;
    Period SessionLifeTime;
    SessionDirectoryAccessMode SessionDirectoryAccess;
    ScalableTime<int> IndividualCPUTime;
    ScalableTime<int> TotalCPUTime;
    ScalableTime<int> IndividualWallTime;
    ScalableTime<int> TotalWallTime;
    NodeAccessType NodeAccess;
    SoftwareRequirement CEType;
    ResourceSlotType SlotRequirement;
    std::string QueueName;
    SoftwareRequirement RunTimeEnvironment;
  };

  class FileType {
  public:
    FileType() :
      KeepData(false),
      IsExecutable(false),
      FileSize(-1),
      Checksum("") {}
    std::string Name;
    bool KeepData;
    bool IsExecutable;
    std::list<URL> Source;
    std::list<URL> Target;
    std::string DelegationID;
    long FileSize;
    /// MD5 checksum of file
    /**
     * The Checksum attribute specifies the textural representation of MD5
     * checksum of file in base 16.
     **/
    std::string Checksum;
  };

  /**
   * The JobDescription class is the internal representation of a job description
   * in the ARC-lib. It is structured into a number of other classes/objects which
   * should strictly follow the description given in the job description document
   * <http://svn.nordugrid.org/trac/nordugrid/browser/arc1/trunk/doc/tech_doc/client/job_description.odt>.
   *
   * The class consist of a parsing method JobDescription::Parse which tries to
   * parse the passed source using a number of different parsers. The parser
   * method is complemented by the JobDescription::UnParse method, a method to
   * generate a job description document in one of the supported formats.
   * Additionally the internal representation is contained in public members which
   * makes it directly accessible and modifiable from outside the scope of the
   * class.
   */
  class JobDescription {
  public:
    friend class JobDescriptionParser;
    JobDescription() : alternatives(), current(alternatives.begin()) {};

    JobDescription(const JobDescription& j, bool withAlternatives = true);

    JobDescription& operator=(const JobDescription& j);

    // Language wrapper constructor
    JobDescription(const long int& ptraddr);

    ~JobDescription() {}

    void AddAlternative(const JobDescription& j);

    bool HasAlternatives() const { return !alternatives.empty(); }
    const std::list<JobDescription>& GetAlternatives() const { return alternatives; }
    std::list<JobDescription>& GetAlternatives() { return alternatives; }
    std::list<JobDescription>  GetAlternativesCopy() const { return alternatives; }
    bool UseAlternative();
    void UseOriginal();

    void RemoveAlternatives();

    /// DEPRECATED: Check whether JobDescription is valid.
    /**
     * The JobDescription class itself is not able to tell whether its objects
     * are valid or not. Instead when parsing/outputting, JobDescriptionParser
     * classes checks the validity. Thus the Parse and UnParse methods should be
     * used for this purpose.
     **/
    operator bool() const;

    /// Parse string into JobDescription objects
    /**
     * The passed string will be tried parsed into the list of JobDescription
     * objects. The available specialized JobDesciptionParser classes will be
     * tried one by one, parsing the string, and if one succeeds the list of
     * JobDescription objects is filled with the parsed contents and true is
     * returned, otherwise false is returned. If no language specified, each
     * JobDescriptionParser will try all its supported languages. On the other
     * hand if a language is specified, only the JobDescriptionParser supporting
     * that language will be tried. A dialect can also be specified, which only
     * has an effect on the parsing if the JobDescriptionParser supports that
     * dialect.
     *
     * @param source
     * @param jobdescs
     * @param language
     * @param dialect
     * @return true if the passed string can be parsed successfully by any of
     *   the available parsers.
     **/
    static bool Parse(const std::string& source, std::list<JobDescription>& jobdescs, const std::string& language = "", const std::string& dialect = "");

    /// DEPRECATED: Parse source string
    /**
     * This method is deprecated, use the Parse(const std::string&, std::list<JobDescription>&, const std::string&, const std::string&)
     * method instead.
     **/
    bool Parse(const std::string& source, const std::string& language = "", const std::string& dialect = "");

    /// DEPRECATED: Parse source string
    /**
     * This method is deprecated, use the Parse(const std::string&, std::list<JobDescription>&, const std::string&, const std::string&)
     * method instead.
     **/
    bool Parse(const XMLNode& xmlSource);

    /// DEPRECATED: Output contents in the specified language
    /**
     * This method is deprecated, use the UnParse(std::string&, std::string, const std::string&)
     * method instead.
     **/
    std::string UnParse(const std::string& language = "nordugrid:jsdl") const;

    /// Output contents in the specified language
    /**
     *
     * @param product
     * @param language
     * @param dialect
     * @return
     **/
    bool UnParse(std::string& product, std::string language, const std::string& dialect = "") const;


    /// Get input source language
    /**
     * If this object was created by a JobDescriptionParser, then this method
     * returns a string which indicates the job description language of the
     * parsed source. If not created by a JobDescripionParser the string
     * returned is empty.
     *
     * @return const std::string& source langauge of parsed input source.
     **/
    const std::string& GetSourceLanguage() const { return sourceLanguage; }

    /// DEPRECATED: Print all values to standard output.
    /**
     * This method is DEPRECATED, use the SaveToStream method instead.
     *
     * @param longlist
     * @see SaveToStream
     */
    void Print(bool longlist = false) const;

    /// Print job description to a std::ostream object.
    /**
     * The job description will be written to the passed std::ostream object
     * out in the format indicated by the format parameter. The format parameter
     * should specify the format of one of the job description languages
     * supported by the library. Or by specifying the special "user" or
     * "userlong" format the job description will be written as a
     * attribute/value pair list with respectively less or more attributes.
     *
     * The mote
     *
     * @return true if writing the job description to the out object succeeds,
     *              otherwise false.
     * @param out a std::ostream reference specifying the ostream to write the
     *            job description to.
     * @param format specifies the format the job description should written in.
     */
    bool SaveToStream(std::ostream& out, const std::string& format) const;

    JobIdentificationType Identification;
    ApplicationType Application;
    ResourcesType Resources;
    std::list<FileType> Files;

    /// Holds attributes not fitting into this class
    /**
     * This member is used by JobDescriptionParser classes to store
     * attribute/value pairs not fitting into attributes stored in this class.
     * The form of the attribute (the key in the map) should be as follows:
     * <language>;<attribute-name>
     * E.g.: "nordugrid:xrsl;hostname".
     **/
    std::map<std::string, std::string> OtherAttributes;

  private:
    void Set(const JobDescription& j);

    std::string sourceLanguage;

    std::list<JobDescription> alternatives;
    std::list<JobDescription>::iterator current;

    static Glib::Mutex jdpl_lock;
    static JobDescriptionParserLoader *jdpl;

    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_JOBDESCRIPTION_H__
