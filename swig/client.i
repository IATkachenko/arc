// Wrap contents of $(top_srcdir)/src/hed/libs/client/Software.h
%{
#include <arc/client/Software.h>
%}
%ignore Arc::SoftwareRequirement::operator=(const SoftwareRequirement&);
%ignore Arc::Software::convert(const ComparisonOperatorEnum& co);
%ignore Arc::Software::toString(ComparisonOperator co);
%ignore operator<<(std::ostream&, const Software&);
%ignore Arc::SoftwareRequirement::SoftwareRequirement(const Software& sw, Software::ComparisonOperator swComOp);
%ignore Arc::SoftwareRequirement::add(const Software& sw, Software::ComparisonOperator swComOp);
%template(SoftwareList) std::list<Arc::Software>;
%template(SoftwareRequirementList) std::list<Arc::SoftwareRequirement>;
#ifdef SWIGJAVA
%ignore Arc::Software::operator();
%ignore Arc::SoftwareRequirement::getComparisonOperatorList() const;
%template(SoftwareListIteratorHandler) listiteratorhandler<Arc::Software>;
%template(SoftwareRequirementListIteratorHandler) listiteratorhandler<Arc::SoftwareRequirement>;
#endif
%include "../src/hed/libs/client/Software.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/Endpoint.h
%{
#include <arc/client/Endpoint.h>
%}
%ignore Arc::Endpoint::operator=(const ConfigEndpoint&);
%template(EndpointList) std::list<Arc::Endpoint>;
#ifdef SWIGJAVA
%template(EndpointListIteratorHandler) listiteratorhandler<Arc::Endpoint>;
#endif
%include "../src/hed/libs/client/Endpoint.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/JobState.h
%{
#include <arc/client/JobState.h>
%}
%ignore Arc::JobState::operator!;
%ignore Arc::JobState::operator=(const JobState&);
%rename(GetType) Arc::JobState::operator StateType; // works with swig 1.3.40, and higher...
%rename(GetType) Arc::JobState::operator Arc::JobState::StateType; // works with swig 1.3.29
%rename(GetNativeState) Arc::JobState::operator();
%template(JobStateList) std::list<Arc::JobState>;
#ifdef SWIGJAVA
%template(JobStateListIteratorHandler) listiteratorhandler<Arc::JobState>;
#endif
%include "../src/hed/libs/client/JobState.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/Job.h
%{
#include <arc/client/Job.h>
%}
%ignore Arc::Job::operator=(XMLNode);
%ignore Arc::Job::operator=(const Job&);
%template(JobList) std::list<Arc::Job>;
#ifdef SWIGPYTHON
%ignore Arc::Job::WriteJobIDsToFile(const std::list<Job>&, const std::string&, unsigned = 10, unsigned = 500000); // Clash. It is sufficient to wrap only WriteJobIDsToFile(cosnt std::list<URL>&, ...);
#endif
#ifdef SWIGJAVA
%template(JobListIteratorHandler) listiteratorhandler<Arc::Job>;
#endif
%include "../src/hed/libs/client/Job.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/JobControllerPlugin.h
%{
#include <arc/client/JobControllerPlugin.h>
%}
%ignore Arc::JobControllerPluginPluginArgument::operator const UserConfig&; // works with swig 1.3.40, and higher...
%ignore Arc::JobControllerPluginPluginArgument::operator const Arc::UserConfig&; // works with swig 1.3.29
%template(JobControllerPluginList) std::list<Arc::JobControllerPlugin *>;
%template(JobControllerPluginMap) std::map<std::string, Arc::JobControllerPlugin *>;
#ifdef SWIGJAVA
%template(JobControllerPluginListIteratorHandler) listiteratorhandler<Arc::JobControllerPlugin *>;
#endif
%include "../src/hed/libs/client/JobControllerPlugin.h"



// Wrap contents of $(top_srcdir)/src/hed/libs/client/EndpointQueryingStatus.h
%{
#include <arc/client/EndpointQueryingStatus.h>
%}
%ignore Arc::EndpointQueryingStatus::operator!;
%ignore Arc::EndpointQueryingStatus::operator=(EndpointQueryingStatusType);
%ignore Arc::EndpointQueryingStatus::operator=(const EndpointQueryingStatus&);
%include "../src/hed/libs/client/EndpointQueryingStatus.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/JobDescription.h
%{
#include <arc/client/JobDescription.h>
%}
%ignore Arc::JobDescriptionResult::operator!;
%ignore Arc::Range<int>::operator int;
%ignore Arc::OptIn::operator=(const OptIn<T>&);
%ignore Arc::OptIn::operator=(const T&);
%ignore Arc::Range::operator=(const Range<T>&);
%ignore Arc::Range::operator=(const T&);
%ignore Arc::SourceType::operator=(const URL&);
%ignore Arc::SourceType::operator=(const std::string&);
%ignore Arc::JobDescription::operator=(const JobDescription&);
#ifdef SWIGPYTHON
%apply std::string& TUPLEOUTPUTSTRING { std::string& product }; /* Applies to:
 * JobDescriptionResult JobDescription::UnParse(std::string& product, std::string language, const std::string& dialect = "") const;
 */
#endif
#ifdef SWIGJAVA
%ignore Arc::JobDescription::GetAlternatives() const;
#endif
%include "../src/hed/libs/client/JobDescription.h"
%template(JobDescriptionList) std::list<Arc::JobDescription>;
%template(InputFileTypeList) std::list<Arc::InputFileType>;
%template(OutputFileTypeList) std::list<Arc::OutputFileType>;
%template(SourceTypeList) std::list<Arc::SourceType>;
%template(TargetTypeList) std::list<Arc::TargetType>;
%template(ScalableTimeInt) Arc::ScalableTime<int>;
%template(RangeInt) Arc::Range<int>;
%template(StringOptIn) Arc::OptIn<std::string>;
#ifdef SWIGPYTHON
%clear std::string& product;
#endif
#ifdef SWIGJAVA
%template(JobDescriptionListIteratorHandler) listiteratorhandler<Arc::JobDescription>;
%template(InputFileTypeListIteratorHandler) listiteratorhandler<Arc::InputFileType>;
%template(OutputFileTypeListIteratorHandler) listiteratorhandler<Arc::OutputFileType>;
%template(SourceTypeListIteratorHandler) listiteratorhandler<Arc::SourceType>;
%template(TargetTypeListIteratorHandler) listiteratorhandler<Arc::TargetType>;
#endif


// Wrap contents of $(top_srcdir)/src/hed/libs/client/ExecutionTarget.h
%{
#include <arc/client/ExecutionTarget.h>
%}
%ignore Arc::ApplicationEnvironment::operator=(const Software&);
%ignore Arc::ExecutionTarget::operator=(const ExecutionTarget&);
%ignore Arc::GLUE2Entity<Arc::LocationAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::AdminDomainAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::ExecutionEnvironmentAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::ComputingManagerAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::ComputingShareAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::ComputingEndpointAttributes>::operator->() const;
%ignore Arc::GLUE2Entity<Arc::ComputingServiceAttributes>::operator->() const;
#ifdef SWIGPYTHON
/* When instantiating a template of the form
 * Arc::CountedPointer< T<S> > two __nonzero__
 * python methods are created in the generated python module file which
 * causes an swig error. The two __nonzero__ methods probably stems from
 * the "operator bool" and "operator ->" methods. At least in the Arc.i
 * file the "operator bool" method is renamed to "__nonzero__". In
 * order to avoid that name clash the following "operator bool" methods 
 * are ignored.
 */
%ignore Arc::CountedPointer< std::map<std::string, double> >::operator bool;
%ignore Arc::CountedPointer< std::list<Arc::ApplicationEnvironment> >::operator bool;
#endif // SWIGPYTHON
#ifdef SWIGJAVA
%ignore Arc::GLUE2Entity<Arc::LocationAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::AdminDomainAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::ExecutionEnvironmentAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::ComputingManagerAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::ComputingShareAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::ComputingEndpointAttributes>::operator*() const;
%ignore Arc::GLUE2Entity<Arc::ComputingServiceAttributes>::operator*() const;
#endif
%include "../src/hed/libs/client/GLUE2Entity.h" // Contains declaration of the GLUE2Entity template, used in ExecutionTarget.h file.
%template(ApplicationEnvironmentList) std::list<Arc::ApplicationEnvironment>;
%template(ExecutionTargetList) std::list<Arc::ExecutionTarget>;
%template(ComputingServiceList) std::list<Arc::ComputingServiceType>;
%template(ComputingEndpointMap) std::map<int, Arc::ComputingEndpointType>;
%template(ComputingShareMap) std::map<int, Arc::ComputingShareType>;
%template(ComputingManagerMap) std::map<int, Arc::ComputingManagerType>;
%template(ExecutionEnvironmentMap) std::map<int, Arc::ExecutionEnvironmentType>;
%template(SharedBenchmarkMap) Arc::CountedPointer< std::map<std::string, double> >;
%template(SharedApplicationEnvironmentList) Arc::CountedPointer< std::list<Arc::ApplicationEnvironment> >;
%template(GLUE2EntityLocationAttributes) Arc::GLUE2Entity<Arc::LocationAttributes>;
%template(CPLocationAttributes) Arc::CountedPointer<Arc::LocationAttributes>;
%template(GLUE2EntityAdminDomainAttributes) Arc::GLUE2Entity<Arc::AdminDomainAttributes>;
%template(CPAdminDomainAttributes) Arc::CountedPointer<Arc::AdminDomainAttributes>;
%template(GLUE2EntityExecutionEnvironmentAttributes) Arc::GLUE2Entity<Arc::ExecutionEnvironmentAttributes>;
%template(CPExecutionEnvironmentAttributes) Arc::CountedPointer<Arc::ExecutionEnvironmentAttributes>;
%template(GLUE2EntityComputingManagerAttributes) Arc::GLUE2Entity<Arc::ComputingManagerAttributes>;
%template(CPComputingManagerAttributes) Arc::CountedPointer<Arc::ComputingManagerAttributes>;
%template(GLUE2EntityComputingShareAttributes) Arc::GLUE2Entity<Arc::ComputingShareAttributes>;
%template(CPComputingShareAttributes) Arc::CountedPointer<Arc::ComputingShareAttributes>;
%template(GLUE2EntityComputingEndpointAttributes) Arc::GLUE2Entity<Arc::ComputingEndpointAttributes>;
%template(CPComputingEndpointAttributes) Arc::CountedPointer<Arc::ComputingEndpointAttributes>;
%template(GLUE2EntityComputingServiceAttributes) Arc::GLUE2Entity<Arc::ComputingServiceAttributes>;
%template(CPComputingServiceAttributes) Arc::CountedPointer<Arc::ComputingServiceAttributes>;
#ifdef SWIGJAVA
%template(ExecutionTargetListIteratorHandler) listiteratorhandler<Arc::ExecutionTarget>;
%template(ApplicationEnvironmentListIteratorHandler) listiteratorhandler<Arc::ApplicationEnvironment>;
%template(ComputingServiceListIteratorHandler) listiteratorhandler<Arc::ComputingServiceType>;
#endif
%include "../src/hed/libs/client/ExecutionTarget.h"
%extend Arc::ComputingServiceType {
  %template(GetExecutionTargetsFromList) GetExecutionTargets< std::list<Arc::ExecutionTarget> >;
};


// Wrap contents of $(top_srcdir)/src/hed/libs/client/EntityRetriever.h
%{
#include <arc/client/EntityRetriever.h>
%}
#ifdef SWIGJAVA
%rename(_wait) Arc::EntityRetriever::wait;
#endif
%include "../src/hed/libs/client/EntityRetriever.h"
%template() Arc::EntityConsumer<Arc::Endpoint>;
%template(EndpointContainer) Arc::EntityContainer<Arc::Endpoint>;
%template(ServiceEndpointQueryOptions) Arc::EndpointQueryOptions<Arc::Endpoint>;
%template(ServiceEndpointRetriever) Arc::EntityRetriever<Arc::Endpoint>;
%template() Arc::EntityConsumer<Arc::ComputingServiceType>;
%template(ComputingServiceContainer) Arc::EntityContainer<Arc::ComputingServiceType>;
%template(ComputingServiceQueryOptions) Arc::EndpointQueryOptions<Arc::ComputingServiceType>;
%template(TargetInformationRetriever) Arc::EntityRetriever<Arc::ComputingServiceType>;
%template() Arc::EntityConsumer<Arc::Job>;
%template(JobContainer) Arc::EntityContainer<Arc::Job>;
%template(JobListQueryOptions) Arc::EndpointQueryOptions<Arc::Job>;
%template(JobListRetriever) Arc::EntityRetriever<Arc::Job>;


// Wrap contents of $(top_srcdir)/src/hed/libs/client/SubmitterPlugin.h
%{
#include <arc/client/SubmitterPlugin.h>
%}
%ignore Arc::SubmitterPluginArgument::operator const UserConfig&; // works with swig 1.3.40, and higher...
%ignore Arc::SubmitterPluginArgument::operator const Arc::UserConfig&; // works with swig 1.3.29
%template(SubmitterPluginList) std::list<Arc::SubmitterPlugin*>;
#ifdef SWIGJAVA
%template(SubmitterPluginListIteratorHandler) listiteratorhandler<Arc::SubmitterPlugin*>;
#endif
%include "../src/hed/libs/client/SubmitterPlugin.h"

// Wrap contents of $(top_srcdir)/src/hed/libs/client/Submitter.h
%{
#include <arc/client/Submitter.h>
%}
#ifdef SWIGPYTHON
#if SWIG_VERSION <= 0x010329
/* Swig version 1.3.29 cannot handle mapping a template of a "const *" type, so
 * adding a traits_from::from method taking a "const *" as taken from
 * swig-1.3.31 makes it possible to handle such types.
 */
%{
namespace swig {
template <class Type> struct traits_from<const Type *> {
  static PyObject *from(const Type* val) {
    return traits_from_ptr<Type>::from(const_cast<Type*>(val), 0);
  }
};
}
%}
#endif
#endif
%template(JobDescriptionPtrList) std::list<const Arc::JobDescription *>;
%template(EndpointQueryingStatusMap) std::map<Arc::Endpoint, Arc::EndpointQueryingStatus>;
%template(EndpointSubmissionStatusMap) std::map<Arc::Endpoint, Arc::EndpointSubmissionStatus>;
#ifdef SWIGJAVA
// TODO
#endif
%include "../src/hed/libs/client/Submitter.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/ComputingServiceRetriever.h
%{
#include <arc/client/ComputingServiceRetriever.h>
%}
#ifdef SWIGJAVA
%rename(_wait) Arc::ComputingServiceRetriever::wait;
#endif
#ifdef SWIGPYTHON
%extend Arc::ComputingServiceRetriever {
  const std::list<ComputingServiceType>& getResults() { return *self; }  

  %insert("python") %{
    def __iter__(self):
      return self.getResults().__iter__()
  %}
}
%pythonprepend Arc::ComputingServiceRetriever::GetExecutionTargets %{
        etList = ExecutionTargetList()
        args = args + (etList,)
%}
%pythonappend Arc::ComputingServiceRetriever::GetExecutionTargets %{
        return etList
%}
#endif
%include "../src/hed/libs/client/ComputingServiceRetriever.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/Broker.h
%{
#include <arc/client/Broker.h>
%}
%ignore Arc::Broker::operator=(const Broker&);
%ignore Arc::BrokerPluginArgument::operator const UserConfig&; // works with swig 1.3.40, and higher...
%ignore Arc::BrokerPluginArgument::operator const Arc::UserConfig&; // works with swig 1.3.29
#ifdef SWIGJAVA
%rename(compare) Arc::Broker::operator()(const ExecutionTarget&, const ExecutionTarget&) const;
%rename(compare) Arc::BrokerPlugin::operator()(const ExecutionTarget&, const ExecutionTarget&) const;
#endif
%include "../src/hed/libs/client/Broker.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/JobSupervisor.h
%{
#include <arc/client/JobSupervisor.h>
%}
%include "../src/hed/libs/client/JobSupervisor.h"


// Wrap contents of $(top_srcdir)/src/hed/libs/client/TestACCControl.h
%{
#include <arc/client/TestACCControl.h>
%}
#ifdef SWIGPYTHON
%warnfilter(SWIGWARN_TYPEMAP_SWIGTYPELEAK) Arc::SubmitterPluginTestACCControl::submitJob;
%warnfilter(SWIGWARN_TYPEMAP_SWIGTYPELEAK) Arc::SubmitterPluginTestACCControl::migrateJob;
%rename(_BrokerPluginTestACCControl) Arc::BrokerPluginTestACCControl;
%rename(_JobDescriptionParserTestACCControl) Arc::JobDescriptionParserTestACCControl;
%rename(_JobControllerPluginTestACCControl) Arc::JobControllerPluginTestACCControl;
%rename(_SubmitterPluginTestACCControl) Arc::SubmitterPluginTestACCControl;
%rename(_ServiceEndpointRetrieverPluginTESTControl) Arc::ServiceEndpointRetrieverPluginTESTControl;
%rename(_TargetInformationRetrieverPluginTESTControl) Arc::TargetInformationRetrieverPluginTESTControl;
#endif
%include "../src/hed/libs/client/TestACCControl.h"
#ifdef SWIGPYTHON
%pythoncode %{
BrokerPluginTestACCControl = StaticPropertyWrapper(_BrokerPluginTestACCControl)
JobDescriptionParserTestACCControl = StaticPropertyWrapper(_JobDescriptionParserTestACCControl)
JobControllerPluginTestACCControl = StaticPropertyWrapper(_JobControllerPluginTestACCControl)
SubmitterPluginTestACCControl = StaticPropertyWrapper(_SubmitterPluginTestACCControl)
ServiceEndpointRetrieverPluginTESTControl = StaticPropertyWrapper(_ServiceEndpointRetrieverPluginTESTControl)
TargetInformationRetrieverPluginTESTControl = StaticPropertyWrapper(_TargetInformationRetrieverPluginTESTControl)
%}
#endif
