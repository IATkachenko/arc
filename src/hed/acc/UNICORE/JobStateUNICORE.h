#ifndef __ARC_JOBSTATEUNICORE_H__
#define __ARC_JOBSTATEUNICORE_H__

#include <arc/compute/JobState.h>

namespace Arc {

  class JobStateUNICORE
    : public JobState {
  public:
    JobStateUNICORE(const std::string& state)
      : JobState(state, &StateMap) {}
    static JobState::StateType StateMap(const std::string& state);
  };

}

#endif // __ARC_JOBSTATEUNICORE_H__
