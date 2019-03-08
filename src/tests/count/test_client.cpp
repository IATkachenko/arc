#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <signal.h>

#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/loader/Loader.h>
#include <arc/message/SOAPEnvelope.h>
#include <arc/message/PayloadSOAP.h>

int main(void) {
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  Arc::Logger logger(Arc::Logger::rootLogger, "Test");
  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::rootLogger.addDestination(logcerr);
  logger.msg(Arc::INFO, "Creating client side chain");
  // Create client chain
  Arc::XMLNode client_doc("\
    <ArcConfig\
      xmlns=\"http://www.nordugrid.org/schemas/ArcConfig/2007\"\
      xmlns:tcp=\"http://www.nordugrid.org/schemas/ArcMCCTCP/2007\">\
     <ModuleManager>\
        <Path>.libs/</Path>\
        <Path>../../hed/mcc/http/.libs/</Path>\
        <Path>../../hed/mcc/soap/.libs/</Path>\
        <Path>../../hed/mcc/tls/.libs/</Path>\
        <Path>../../hed/mcc/tcp/.libs/</Path>\
     </ModuleManager>\
     <Plugins><Name>mcctcp</Name></Plugins>\
     <Plugins><Name>mcctls</Name></Plugins>\
     <Plugins><Name>mcchttp</Name></Plugins>\
     <Plugins><Name>mccsoap</Name></Plugins>\
     <Chain>\
      <Component name='tcp.client' id='tcp'><tcp:Connect><tcp:Host>localhost</tcp:Host><tcp:Port>60000</tcp:Port></tcp:Connect></Component>\
      <Component name='tls.client' id='tls'> <next id='tcp'/>\
        <KeyPath>./key.pem</KeyPath>\
        <CertificatePath>./cert.pem</CertificatePath>\
        <CACertificatePath>./ca.pem</CACertificatePath>\
      </Component>\
      <Component name='http.client' id='http'><next id='tls'/><Method>POST</Method><Endpoint>/Count</Endpoint></Component>\
      <Component name='soap.client' id='soap' entry='soap'><next id='http'/></Component>\
     </Chain>\
    </ArcConfig>");
  Arc::Config client_config(client_doc);
  if(!client_config) {
    logger.msg(Arc::ERROR, "Failed to load client configuration");
    return -1;
  };
  Arc::Loader client_loader(&client_config);
  logger.msg(Arc::INFO, "Client side MCCs are loaded");
  Arc::MCC* client_entry = client_loader["soap"];
  if(!client_entry) {
    logger.msg(Arc::ERROR, "Client chain does not have entry point");
    return -1;
  };

  logger.msg(Arc::INFO, "Creating and sending request");
  Arc::NS count_ns;
  count_ns["count"]="urn:count";
  Arc::PayloadSOAP req(count_ns);
  Arc::XMLNode plus = req.NewChild("plus");  
  plus.NewChild("input1").NewChild("number1")="10";
  plus.NewChild("input2").NewChild("number2")="23";
  Arc::Message reqmsg;
  Arc::Message repmsg;
  reqmsg.Payload(&req);

  // It is a responsibility of code initiating first Message to
  // provide Context and Attributes as well.
  Arc::MessageAttributes attributes_req;
  Arc::MessageAttributes attributes_rep;
  Arc::MessageContext context;
  reqmsg.Attributes(&attributes_req);
  reqmsg.Context(&context);
  repmsg.Attributes(&attributes_rep);
  repmsg.Context(&context);  

  Arc::MCC_Status status = client_entry->process(reqmsg,repmsg);
  if(!status) {
    logger.msg(Arc::ERROR, "Request failed");
    std::cerr << "Status: " << std::string(status) << std::endl;
    return -1;
  };
  Arc::PayloadSOAP* resp = NULL;
  if(repmsg.Payload() == NULL) {
    logger.msg(Arc::ERROR, "There is no response");
    return -1;
  };
  try {
    resp = dynamic_cast<Arc::PayloadSOAP*>(repmsg.Payload());
  } catch(std::exception&) { };
  if(resp == NULL) {
    logger.msg(Arc::ERROR, "Response is not SOAP");
    return -1;
  };
  std::string xml;
  resp->GetXML(xml);
  std::cout << "XML: "<< xml << std::endl;
  std::cout << "Response: " << (std::string)((*resp)["countResponse"]) << std::endl;
  return 0;
}
