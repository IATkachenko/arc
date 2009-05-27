%{
#include <arc/message/MCC.h>
#include <arc/message/MCC_Status.h>
#include <arc/message/MessageAttributes.h>
#include <arc/message/SecAttr.h> 
#include <arc/message/MessageAuth.h>
#include <arc/message/Message.h>
#include <arc/message/PayloadRaw.h>
#include <arc/message/SOAPEnvelope.h>
#include <arc/message/SOAPMessage.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/PayloadStream.h>
#include <arc/message/Service.h> 
%}
%include <typemaps.i>

#ifdef SWIGJAVA
%ignore SOAPEnvelope(const char *);
%ignore Reason(const char *);
%ignore *::Put(const char *);
#endif

%include "../src/hed/libs/message/MCC_Status.h"
%rename(next) Arc::AttributeIterator::operator++;

#ifdef SWIGPYTHON
%pythonappend Arc::MessageAttributes::getAll %{
        d = dict()
        while val.hasMore():
            d[val.key()] = val.__ref__()
            val.next()
        return d
%}
#endif

%include "../src/hed/libs/message/MessageAttributes.h"
%apply std::string& OUTPUT { std::string &val };
%include "../src/hed/libs/message/SecAttr.h"
%clear std::string &val;

#ifdef SWIGPYTHON
%pythonprepend Arc::MessageAuth::Export %{
        x = XMLNode("<dummy/>")
        args = (args[0], args[1].fget(), x)
%}
%pythonappend Arc::MessageAuth::Export %{
        return x
%}

%import "../src/hed/libs/loader/Loader.h"
%import "../src/hed/libs/loader/ModuleManager.h"
%import "../src/hed/libs/loader/Plugin.h"
#endif

%ignore operator Arc::Config*;
%ignore operator Arc::ChainContext*;
%ignore operator Arc::XMLNode;

%include "../src/hed/libs/message/MessageAuth.h"
%include "../src/hed/libs/message/Message.h"
%include "../src/hed/libs/message/MCC.h"
%include "../src/hed/libs/message/PayloadRaw.h"

%apply std::string& OUTPUT { std::string& out_xml_str }; 
%include "../src/hed/libs/message/SOAPEnvelope.h"
%clear std::string& out_xml_str;

%include "../src/hed/libs/message/SOAPMessage.h"
%include "../src/hed/libs/message/PayloadSOAP.h"
%include "../src/hed/libs/message/PayloadStream.h"
%include "../src/hed/libs/message/Service.h"
