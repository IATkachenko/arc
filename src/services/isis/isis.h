#ifndef __ARC_ISIS_H__
#define __ARC_ISIS_H__

#include <string>

#include <arc/Logger.h>
#include <arc/XMLNode.h>
#include <arc/infosys/RegisteredService.h>
#include <arc/dbxml/XmlDatabase.h>

namespace ISIS {

    class ISIService: public Arc::RegisteredService {
        private:
            Arc::Logger logger_;
            std::string serviceid_;
            std::string endpoint_;
            std::string expiration_;

            Arc::XmlDatabase *db_;
            Arc::NS ns_;
            Arc::MCC_Status make_soap_fault(Arc::Message &outmsg);
            // List of known neighbor's endpoint URL in string
            std::vector<std::string> neighbors_;

            // Functions for the service specific interface
            Arc::MCC_Status Query(Arc::XMLNode &request, Arc::XMLNode &response);
            Arc::MCC_Status Register(Arc::XMLNode &request, Arc::XMLNode &response);
            Arc::MCC_Status RemoveRegistrations(Arc::XMLNode &request, Arc::XMLNode &response);
            Arc::MCC_Status GetISISList(Arc::XMLNode &request, Arc::XMLNode &response);
        public:
            ISIService(Arc::Config *cfg);
            virtual ~ISIService(void);
            virtual Arc::MCC_Status process(Arc::Message &in, Arc::Message &out);
            virtual bool RegistrationCollector(Arc::XMLNode &doc);
            virtual std::string getID();
    };
} // namespace
#endif

