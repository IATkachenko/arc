#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include "GlobusSigningPolicy.h"

#include "PayloadTLSMCC.h"
#include <openssl/err.h>
#include <glibmm/miscutils.h>
#include <arc/DateTime.h>

namespace Arc {

static void* ex_data_id = (void*)"ARC_MCC_Payload_TLS";
int PayloadTLSMCC::ex_data_index_ = -1;

#ifndef HAVE_OPENSSL_X509_VERIFY_PARAM
#define FLAG_CRL_DISABLED (0x1)

static unsigned long get_flag_STORE_CTX(X509_STORE_CTX* container) {
  PayloadTLSMCC* it = PayloadTLSMCC::RetrieveInstance(container);
  if(!it) return 0;
  return it->Flags();
}

static void set_flag_STORE_CTX(X509_STORE_CTX* container,unsigned long flags) {
  PayloadTLSMCC* it = PayloadTLSMCC::RetrieveInstance(container);
  if(!it) return;
  it->Flags(flags);
}
#endif

Time asn1_to_utctime(const ASN1_UTCTIME *s) {
  std::string t_str;
  if(s->type == V_ASN1_UTCTIME) {
    t_str.append("20");
    t_str.append((char*)(s->data));
  }
  else {//V_ASN1_GENERALIZEDTIME
    t_str.append((char*)(s->data));
  }
  return Time(t_str);
}

// This callback implements additional verification
// algorithms not present in OpenSSL
static int verify_callback(int ok,X509_STORE_CTX *sctx) {
/*
#ifdef HAVE_OPENSSL_X509_VERIFY_PARAM
  unsigned long flag = get_flag_STORE_CTX(sctx);
  if(!(flag & FLAG_CRL_DISABLED)) {
    // Not sure if this will work
#if OPENSSL_VERSION_NUMBER < 0x00908000 
    if(!(sctx->flags & X509_V_FLAG_CRL_CHECK)) {
      sctx->flags |= X509_V_FLAG_CRL_CHECK;
#else
    if(!(sctx->param->flags & X509_V_FLAG_CRL_CHECK)) {
      sctx->param->flags |= X509_V_FLAG_CRL_CHECK;
#endif
      ok=X509_verify_cert(sctx);
      return ok;
    };
  };
#endif
*/
  if (ok != 1) {
    int err = X509_STORE_CTX_get_error(sctx);
    switch(err) {
#ifdef HAVE_OPENSSL_PROXY
      case X509_V_ERR_PROXY_CERTIFICATES_NOT_ALLOWED: {
        // This shouldn't happen here because flags are already set
        // to allow proxy. But one can never know because used flag
        // setting method is undocumented.
        X509_STORE_CTX_set_flags(sctx,X509_V_FLAG_ALLOW_PROXY_CERTS);
        // Calling X509_verify_cert will cause a recursive call to verify_callback.
        // But there should be no loop because PROXY_CERTIFICATES_NOT_ALLOWED error
        // can't happen anymore.
        //ok=X509_verify_cert(sctx);
        ok=1;
        if(ok == 1) X509_STORE_CTX_set_error(sctx,X509_V_OK);
      }; break;
#endif
      case X509_V_ERR_UNABLE_TO_GET_CRL: {
        // Missing CRL is not an error (TODO: make it configurable)
        // Consider that to be a policy of site like Globus does
        // Not sure of there is need for recursive X509_verify_cert() here.
        // It looks like openssl runs tests sequentially for whole chain.
        // But not sure if behavior is same in all versions.
#ifdef HAVE_OPENSSL_X509_VERIFY_PARAM
        if(sctx->param) {
          X509_VERIFY_PARAM_clear_flags(sctx->param,X509_V_FLAG_CRL_CHECK);
          //ok=X509_verify_cert(sctx);
          //X509_VERIFY_PARAM_set_flags(sctx->param,X509_V_FLAG_CRL_CHECK);
          ok=1;
          if(ok == 1) X509_STORE_CTX_set_error(sctx,X509_V_OK);
        };
#else

#if OPENSSL_VERSION_NUMBER < 0x00908000
        sctx->flags &= ~X509_V_FLAG_CRL_CHECK;
#else
        sctx->param->flags &= ~X509_V_FLAG_CRL_CHECK;
#endif

        set_flag_STORE_CTX(sctx,get_flag_STORE_CTX(sctx) | FLAG_CRL_DISABLED);
        //ok=X509_verify_cert(sctx);
        //sctx->flags |= X509_V_FLAG_CRL_CHECK;
        //set_flag_STORE_CTX(sctx,get_flag_STORE_CTX(sctx) & (~FLAG_CRL_DISABLED));
        ok=1;
        if(ok == 1) X509_STORE_CTX_set_error(sctx,X509_V_OK);
#endif
      }; break;
      default: {
        PayloadTLSMCC::HandleError(Logger::getRootLogger(),err);
      }; break;
    };
  };
  if(ok == 1) {
    // Do additional verification here.
    PayloadTLSMCC* it = PayloadTLSMCC::RetrieveInstance(sctx);
    if(it == NULL) {
      Logger::getRootLogger().msg(WARNING,"Failed to retrieve link to TLS stream. Additional policy matching is skipped.");
    } else {
      // Globus signing policy
      // Do not apply to proxies and self-signed CAs.
      if((it->Config().GlobusPolicy()) && (!(it->Config().CADir().empty()))) {
        X509* cert = X509_STORE_CTX_get_current_cert(sctx);
#ifdef HAVE_OPENSSL_PROXY
        int pos = X509_get_ext_by_NID(cert,NID_proxyCertInfo,-1);
        if(pos < 0) {
#else
        {
#endif
          std::istream* in = open_globus_policy(X509_get_issuer_name(cert),it->Config().CADir());
          if(in) {
            if(!match_globus_policy(*in,X509_get_issuer_name(cert),X509_get_subject_name(cert))) {
              char* s = X509_NAME_oneline(X509_get_subject_name(cert),NULL,0);
              Logger::getRootLogger().msg(ERROR,"Certificate %s failed Globus signing policy",s);
              OPENSSL_free(s);
              ok=0;
              X509_STORE_CTX_set_error(sctx,X509_V_ERR_SUBJECT_ISSUER_MISMATCH);            };
            delete in;
          };
        };
      };
    };
    //Check the left validity time of the peer certificate;
    //Give warning if the certificate is going to be expired 
    //in a while of time
    char * subject_name = X509_NAME_oneline(X509_get_subject_name(sctx->current_cert), 0, 0);
    Arc::Period timeleft;
    timeleft = asn1_to_utctime(X509_get_notAfter(sctx->current_cert)) - Time();
#ifdef HAVE_OPENSSL_PROXY
    int pos = X509_get_ext_by_NID(sctx->current_cert, NID_proxyCertInfo,-1);
    if(pos >= 0) {           //for proxy certificate, give warning 1 hour in advance
      if(timeleft <= 3600)
        Logger::getRootLogger().msg(WARNING,"Certificate %s will be expired in %s", subject_name, timeleft.istr());
    }
#else
    if(timeleft <= 5*24*3600) //for EEC certificate, give warning 5 days in advance
      Logger::getRootLogger().msg(WARNING,"Certificate %s will be expired in %s", subject_name, timeleft.istr());
#endif
    OPENSSL_free(subject_name);
  };
  return ok;
}

// This callback is just a placeholder. We do not expect 
// encrypted private keys here.
static int no_passphrase_callback(char*, int, int, void*) {
   return -1;
}

static void config_VOMS_add(XMLNode cfg,std::vector<std::string>& vomscert_trust_dn) {
  XMLNode nd = cfg["VOMSCertTrustDNChain"];
  for(;(bool)nd;++nd) {
    XMLNode cnd = nd["VOMSCertTrustDN"];
    for(;(bool)cnd;++cnd) {
      vomscert_trust_dn.push_back((std::string)cnd);
    }
    vomscert_trust_dn.push_back("----NEXT CHAIN----");
  }
}

// This class is collection of configuration information

ConfigTLSMCC::ConfigTLSMCC(XMLNode cfg,Logger& logger,bool client) {
  client_authn_ = true;
  cert_file_ = (std::string)(cfg["CertificatePath"]);
  key_file_ = (std::string)(cfg["KeyPath"]);
  ca_file_ = (std::string)(cfg["CACertificatePath"]);
  ca_dir_ = (std::string)(cfg["CACertificatesDir"]);
  globus_policy_ = (((std::string)(cfg["CACertificatesDir"].Attribute("PolicyGlobus"))) == "true");
  handshake_ = (cfg["Handshake"] == "SSLv3")?ssl3_handshake:tls_handshake;
  proxy_file_ = (std::string)(cfg["ProxyPath"]);
  
  std::vector<std::string> gridSecDir (2);
  gridSecDir[0] = G_DIR_SEPARATOR_S + std::string("etc");
  gridSecDir[1] = "grid-security";
  std::string gridSecurityDir = Glib::build_path(G_DIR_SEPARATOR_S, gridSecDir);

  if(!client) {
    
    if(cert_file_.empty()) cert_file_= Glib::build_filename(gridSecurityDir, "hostcert.pem");
    if(key_file_.empty()) key_file_= Glib::build_filename(gridSecurityDir, "hostkey.pem");
    // Use VOMS trust DN of server certificates specified in configuration
    config_VOMS_add(cfg,vomscert_trust_dn_);
    // Look for those configured in separate files
    // TODO: should those file be reread on every connection
    XMLNode locnd = cfg["VOMSCertTrustDNChainsLocation"];
    for(;(bool)locnd;++locnd) {
      std::string filename = (std::string)locnd;
      std::ifstream file(filename.c_str());
      if (!file) {
        logger.msg(ERROR, "Can not read file %s with list of trusted VOMS DNs", filename);
        continue;
      };
      XMLNode node;
      file >> node;
      config_VOMS_add(node,vomscert_trust_dn_);
    };
    //If ClientAuthn is explicitly set to be "false" in configuration,
    //then client/authentication is not required, which means client 
    //side does not need to provide certificate and key in its configuration.
    //The default value of ClientAuthn is "true"
    if (((std::string)((cfg)["ClientAuthn"])) == "false") client_authn_ = false;
  } else {
    //If both CertificatePath and ProxyPath have not beed configured, 
    //client side can not provide certificate for server side. Then server 
    //side should not require client authentication
    if(cert_file_.empty() && proxy_file_.empty()) client_authn_ = false;
  };
  if(ca_dir_.empty() && ca_file_.empty()) ca_dir_= gridSecurityDir + G_DIR_SEPARATOR_S + "certificates";
  if(!proxy_file_.empty()) { key_file_=proxy_file_; cert_file_=proxy_file_; };
}

bool ConfigTLSMCC::Set(SSL_CTX* sslctx,Logger& logger) {
  int r;
  if((!ca_file_.empty()) || (!ca_dir_.empty())) {
    if(!SSL_CTX_load_verify_locations(sslctx, ca_file_.empty()?NULL:ca_file_.c_str(), ca_dir_.empty()?NULL:ca_dir_.c_str())) {
      logger.msg(ERROR, "Can not assign CA location - %s",ca_dir_.empty()?ca_file_:ca_dir_);
      PayloadTLSStream::HandleError(logger);
      return false;
    };
  };
  if(!cert_file_.empty()) {
    // Try to load proxy then PEM and then ASN1 certificate
    if((SSL_CTX_use_certificate_chain_file(sslctx,cert_file_.c_str()) != 1) &&
       (SSL_CTX_use_certificate_file(sslctx,cert_file_.c_str(),SSL_FILETYPE_PEM) != 1) &&
       (SSL_CTX_use_certificate_file(sslctx,cert_file_.c_str(),SSL_FILETYPE_ASN1) != 1)) {
      logger.msg(ERROR, "Can not load certificate file - %s",cert_file_);
      PayloadTLSStream::HandleError(logger);
      return false;
    };
  };
  if(!key_file_.empty()) {
    if((SSL_CTX_use_PrivateKey_file(sslctx,key_file_.c_str(),SSL_FILETYPE_PEM) != 1) &&
       (SSL_CTX_use_PrivateKey_file(sslctx,key_file_.c_str(),SSL_FILETYPE_ASN1) != 1)) {
      logger.msg(ERROR, "Can not load key file - %s",key_file_);
      PayloadTLSStream::HandleError(logger);
      return false;
    };
  };
  if((!key_file_.empty()) && (!cert_file_.empty()))
    if(!(SSL_CTX_check_private_key(sslctx))) {
      logger.msg(ERROR, "Private key %s does not match certificate %s",key_file_,cert_file_);
      PayloadTLSStream::HandleError(logger);
      return false;
    };
  return true;
}


bool PayloadTLSMCC::StoreInstance(void) {
   if(ex_data_index_ == -1) {
      // In case of race condition we will have 2 indices assigned - harmless
      ex_data_index_=SSL_CTX_get_ex_new_index(0,ex_data_id,NULL,NULL,NULL);
   };
   if(ex_data_index_ == -1) {
      Logger::getRootLogger().msg(ERROR,"Failed to store application data");
      return false;
   };
   SSL_CTX_set_ex_data(sslctx_,ex_data_index_,this);
   return true;
}

PayloadTLSMCC* PayloadTLSMCC::RetrieveInstance(X509_STORE_CTX* container) {
  PayloadTLSMCC* it = NULL;
  if(ex_data_index_ != -1) {
    SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(container,SSL_get_ex_data_X509_STORE_CTX_idx());
    if(ssl != NULL) {
      SSL_CTX* ssl_ctx = SSL_get_SSL_CTX(ssl);
      if(ssl_ctx != NULL) {
        it = (PayloadTLSMCC*)SSL_CTX_get_ex_data(ssl_ctx,ex_data_index_);
      }
    };
  };
  if(it == NULL) {
    Logger::getRootLogger().msg(ERROR,"Failed to retrieve application data from OpenSSL");
  };
  return it;
}

PayloadTLSMCC::PayloadTLSMCC(MCCInterface* mcc, const ConfigTLSMCC& cfg, Logger& logger):PayloadTLSStream(logger),sslctx_(NULL),config_(cfg) { 
   // Client mode
   int err = SSL_ERROR_NONE;
   master_=true;
   // Creating BIO for communication through stream which it will
   // extract from provided MCC
   BIO* bio = BIO_new_MCC(mcc); 
   // Initialize the SSL Context object
   if(cfg.IfTLSHandshake()) {
     sslctx_=SSL_CTX_new(SSLv23_client_method());
   } else {
     sslctx_=SSL_CTX_new(SSLv3_client_method());
   };
   if(sslctx_==NULL){
      logger.msg(ERROR, "Can not create the SSL Context object");
      goto error;
   };
   SSL_CTX_set_mode(sslctx_,SSL_MODE_ENABLE_PARTIAL_WRITE);
   SSL_CTX_set_session_cache_mode(sslctx_,SSL_SESS_CACHE_OFF);
   if(!config_.Set(sslctx_,logger_)) goto error;
   SSL_CTX_set_verify(sslctx_, SSL_VERIFY_PEER |  SSL_VERIFY_FAIL_IF_NO_PEER_CERT, &verify_callback);

   // Allow proxies, request CRL check
#ifdef HAVE_OPENSSL_X509_VERIFY_PARAM
   if(sslctx_->param == NULL) {
      logger.msg(ERROR,"Can't set OpenSSL verify flags");
      goto error;
   } else {
#ifdef HAVE_OPENSSL_PROXY
      if(sslctx_->param) X509_VERIFY_PARAM_set_flags(sslctx_->param,X509_V_FLAG_CRL_CHECK | X509_V_FLAG_ALLOW_PROXY_CERTS);
#else
      if(sslctx_->param) X509_VERIFY_PARAM_set_flags(sslctx_->param,X509_V_FLAG_CRL_CHECK);
#endif
   };
#endif
   StoreInstance();
#ifdef SSL_OP_NO_TICKET
   SSL_CTX_set_options(sslctx_, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2 | SSL_OP_ALL | SSL_OP_NO_TICKET);
#else
   SSL_CTX_set_options(sslctx_, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2 | SSL_OP_ALL);
#endif

   SSL_CTX_set_default_passwd_cb(sslctx_, no_passphrase_callback);
   /* Get DN from certificate, and put it into message's attribute */

   // Creating SSL object for handling connection
   ssl_ = SSL_new(sslctx_);
   if (ssl_ == NULL){
      logger.msg(ERROR, "Can not create the SSL object");
      goto error;
   };
   SSL_set_bio(ssl_,bio,bio); bio=NULL;
   //SSL_set_connect_state(ssl_);
   if((err=SSL_connect(ssl_)) != 1) {
      logger.msg(ERROR, "Failed to establish SSL connection");
      goto error;
   };
   // if(SSL_in_init(ssl_)){
   //handle error
   // }
   return;
error:
   HandleError(err);
   if(bio) BIO_free(bio);
   if(ssl_) SSL_free(ssl_); ssl_=NULL;
   if(sslctx_) SSL_CTX_free(sslctx_); sslctx_=NULL;
   return;
}

PayloadTLSMCC::PayloadTLSMCC(PayloadStreamInterface* stream, const ConfigTLSMCC& cfg, Logger& logger):PayloadTLSStream(logger),sslctx_(NULL),config_(cfg) {
   // Server mode
   int err = SSL_ERROR_NONE;
   master_=true;
   // Creating BIO for communication through provided stream
   BIO* bio = BIO_new_MCC(stream);
   // Initialize the SSL Context object
   if(cfg.IfTLSHandshake()) {
     sslctx_=SSL_CTX_new(SSLv23_server_method());
   } else {
     sslctx_=SSL_CTX_new(SSLv3_server_method());
   };
   if(sslctx_==NULL){
      logger.msg(ERROR, "Can not create the SSL Context object");
      goto error;
   };
   SSL_CTX_set_mode(sslctx_,SSL_MODE_ENABLE_PARTIAL_WRITE);
   SSL_CTX_set_session_cache_mode(sslctx_,SSL_SESS_CACHE_OFF);
   if(config_.IfClientAuthn()) {
     SSL_CTX_set_verify(sslctx_, SSL_VERIFY_PEER |  SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE, &verify_callback);
   }
   else {
     SSL_CTX_set_verify(sslctx_, SSL_VERIFY_NONE, NULL);
   }
   if(!config_.Set(sslctx_,logger_)) goto error;

   // Allow proxies, request CRL check
#ifdef HAVE_OPENSSL_X509_VERIFY_PARAM
   if(sslctx_->param == NULL) {
      logger.msg(ERROR,"Can't set OpenSSL verify flags");
      goto error;
   } else {
#ifdef HAVE_OPENSSL_PROXY
      if(sslctx_->param) X509_VERIFY_PARAM_set_flags(sslctx_->param,X509_V_FLAG_CRL_CHECK | X509_V_FLAG_ALLOW_PROXY_CERTS);
#else
      if(sslctx_->param) X509_VERIFY_PARAM_set_flags(sslctx_->param,X509_V_FLAG_CRL_CHECK);
#endif
   };
#endif
   StoreInstance();
   SSL_CTX_set_options(sslctx_, SSL_OP_SINGLE_DH_USE | SSL_OP_NO_SSLv2 | SSL_OP_ALL);
   SSL_CTX_set_default_passwd_cb(sslctx_, no_passphrase_callback);

   // Creating SSL object for handling connection
   ssl_ = SSL_new(sslctx_);
   if (ssl_ == NULL){
      logger.msg(ERROR, "Can not create the SSL object");
      goto error;
   };
   SSL_set_bio(ssl_,bio,bio); bio=NULL;
   //SSL_set_accept_state(ssl_);
   if((err=SSL_accept(ssl_)) != 1) {
      logger.msg(ERROR, "Failed to accept SSL connection");
      goto error;
   };
   //handle error
   // if(SSL_in_init(ssl_)){
   //handle error
   // }
   return;
error:
   HandleError(err);
   if(bio) BIO_free(bio);
   if(ssl_) SSL_free(ssl_); ssl_=NULL;
   if(sslctx_) SSL_CTX_free(sslctx_); sslctx_=NULL;
   return;
}

PayloadTLSMCC::PayloadTLSMCC(PayloadTLSMCC& stream):
  PayloadTLSStream(stream), config_(stream.config_) {
   master_=false;
   sslctx_=stream.sslctx_; 
   ssl_=stream.ssl_; 
}


PayloadTLSMCC::~PayloadTLSMCC(void) {
  if (!master_)
    return;
  if (ssl_) {
    if (SSL_shutdown(ssl_) < 0)
      logger_.msg(ERROR, "Failed to shut down SSL");
    SSL_free(ssl_);
    ssl_ = NULL;
  }
  if(sslctx_) {
    SSL_CTX_free(sslctx_);
    sslctx_ = NULL;
  }
}

} // namespace Arc
