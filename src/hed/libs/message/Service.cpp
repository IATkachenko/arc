#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Service.h"

namespace Arc {

Logger Service::logger(Logger::getRootLogger(), "Service");

Service::Service(Config*) {
}

void Service::AddSecHandler(Config* cfg,ArcSec::SecHandler* sechandler,const std::string& label) {
    if(sechandler) {
        sechandlers_[label].push_back(sechandler); //need polishing to put the SecHandlerFactory->getinstance here
        XMLNode cn = (*cfg)["SecHandler"];
        Config cfg_(cn);
    }
}

bool Service::ProcessSecHandlers(Message& message,const std::string& label) {
    std::map<std::string,std::list<ArcSec::SecHandler*> >::iterator q = sechandlers_.find(label);
    if(q == sechandlers_.end()) {
        logger.msg(VERBOSE, "No security processing/check requested for '%s'", label);
        return true;
    }

    std::list<ArcSec::SecHandler*>::iterator h = q->second.begin();
    for(;h!=q->second.end();++h) {
        ArcSec::SecHandler* handler = *h;
        if(handler) if(!(handler->Handle(&message))) {
            logger.msg(VERBOSE, "Security processing/check for '%s' failed", label);
            return false;
        }
    }
    logger.msg(VERBOSE, "Security processing/check for '%s' passed", label);
    return true;
}

bool Service::RegistrationCollector(XMLNode &doc)
{
    logger.msg(WARNING, "Empty registration collector");
}

} // namespace Arc
