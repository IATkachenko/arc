#ifndef __ARC_SP_H__
#define __ARC_SP_H__

#include <map>
#include <arc/Logger.h>
#include <arc/infosys/RegisteredService.h>
#include <arc/infosys/InformationInterface.h>

namespace SPService {

/** This is service which accepts HTTP request from user agent (web browser) in the 
 * client side and processes the functionality of Service Provider in SAML2 SSO
 * profile --- composing <AuthnRequest/>
 * Note: the IdP name is provided by the user agent directely when it gives a request,
 * instead of the WRYF(where are you from) or Discovery Service in other implementation
 */

class Service_SP: public Arc::RegisteredService
{
    protected:
        Arc::NS ns_;
        Arc::Logger logger;
        Arc::XMLNode metadata_node_;
        std::string sp_name_;
        std::string cert_file_;
        std::string privkey_file_;
        std::string endpoint_;
        std::string expiration_;
    public:
        /** Constructor*/
        Service_SP(Arc::Config *cfg);
        virtual ~Service_SP(void);
        /** Service request processing routine */
        virtual Arc::MCC_Status process(Arc::Message&,Arc::Message&);

        bool RegistrationCollector(Arc::XMLNode &doc);

};

} // namespace SPService

#endif /* __ARC_SP_H__ */
