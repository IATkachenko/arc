#ifndef GRID_MANAGER_JOB_H
#define GRID_MANAGER_JOB_H

#include <sys/types.h>
#include <string>

#include <arc/Run.h>

class JobsList;
class JobLocalDescription;
class JobUser;

typedef enum {
  JOB_STATE_ACCEPTED   = 0,
  JOB_STATE_PREPARING  = 1,
  JOB_STATE_SUBMITTING = 2,
  JOB_STATE_INLRMS     = 3,
  JOB_STATE_FINISHING  = 4,
  JOB_STATE_FINISHED   = 5,
  JOB_STATE_DELETED    = 6,
  JOB_STATE_CANCELING  = 7,
  JOB_STATE_UNDEFINED  = 8
} job_state_t;

typedef struct {
  job_state_t id;
  const char* name;
  char mail_flag;
} job_state_rec_t;

extern job_state_rec_t states_all[JOB_STATE_UNDEFINED+1];

#define JOB_STATE_NUM (JOB_STATE_UNDEFINED+1)

/* all states under control - not nice */
#define JOB_NUM_ACCEPTED (\
  jcfg.jobs_num[JOB_STATE_ACCEPTED] + \
  jcfg.jobs_num[JOB_STATE_PREPARING] + \
  jcfg.jobs_num[JOB_STATE_SUBMITTING] + \
  jcfg.jobs_num[JOB_STATE_INLRMS] + \
  jcfg.jobs_num[JOB_STATE_FINISHING] + \
  jcfg.jobs_pending \
)

/* states running something heavy on frontend */
#define JOB_NUM_PROCESSING (\
  jcfg.jobs_num[JOB_STATE_PREPARING] + \
  jcfg.jobs_num[JOB_STATE_FINISHING] \
)

#define JOB_NUM_PREPARING (\
  jcfg.jobs_num[JOB_STATE_PREPARING] \
)

#define JOB_NUM_FINISHING (\
  jcfg.jobs_num[JOB_STATE_FINISHING] \
)

/* states doing their job mostly on computing nodes */
#define JOB_NUM_RUNNING (\
  jcfg.jobs_num[JOB_STATE_SUBMITTING] + \
  jcfg.jobs_num[JOB_STATE_INLRMS] \
)

/*
  Jobs identifier. Stored as string. Normally is a big number.
*/
typedef std::string JobId;

/*
  Object to represent job in memory.
*/
class JobDescription {
 friend class JobsList;
 private:
  /* state of the job (state machine) */
  job_state_t job_state;
  /* flag to indicate job stays at this stage due to limits imposed
     such jobs are not counted in couters */
  bool job_pending; 
  /* identifier */
  JobId job_id;
  /* directory to run job in */
  std::string session_dir;
  /* explanation of job's failure */
  std::string failure_reason;
  /* how long job is kept on cluster after it finished */
  time_t keep_finished;
  time_t keep_deleted;
  /* pointer to object containing most important parameters of job,
     loaded when needed. */
  JobLocalDescription* local;
  /* uid and gid of job's owner */
  uid_t job_uid;
  gid_t job_gid;
  /* reties left (should be reset for each state change) */
  int retries;
  /* time to perform the next retry */
  time_t next_retry;
  /* used to determine data transfer share (eg DN, VOMS VO) */
  std::string transfer_share;
 public:
  /* external utility beeing run to perform tasks like stage-in/our, 
     submit/cancel. (todo - move to private) */
  Arc::Run* child;
  /* 
    Constructors and destructor.
    Accepts:
      job_id - identifier
      dir - session_dir of job
      state - initial state of job
  */
  JobDescription(void);
  JobDescription(const JobDescription &job);
  JobDescription(const JobId &job_id,const std::string &dir,job_state_t state = JOB_STATE_UNDEFINED);
  ~JobDescription(void);
  job_state_t get_state() const { return job_state; };
  const char* get_state_name() const;
  static const char* get_state_name(job_state_t st);
  static job_state_t get_state(const char* state);
  const JobId& get_id() const { return job_id; };
  std::string SessionDir(void) const { return session_dir; };
  void AddFailure(const std::string &reason) {
    //if(failure_reason.length()) failure_reason+="\n";
    failure_reason+=reason; failure_reason+="\n";
  };
  const std::string& GetFailure(void) const { return failure_reason; };
  JobLocalDescription* get_local(void) const { return local; };
  void set_local(JobLocalDescription* desc) { local=desc; };
//  void set_state(job_state_t state) { job_state=state; };
  bool operator==(const JobId &id) { return (job_id == id); };
  bool operator!=(const JobId &id) { return (job_id != id); };
  void set_uid(uid_t uid,gid_t gid) {
    if(uid != (uid_t)(-1)) { job_uid=uid; };
    if(gid != (uid_t)(-1)) { job_gid=gid; };
  };
  uid_t get_uid(void) const { return job_uid; };
  gid_t get_gid(void) const { return job_gid; };
  void set_share(std::string share);
  /* force 'local' to be created and read from file if not already available */
  bool GetLocalDescription(const JobUser &user);
};
#endif
