#ifndef __ARC_CERTUTIL_H__
#define __ARC_CERTUTIL_H__

#include <string>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/stack.h>

#include <arc/credential/Proxycertinfo.h>

namespace ArcCredential {
  
    #define PROXYCERTINFO_V3      "1.3.6.1.4.1.3536.1.222"
    #ifdef HAVE_OPENSSL_PROXY
      #define PROXYCERTINFO_V4      "1.3.6.1.5.5.7.1.1400"
    #else
      #define PROXYCERTINFO_V4      "1.3.6.1.5.5.7.1.14"
    #endif
    #define PROXYCERTINFO_OPENSSL      "1.3.6.1.5.5.7.1.14"

 
    /** Certificate Types */
    typedef enum {
      /** A end entity certificate */
      CERT_TYPE_EEC,
      /** A CA certificate */
      CERT_TYPE_CA,
      /** A X.509 Proxy Certificate Profile (pre-RFC) compliant impersonation proxy */
      CERT_TYPE_GSI_3_IMPERSONATION_PROXY,
      /** A X.509 Proxy Certificate Profile (pre-RFC) compliant independent proxy */
      CERT_TYPE_GSI_3_INDEPENDENT_PROXY,
      /** A X.509 Proxy Certificate Profile (pre-RFC) compliant limited proxy */
      CERT_TYPE_GSI_3_LIMITED_PROXY,
      /** A X.509 Proxy Certificate Profile (pre-RFC) compliant restricted proxy */
      CERT_TYPE_GSI_3_RESTRICTED_PROXY,
      /** A legacy Globus impersonation proxy */
      CERT_TYPE_GSI_2_PROXY,
      /** A legacy Globus limited impersonation proxy */
      CERT_TYPE_GSI_2_LIMITED_PROXY,
      /** A X.509 Proxy Certificate Profile RFC compliant impersonation proxy; RFC inheritAll proxy */
      CERT_TYPE_RFC_IMPERSONATION_PROXY,
      /** A X.509 Proxy Certificate Profile RFC compliant independent proxy; RFC independent proxy */
      CERT_TYPE_RFC_INDEPENDENT_PROXY,
      /** A X.509 Proxy Certificate Profile RFC compliant limited proxy */
      CERT_TYPE_RFC_LIMITED_PROXY,
      /** A X.509 Proxy Certificate Profile RFC compliant restricted proxy */
      CERT_TYPE_RFC_RESTRICTED_PROXY,
      /** RFC anyLanguage proxy */
      CERT_TYPE_RFC_ANYLANGUAGE_PROXY
    } certType; 

    /** True if certificate type is one of proxy certificates */
    #define CERT_IS_PROXY(cert_type) \
        (cert_type == CERT_TYPE_GSI_3_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_GSI_3_INDEPENDENT_PROXY || \
         cert_type == CERT_TYPE_GSI_3_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_3_RESTRICTED_PROXY || \
         cert_type == CERT_TYPE_RFC_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_RFC_INDEPENDENT_PROXY || \
         cert_type == CERT_TYPE_RFC_LIMITED_PROXY || \
         cert_type == CERT_TYPE_RFC_RESTRICTED_PROXY || \
         cert_type == CERT_TYPE_RFC_ANYLANGUAGE_PROXY || \
         cert_type == CERT_TYPE_GSI_2_PROXY || \
         cert_type == CERT_TYPE_GSI_2_LIMITED_PROXY)

    /** True if certificate type is one of standard proxy certificates */
    #define CERT_IS_RFC_PROXY(cert_type) \
        (cert_type == CERT_TYPE_RFC_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_RFC_INDEPENDENT_PROXY || \
         cert_type == CERT_TYPE_RFC_LIMITED_PROXY || \
         cert_type == CERT_TYPE_RFC_RESTRICTED_PROXY || \
         cert_type == CERT_TYPE_RFC_ANYLANGUAGE_PROXY)

    /** True if certificate type is one of Globus newer proxy certificates */
    #define CERT_IS_GSI_3_PROXY(cert_type) \
        (cert_type == CERT_TYPE_GSI_3_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_GSI_3_INDEPENDENT_PROXY || \
         cert_type == CERT_TYPE_GSI_3_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_3_RESTRICTED_PROXY)

    /** True if certificate type is one of Globus older proxy certificates */
    #define CERT_IS_GSI_2_PROXY(cert_type) \
        (cert_type == CERT_TYPE_GSI_2_PROXY || \
         cert_type == CERT_TYPE_GSI_2_LIMITED_PROXY)

    #define CERT_IS_INDEPENDENT_PROXY(cert_type) \
        (cert_type == CERT_TYPE_RFC_INDEPENDENT_PROXY || \
         cert_type == CERT_TYPE_GSI_3_INDEPENDENT_PROXY)

    #define CERT_IS_RESTRICTED_PROXY(cert_type) \
        (cert_type == CERT_TYPE_RFC_RESTRICTED_PROXY || \
         cert_type == CERT_TYPE_GSI_3_RESTRICTED_PROXY)

    #define CERT_IS_LIMITED_PROXY(cert_type) \
        (cert_type == CERT_TYPE_RFC_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_3_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_2_LIMITED_PROXY)

    #define CERT_IS_IMPERSONATION_PROXY(cert_type) \
        (cert_type == CERT_TYPE_RFC_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_RFC_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_3_IMPERSONATION_PROXY || \
         cert_type == CERT_TYPE_GSI_3_LIMITED_PROXY || \
         cert_type == CERT_TYPE_GSI_2_PROXY || \
         cert_type == CERT_TYPE_GSI_2_LIMITED_PROXY)

    /* VERIFY_CTX_STORE_EX_DATA_IDX here could be temporal solution.
     * OpenSSL >= 098 has get_proxy_auth_ex_data_idx() which is 
     * specific for proxy extention.
     */
    #define VERIFY_CTX_STORE_EX_DATA_IDX  1

    typedef struct {
      X509_STORE_CTX *                    cert_store;
      int                                 cert_depth;
      int                                 proxy_depth;
      int                                 max_proxy_depth;
      int                                 limited_proxy;
      certType                            cert_type;
      STACK_OF(X509) *                    cert_chain; /*  X509 */
      std::string                         ca_dir;
      std::string                         ca_file;
      std::string                         proxy_policy; /* The policy attached to proxy cert info extension*/
    } cert_verify_context;

    int verify_cert_chain(X509* cert, STACK_OF(X509)** certchain, cert_verify_context* vctx);
    bool check_cert_type(X509* cert, certType& type);
    const char* certTypeToString(certType type);

}

#endif // __ARC_CERTUTIL_H__

