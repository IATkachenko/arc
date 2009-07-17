#ifndef __ARC_SCHED_H__
#define __ARC_SCHED_H__

#include <arc/infosys/RegisteredService.h>
#include <arc/message/Service.h>
#include <arc/delegation/DelegationInterface.h>
#include <arc/infosys/InformationInterface.h>

#include "job_queue.h"
#include "resources_handling.h"

namespace GridScheduler {

// Sched job queue initializator

class GridSchedulerService: public Arc::RegisteredService {
    private:
        bool IsAcceptingNewActivities;
        Arc::JobQueue jobq;
        ResourcesHandling resources;
        std::string db_path;
        std::string endpoint;
        std::map<std::string, std::string> cli_config;
        int lifetime_after_done;
        int reschedule_period;
        int reschedule_wait;
        int period;
        int timeout;
        Arc::NS ns_;
        Arc::Logger logger_;
        Arc::DelegationContainerSOAP delegations_;
        Arc::InformationContainer infodoc_;
        // BES Interface
        Arc::MCC_Status CreateActivity(Arc::XMLNode &in, Arc::XMLNode &out);
        Arc::MCC_Status GetActivityStatuses(Arc::XMLNode &in, 
                                            Arc::XMLNode &out);
        Arc::MCC_Status TerminateActivities(Arc::XMLNode &in, 
                                            Arc::XMLNode &out);
        Arc::MCC_Status GetFactoryAttributesDocument(Arc::XMLNode &in, 
                                                     Arc::XMLNode &out);
        Arc::MCC_Status StopAcceptingNewActivities(Arc::XMLNode &in, 
                                                   Arc::XMLNode &out);
        Arc::MCC_Status StartAcceptingNewActivities(Arc::XMLNode &in, 
                                                    Arc::XMLNode &out);
        Arc::MCC_Status ChangeActivityStatus(Arc::XMLNode &in, 
                                             Arc::XMLNode &out);
        // iBES Interface
        Arc::MCC_Status GetActivities(Arc::XMLNode &in, 
                                      Arc::XMLNode &out, 
                                      const std::string &resource_id);
        Arc::MCC_Status ReportActivitiesStatus(Arc::XMLNode &in, 
                                               Arc::XMLNode &out, 
                                            const std::string &resource_id);
        Arc::MCC_Status GetActivitiesStatusChanges(Arc::XMLNode &in, 
                                                   Arc::XMLNode &out, 
                                            const std::string &resource_id);

        // WS-Propoerty Interface
        Arc::MCC_Status GetActivityDocuments(Arc::XMLNode &in, 
                                             Arc::XMLNode &out);
        // Fault handlers
        Arc::MCC_Status make_soap_fault(Arc::Message& outmsg);
        bool match(Arc::XMLNode &in, Arc::Job *j);
    public:
        GridSchedulerService(Arc::Config *cfg);
        virtual ~GridSchedulerService(void);
        virtual Arc::MCC_Status process(Arc::Message& inmsg,
                                        Arc::Message& outmsg);
        void doSched(void);
        void doReschedule(void);
        int getPeriod(void) { return period; };
        int getReschedulePeriod(void) { return reschedule_period; };
        void InformationCollector(void);
        virtual bool RegistrationCollector(Arc::XMLNode &doc);
}; // class GridSchedulerService

} // namespace GridScheduler

#endif

