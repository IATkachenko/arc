// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOB_H__
#define __ARC_JOB_H__

#include <arc/DateTime.h>
#include <arc/URL.h>
#include <arc/client/JobState.h>
#include <string>

namespace Arc {

  class Logger;

  /// Job
  /**
   * This class describe a Grid job. Most of the members contained in
   * this class are directly linked to the ComputingActivity defined
   * in the GLUE Specification v. 2.0 (GFD-R-P.147).
   */
  class Job {
  public:

    /// Create a Job object
    /**
     * Default constructor. Takes no arguments.
     */
    Job();
    ~Job();

    Job(const Job& job);
    Job(XMLNode job);

    // Information stored in the job list file
    // Obligatory information
    std::string Flavour;
    URL& JobID;
    URL Cluster;
    // Optional information (ACCs fills if they need it)
    URL SubmissionEndpoint;
    URL InfoEndpoint;
    URL ISB;
    URL OSB;
    // ACC implementation dependent information
    std::string AuxInfo;

    // Information retrieved from the information system
    std::string Name;
    std::string Type;
    URL IDFromEndpoint;
    std::string LocalIDFromManager;
    std::string JobDescription;
    std::string JobDescriptionDocument;
    JobState State;
    JobState RestartState;
    int ExitCode;
    std::string ComputingManagerExitCode;
    std::list<std::string> Error;
    int WaitingPosition;
    std::string UserDomain;
    std::string Owner;
    std::string LocalOwner;
    Period RequestedTotalWallTime;
    Period RequestedTotalCPUTime;
    int RequestedSlots;
    std::list<std::string> RequestedApplicationEnvironment;
    std::string StdIn;
    std::string StdOut;
    std::string StdErr;
    std::string LogDir;
    std::list<std::string> ExecutionNode;
    std::string Queue;
    Period UsedTotalWallTime;
    Period UsedTotalCPUTime;
    int UsedMainMemory;
    std::list<std::string> UsedApplicationEnvironment;
    int UsedSlots;
    Time LocalSubmissionTime;
    Time SubmissionTime;
    Time ComputingManagerSubmissionTime;
    Time StartTime;
    Time ComputingManagerEndTime;
    Time EndTime;
    Time WorkingAreaEraseTime;
    Time ProxyExpirationTime;
    std::string SubmissionHost;
    std::string SubmissionClientName;
    Time CreationTime;
    Period Validity;
    std::list<std::string> OtherMessages;
    //Associations
    std::list<std::string>  ActivityOldID;
    std::map<std::string, std::string> LocalInputFiles;
    //ExecutionEnvironment (condensed)
    bool VirtualMachine;
    std::string UsedCPUType;
    std::string UsedOSFamily;
    std::string UsedPlatform;

    /// DEPRECATED: Print the Job information to std::cout
    /**
     * This method is DEPRECATED, use the SaveToStream method instead. Method to
     * print the Job attributes to std::cout
     *
     * @param longlist is boolean for long listing (more details).
     * @see SaveToStream
     **/
    void Print(bool longlist) const;

    /// Write job information to a std::ostream object
    /**
     * This method will write job information to the passed std::ostream object.
     * The longlist boolean specifies whether more (true) or less (false)
     * information should be printed.
     *
     * @param out is the std::ostream object to print the attributes to.
     * @param longlist is a boolean for switching on long listing (more
     *        details).
     **/
    void SaveToStream(std::ostream& out, bool longlist) const;

    /// Set Job attributes from a XMLNode
    /**
     * The attributes of the Job object is set to the values specified in the
     * XMLNode. The XMLNode should be a ComputingActivity type using the GLUE2
     * XML hierarchical rendering, see
     * http://forge.gridforum.org/sf/wiki/do/viewPage/projects.glue-wg/wiki/GLUE2XMLSchema
     * for more information. Note that associations are not parsed.
     *
     * @param job is a XMLNode of GLUE2 ComputingActivity type.
     * @see ToXML
     **/
    Job& operator=(XMLNode job);

    Job& operator=(const Job& job);

    /// Add job information to a XMLNode
    /**
     * Child nodes of GLUE ComputingActivity type containing job information of
     * this object will be added to the passed XMLNode.
     *
     * @param job is the XMLNode to add job information to in form of
     *        GLUE2 ComputingActivity type child nodes.
     * @see operator=
     **/
    void ToXML(XMLNode job) const;

    static bool CompareJobID(const Job* a, const Job* b);
    static bool CompareSubmissionTime(const Job* a, const Job* b);
    static bool CompareJobName(const Job* a, const Job* b);

    /// Read all jobs from file
    /**
     * This static method will read jobs (in XML format) from the specified
     * file, and they will be stored in the referenced list of jobs. The XML
     * element in the file representing a job should be named "Job", and have
     * the same format as accepted by the operator=(XMLNode) method. To avoid
     * simultaneous use (writing and reading) of the file, reading will not be
     * initiated before a lock on the file has been acquired. For this purpose
     * the FileLock class is used. nTries specifies the maximal number of times
     * the method will try to acquire a lock on the file, with an interval of
     * tryInterval micro seconds between each attempt. If a lock is not acquired
     * this method returns false. The method will also return false if the
     * content of file is not in XML format. Otherwise it returns true.
     *
     * @param filename is the filename of the job list to read jobs from.
     * @param jobs is a reference to a list of Job objects, which will be filled
     *  with the jobs read from file (cleared before use).
     * @param nTries specifies the maximal number of times the method will try
     *  to acquire a lock on file to read.
     * @param tryInterval specifies the interval (in micro seconds) between each
     *  attempt to acquire a lock.
     * @return true in case of success, otherwise false.
     * @see operator=(XMLNode)
     * @see WriteJobsToTruncatedFile
     * @see WriteJobsToFile
     * @see RemoveJobsFromFile
     * @see FileLock
     * @see XMLNode::ReadFromFile
     **/
    static bool ReadAllJobsFromFile(const std::string& filename, std::list<Job>& jobs, unsigned nTries = 10, unsigned tryInterval = 500000);

    /// Truncate file and write jobs to it
    /**
     * This static method will write the passed list of jobs to the specified
     * file, but before writing the file will be truncated. Jobs will be written
     * in XML format as returned by the ToXML method, and each job will be
     * contained in a element named "Job". The passed list of jobs must not
     * contain two identical jobs (i.e. IDFromEndpoint identical), if that is
     * the case false will be returned. To avoid simultaneous use (writing and
     * reading) of the file, writing will not be initiated before a lock on the
     * file has been acquired. For this purpose the FileLock class is used.
     * nTries specifies the maximal number of times the method will try to
     * acquire a lock on the file, with an interval of tryInterval micro seconds
     * between each attempt. If a lock is not acquired this method returns
     * false. The method will also return false if writing jobs to the file
     * fails. Otherwise it returns true.
     *
     * @param filename is the filename of the job list to write jobs to.
     * @param jobs is the list of Job objects which should be written to file.
     * @param nTries specifies the maximal number of times the method will try
     *  to acquire a lock on file to read.
     * @param tryInterval specifies the interval (in micro seconds) between each
     *  attempt to acquire a lock.
     * @return true in case of success, otherwise false.
     * @see ToXML
     * @see ReadAllJobsFromFile
     * @see WriteJobsToFile
     * @see RemoveJobsFromFile
     * @see FileLock
     * @see XMLNode::SaveToFile
     **/
    static bool WriteJobsToTruncatedFile(const std::string& filename, const std::list<Job>& jobs, unsigned nTries = 10, unsigned tryInterval = 500000);

    /// Write jobs to file
    /**
     * This method is in all respects identical to the WriteJobsToFile(const std::string&, const std::list<Job>&, std::list<const Job*>&, unsigned, unsigned)
     * method, except for the information about new jobs which is disregarded.
     *
     * @see WriteJobsToFile(const std::string&, const std::list<Job>&, std::list<const Job*>&, unsigned, unsigned)
     */
    static bool WriteJobsToFile(const std::string& filename, const std::list<Job>& jobs, unsigned nTries = 10, unsigned tryInterval = 500000);

    /// Write jobs to file
    /**
     * This static method will write (appending) the passed list of jobs to the
     * specified file. Jobs will be written in XML format as returned by the
     * ToXML method, and each job will be contained in a element named "Job".
     * The passed list of jobs must not contain two identical jobs (i.e.
     * IDFromEndpoint identical), if that is the case false will be returned. If
     * on the other hand a job in the list is identical to one in file, the one
     * in file will be overwritten. A pointer (no new) to those jobs from the
     * list which are not in the file will be added to newJobs list, thus these
     * pointers goes out of scope when jobs list goes out of scope. To avoid
     * simultaneous use (writing and reading) of the file, writing will not be
     * initiated before a lock on the file has been acquired. For this purpose
     * the FileLock class is used. nTries specifies the maximal number of times
     * the method will try to acquire a lock on the file, with an interval of
     * tryInterval micro seconds between each attempt. If a lock is not acquired
     * this method returns false. The method will also return false if writing
     * jobs to the file fails. Otherwise it returns true.
     *
     * @param filename is the filename of the job list to write jobs to.
     * @param jobs is the list of Job objects which should be written to file.
     * @param newJobs is a reference to a list of pointers to Job objects which
     *  are not duplicates (cleared before use).
     * @param nTries specifies the maximal number of times the method will try
     *  to acquire a lock on file to read.
     * @param tryInterval specifies the interval (in micro seconds) between each
     *  attempt to acquire a lock.
     * @return true in case of success, otherwise false.
     * @see ToXML
     * @see ReadAllJobsFromFile
     * @see WriteJobsToTruncatedFile
     * @see RemoveJobsFromFile
     * @see FileLock
     * @see XMLNode::SaveToFile
     **/
    static bool WriteJobsToFile(const std::string& filename, const std::list<Job>& jobs, std::list<const Job*>& newJobs, unsigned nTries = 10, unsigned tryInterval = 500000);

    /// Truncate file and write jobs to it
    /**
     * This static method will remove the jobs having IDFromEndpoint identical
     * to any of those in the passed list jobids. To avoid simultaneous use
     * (writing and reading) of the file, writing will not be initiated before a
     * lock on the file has been acquired. For this purpose the FileLock class
     * is used. nTries specifies the maximal number of times the method will try
     * to acquire a lock on the file, with an interval of tryInterval micro
     * seconds between each attempt. If a lock is not acquired this method
     * returns false. The method will also return false if reading from or
     * writing jobs to the file fails. Otherwise it returns true.
     *
     * @param filename is the filename of the job list to write jobs to.
     * @param jobids is a list of URL objects which specifies which jobs from
     *  the file to remove.
     * @param nTries specifies the maximal number of times the method will try
     *  to acquire a lock on file to read.
     * @param tryInterval specifies the interval (in micro seconds) between each
     *  attempt to acquire a lock.
     * @return true in case of success, otherwise false.
     * @see ReadAllJobsFromFile
     * @see WriteJobsToTruncatedFile
     * @see WriteJobsToFile
     * @see FileLock
     * @see XMLNode::ReadFromFile
     * @see XMLNode::SaveToFile
     **/
    static bool RemoveJobsFromFile(const std::string& filename, const std::list<URL>& jobids, unsigned nTries = 10, unsigned tryInterval = 500000);

  private:
    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_JOB_H__
