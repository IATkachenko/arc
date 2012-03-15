#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arc/security/ClassLoader.h>

#include "ArcAttributeFactory.h"
#include <arc/security/ArcPDP/attr/AttributeProxy.h>
#include <arc/security/ArcPDP/attr/StringAttribute.h>
#include <arc/security/ArcPDP/attr/DateTimeAttribute.h>
#include <arc/security/ArcPDP/attr/X500NameAttribute.h>
#include <arc/security/ArcPDP/attr/AnyURIAttribute.h>
#include <arc/security/ArcPDP/attr/GenericAttribute.h>

#include "ArcAttributeProxy.h"

/*
loader_descriptors __arc_attrfactory_modules__  = {
    { "attr.factory", 0, &get_attr_factory },
    { NULL, 0, NULL }
};
*/

using namespace Arc;
namespace ArcSec {

Arc::Plugin* get_arcpdp_attr_factory (Arc::PluginArgument* arg) {
    return new ArcSec::ArcAttributeFactory(arg);
}

void ArcAttributeFactory::initDatatypes(){
  //Some Arc specified attribute types
  apmap.insert(std::pair<std::string, AttributeProxy*>(StringAttribute::getIdentifier(), new ArcAttributeProxy<StringAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(DateTimeAttribute::getIdentifier(), new ArcAttributeProxy<DateTimeAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(DateAttribute::getIdentifier(), new ArcAttributeProxy<DateAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(TimeAttribute::getIdentifier(), new ArcAttributeProxy<TimeAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(DurationAttribute::getIdentifier(), new ArcAttributeProxy<DurationAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(PeriodAttribute::getIdentifier(), new ArcAttributeProxy<PeriodAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(X500NameAttribute::getIdentifier(), new ArcAttributeProxy<X500NameAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(AnyURIAttribute::getIdentifier(), new ArcAttributeProxy<AnyURIAttribute>));
  apmap.insert(std::pair<std::string, AttributeProxy*>(GenericAttribute::getIdentifier(), new ArcAttributeProxy<GenericAttribute>));

 /** TODO:  other datatype............. */

}

ArcAttributeFactory::ArcAttributeFactory(Arc::PluginArgument* parg) : AttributeFactory(parg) {
  initDatatypes();
}

AttributeValue* ArcAttributeFactory::createValue(const XMLNode& node, const std::string& type){
  AttrProxyMap::iterator it;
  if((it=apmap.find(type)) != apmap.end())
    return ((*it).second)->getAttribute(node);
  // This may look like hack, but generic attribute needs special treatment
  GenericAttribute* attr = new GenericAttribute(
          (std::string)const_cast<XMLNode&>(node),
          (std::string)(const_cast<XMLNode&>(node).Attribute("AttributeId")));
  attr->setType(type);
  return attr;
  // return NULL;
}

ArcAttributeFactory::~ArcAttributeFactory(){
  AttrProxyMap::iterator it;
  for(it = apmap.begin(); it != apmap.end(); it = apmap.begin()){
    AttributeProxy* attrproxy = (*it).second;
    apmap.erase(it);
    if(attrproxy) delete attrproxy;
  }
}

} // namespace ArcSec

