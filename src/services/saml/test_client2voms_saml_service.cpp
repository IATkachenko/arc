#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <signal.h>

#include <arc/ArcConfig.h>
#include <arc/Logger.h>
#include <arc/XMLNode.h>
#include <arc/message/SOAPEnvelope.h>
#include <arc/message/PayloadSOAP.h>
#include <arc/message/MCC.h>
#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/DateTime.h>
#include <arc/GUID.h>
#include <arc/credential/Credential.h>
#include <arc/communication/ClientInterface.h>
#include <arc/URL.h>

#include "../../hed/libs/xmlsec/XmlSecUtils.h"
#include "../../hed/libs/xmlsec/XMLSecNode.h"

#define SAML_NAMESPACE "urn:oasis:names:tc:SAML:2.0:assertion"
#define SAMLP_NAMESPACE "urn:oasis:names:tc:SAML:2.0:protocol"

#define XENC_NAMESPACE   "http://www.w3.org/2001/04/xmlenc#"
#define DSIG_NAMESPACE   "http://www.w3.org/2000/09/xmldsig#"

///A example about how to compose a SAML <AttributeQuery> and call the voms saml service, by
///using xmlsec library to compose <AttributeQuery> and process the <Response>.
int main(void) {
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGPIPE,SIG_IGN);
  Arc::Logger logger(Arc::Logger::rootLogger, "SAMLTest");
  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::rootLogger.addDestination(logcerr);
  
  // -------------------------------------------------------
  //    Compose request and send to voms saml service
  // -------------------------------------------------------
  //Compose request
  Arc::NS ns;
  ns["saml"] = SAML_NAMESPACE;
  ns["samlp"] = SAMLP_NAMESPACE;

  std::string cert("../../tests/echo/usercert.pem");
  std::string key("../../tests/echo/userkey-nopass.pem");
  std::string cafile("../../tests/echo/testcacert.pem");
  std::string cadir("../../tests/echo/certificates");

  Arc::Credential cred(cert, key, cadir, cafile);
  std::string local_dn_str = cred.GetDN();
  std::string local_dn;
  size_t pos1 = std::string::npos;
  size_t pos2;
  do { //The DN should be like "CN=test,O=UiO,ST=Oslo,C=NO", so we need to change the format here
    std::string str;
    pos2 = local_dn_str.find_last_of("/", pos1);
    if(pos2 != std::string::npos && pos1 == std::string::npos) { 
      str = local_dn_str.substr(pos2+1);
      local_dn.append(str);
      pos1 = pos2-1;
    }
    else if (pos2 != std::string::npos && pos1 != std::string::npos) {
      str = local_dn_str.substr(pos2+1, pos1-pos2);
      local_dn.append(str);
      pos1 = pos2-1;
    }
    if(pos2 != (std::string::npos+1)) local_dn.append(",");
  }while(pos2 != std::string::npos && pos2 != (std::string::npos+1));

  //Compose <samlp:AttributeQuery/>
  Arc::XMLNode attr_query(ns, "samlp:AttributeQuery");
  std::string query_id = Arc::UUID();
  attr_query.NewAttribute("ID") = query_id;
  Arc::Time t;
  std::string current_time = t.str(Arc::UTCTime);
  attr_query.NewAttribute("IssueInstant") = current_time;
  attr_query.NewAttribute("Version") = std::string("2.0");

  //<saml:Issuer/>
  std::string issuer_name = local_dn;
  Arc::XMLNode issuer = attr_query.NewChild("saml:Issuer");
  issuer = issuer_name;
  issuer.NewAttribute("Format") = std::string("urn:oasis:names:tc:SAML:1.1:nameid-format:x509SubjectName");

  //<saml:Subject/>
  Arc::XMLNode subject = attr_query.NewChild("saml:Subject");
  Arc::XMLNode name_id = subject.NewChild("saml:NameID");
  name_id.NewAttribute("Format")=std::string("urn:oasis:names:tc:SAML:1.1:nameid-format:x509SubjectName");
  name_id = local_dn;

  //Add one or more <Attribute>s into AttributeQuery here, which means the Requestor would
  //get these <Attribute>s from AA
  //<saml:Attribute/>
 /* 
  Arc::XMLNode attribute = attr_query.NewChild("saml:Attribute");
  attribute.NewAttribute("Name")=std::string("urn:oid:1.3.6.1.4.1.5923.1.1.1.6");
  attribute.NewAttribute("NameFormat")=std::string("urn:oasis:names:tc:SAML:2.0:attrname-format:uri");
  attribute.NewAttribute("FriendlyName")=std::string("eduPersonPrincipalName");
  */

  Arc::init_xmlsec();
  Arc::XMLSecNode attr_query_secnd(attr_query);
  std::string attr_query_idname("ID");
  attr_query_secnd.AddSignatureTemplate(attr_query_idname, Arc::XMLSecNode::RSA_SHA1);
  if(attr_query_secnd.SignNode(key,cert)) {
    std::cout<<"Succeeded to sign the signature under <samlp:AttributeQuery/>"<<std::endl;
  }


  std::string str;
  attr_query.GetXML(str);
  std::cout<<"AttributeQuery: "<<str<<std::endl;

  Arc::NS soap_ns;
  Arc::SOAPEnvelope envelope(soap_ns);
  envelope.NewChild(attr_query);
  Arc::PayloadSOAP request(envelope);

  std::string tmp;
  request.GetXML(tmp);
  std::cout<<"SOAP request: "<<tmp<<std::endl<<std::endl;
 
  // Send request
  Arc::MCCConfig cfg;
  if (!cert.empty())
    cfg.AddCertificate(cert);
  if (!key.empty())
    cfg.AddPrivateKey(key);
  if (!cafile.empty())
    cfg.AddCAFile(cafile);
  if (!cadir.empty())
    cfg.AddCADir(cadir);


  std::string path("/voms/test_saml/services/VOMSSaml");
  std::string service_url_str("https://omii002.cnaf.infn.it:8443");

//  std::string path("/aaservice");
//  std::string service_url_str("https://squark.uio.no:60001");

//  std::string path("/voms/saml/knowarc/services/AttributeAuthorityPortType");
//  std::string service_url_str("https://squark.uio.no:8444");

//  std::string path("/voms/saml/testvo/services/AttributeAuthorityPortType");
//  std::string service_url_str("https://127.0.0.1:8443");

//  std::string path("/voms/saml/omiieurope/services/AttributeAuthorityPortType");
//  std::string service_url_str("https://omii002.cnaf.infn.it:8443");

  Arc::URL service_url(service_url_str + path);
  Arc::ClientSOAP client(cfg, service_url);

  Arc::PayloadSOAP *response = NULL;
  Arc::MCC_Status status = client.process(&request,&response);
  if (!response) {
    logger.msg(Arc::ERROR, "Request failed: No response");
    return -1;
  }
  if (!status) {
    std::string tmp;
    response->GetXML(tmp);
    std::cout<<"SOAP Response: "<<tmp<<std::endl;
    logger.msg(Arc::ERROR, "Request failed: Error");
    return -1;
  }

  response->GetXML(str);
  std::cout<<"SOAP Response: "<<str<<std::endl<<std::endl;

  // -------------------------------------------------------
  //   Consume the response from saml voms service
  // -------------------------------------------------------
  //Consume the response from AA
  Arc::XMLNode attr_resp;
  
  //<samlp:Response/>
  attr_resp = (*response).Body().Child(0);
  attr_resp.GetXML(str);
  std::cout<<"SAML Response: "<<str<<std::endl<<std::endl;

  //TODO: metadata processing.
  //std::string aa_name = attr_resp["saml:Issuer"]; 
 
  //Check validity of the signature on <samlp:Response/>
  std::string resp_idname = "ID";
/*
  Arc::XMLSecNode attr_resp_secnode(attr_resp);
  if(attr_resp_secnode.VerifyNode(resp_idname, cafile, cadir)) {
    logger.msg(Arc::INFO, "Succeeded to verify the signature under <samlp:Response/>");
  }
  else {
    logger.msg(Arc::ERROR, "Failed to verify the signature under <samlp:Response/>");
    Arc::final_xmlsec(); return -1;
  }
*/

  //Check whether the "InResponseTo" is the same as the local ID

  std::string responseto_id = (std::string)(attr_resp.Attribute("InResponseTo"));
  if(query_id != responseto_id) {
    logger.msg(Arc::INFO, "The Response is not going to this end");
    Arc::final_xmlsec(); return -1;
  }


  std::string resp_time = attr_resp.Attribute("IssueInstant");

  //<samlp:Status/>
  std::string statuscode_value = attr_resp["Status"]["StatusCode"].Attribute("Value");
  if(statuscode_value == "urn:oasis:names:tc:SAML:2.0:status:Success")
    logger.msg(Arc::INFO, "The StatusCode is Success");
  else logger.msg(Arc::ERROR, "Can not find StatusCode");


  //<saml:Assertion/>
  Arc::XMLNode assertion = attr_resp["saml:Assertion"];

  //TODO: metadata processing.
  //std::string aa_name = assertion["saml:Issuer"];
 
  //Check validity of the signature on <saml:Assertion/>

  assertion.GetXML(tmp);
  std::cout<<"SAML Assertion: "<<tmp<<std::endl<<std::endl;


  std::string assertion_idname = "ID";
  Arc::XMLSecNode assertion_secnode(assertion);
  if(assertion_secnode.VerifyNode(assertion_idname, cafile, cadir,false)) {
    logger.msg(Arc::INFO, "Succeeded to verify the signature under <saml:Assertion/>");
  }
  else {
    logger.msg(Arc::ERROR, "Failed to verify the signature under <saml:Assertion/>");
    Arc::final_xmlsec(); return -1;
  }


  //<saml:Subject/>, TODO: 
  Arc::XMLNode subject_nd = assertion["saml:Subject"];

  //<saml:Condition/>, TODO: Condition checking
  Arc::XMLNode cond_nd = assertion["saml:Conditions"];

  //<saml:AttributeStatement/>
  Arc::XMLNode attr_statement = assertion["saml:AttributeStatement"];

  //<saml:Attribute/>
  Arc::XMLNode attr_nd;
  std::vector<std::string> attributes_value;
  for(int i=0;;i++) {
    attr_nd = attr_statement["saml:Attribute"][i];
    if(!attr_nd) break;

    std::string name = attr_nd.Attribute("Name");
    std::string nameformat = attr_nd.Attribute("NameFormat");

    for(int j=0;;j++) {
      Arc::XMLNode attr_value = attr_nd["saml:AttributeValue"][j];
      if(!attr_value) break;
      std::string str;
      str.append("Name=").append(name).append(" Value=").append(attr_value);
      attributes_value.push_back(str);
    }
  }

  for(int i=0; i<attributes_value.size(); i++) {
    std::cout<<"Attribute Value: "<<attributes_value[i]<<std::endl;  
  }

  Arc::final_xmlsec();
  return 0;
}
