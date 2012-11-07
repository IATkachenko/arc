// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_JOBLISTRETRIEVERPLUGINWSRFBES_H__
#define __ARC_JOBLISTRETRIEVERPLUGINWSRFBES_H__

#include <arc/compute/Job.h>
#include <arc/compute/EntityRetriever.h>

namespace Arc {

  class Logger;

  class JobListRetrieverPluginWSRFBES : public JobListRetrieverPlugin {
  public:
    JobListRetrieverPluginWSRFBES(PluginArgument* parg): JobListRetrieverPlugin(parg) {
      supportedInterfaces.push_back("org.ogf.bes");
    }
    virtual ~JobListRetrieverPluginWSRFBES() {}

    static Plugin* Instance(PluginArgument *arg) { return new JobListRetrieverPluginWSRFBES(arg); }
    virtual EndpointQueryingStatus Query(const UserConfig&, const Endpoint&, std::list<Job>&, const EndpointQueryOptions<Job>&) const; // No implementation in cpp file -- returns EndpointQueryingStatus::FAILED.
    virtual bool isEndpointNotSupported(const Endpoint&) const;

  private:
    static Logger logger;
  };

} // namespace Arc

#endif // __ARC_JOBLISTRETRIEVERPLUGINWSRFBES_H__
