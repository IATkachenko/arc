#include <cctype>
#include <cstdlib>
#include <cstring>

#include <arc/Utils.h>
#include <arc/Base64.h>
#include <arc/StringConv.h>
#include <arc/external/cJSON/cJSON.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#include "jwse.h"
#include "jwse_private.h"


static EVP_PKEY* X509_get_privkey(X509*) {
  return NULL;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L

static int RSA_set0_key(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d) {
  /* If the fields n and e in r are NULL, the corresponding input
   * parameters MUST be non-NULL for n and e.  d may be
   * left NULL (in case only the public key is used).
   */
  if ((r->n == NULL && n == NULL)
      || (r->e == NULL && e == NULL))
    return 0;

  if (n != NULL) {
    BN_free(r->n);
    r->n = n;
  }
  if (e != NULL) {
    BN_free(r->e);
    r->e = e;
  }
  if (d != NULL) {
    BN_free(r->d);
    r->d = d;
  }

  return 1;
}

static RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey) {
  if(pkey)
    if((pkey->type == EVP_PKEY_RSA) || (pkey->type == EVP_PKEY_RSA2))
      return pkey->pkey.rsa;
  return NULL;
}

void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d) {
  if(n) *n = NULL;
  if(e) *e = NULL;
  if(d) *d = NULL;
  if(!r) return;
  if(n) *n = r->n;
  if(e) *e = r->e;
  if(d) *d = r->d;
}

#endif


namespace Arc {
 

  char const * const JWSE::HeaderNameX509CertChain = "x5c";
  char const * const JWSE::HeaderNameJSONWebKey = "jwk";

  static void sk_x509_deallocate(STACK_OF(X509)* o) {
    sk_X509_pop_free(o, X509_free);
  }

  static void BIO_deallocate(BIO* bio) {
    (void)BIO_free(bio);
  }

  static EVP_PKEY* jwkECParse(cJSON* jwkObject) {
    return NULL;
  }

  static EVP_PKEY* jwkRSAParse(cJSON* jwkObject) {
    cJSON* modulusObject = cJSON_GetObjectItem(jwkObject, "n");
    cJSON* exponentObject = cJSON_GetObjectItem(jwkObject, "e");
    if((modulusObject == NULL) || (exponentObject == NULL)) return NULL;
    if((modulusObject->type != cJSON_String) || (exponentObject->type != cJSON_String)) return NULL;
    std::string modulus = Base64::decodeURLSafe(modulusObject->valuestring);
    std::string exponent = Base64::decodeURLSafe(exponentObject->valuestring);
    AutoPointer<RSA> rsaKey(RSA_new(),&RSA_free);
    if(!rsaKey) return NULL;
    BIGNUM* n(NULL);
    BIGNUM* e(NULL);
    if((n = BN_bin2bn(reinterpret_cast<unsigned char const *>(modulus.c_str()), modulus.length(), NULL)) == NULL) return NULL;
    if((e = BN_bin2bn(reinterpret_cast<unsigned char const *>(exponent.c_str()), exponent.length(), NULL)) == NULL) return NULL;
    RSA_set0_key(rsaKey.Ptr(), n, e, NULL);
    AutoPointer<EVP_PKEY> evpKey(EVP_PKEY_new(), &EVP_PKEY_free);
    if(!evpKey) return NULL;
    if(EVP_PKEY_set1_RSA(evpKey.Ptr(), rsaKey.Ptr()) != 1) return NULL;
    return evpKey.Release();
  }

  static EVP_PKEY* jwkOctParse(cJSON* jwkObject) {
    cJSON* keyObject = cJSON_GetObjectItem(jwkObject, "k");
    cJSON* algObject = cJSON_GetObjectItem(jwkObject, "alg");
    if((keyObject == NULL) || (algObject == NULL)) return NULL;
    if((keyObject->type != cJSON_String) || (algObject->type != cJSON_String)) return NULL;
    std::string key = Base64::decodeURLSafe(keyObject->valuestring);
    // It looks like RFC does not define any "alg" values with "JWK" usage.
    return NULL;
  }

  static bool jwkParse(cJSON* jwkObject, JWSEKeyHolder& keyHolder) {
    if(jwkObject->type != cJSON_Object) return false;

    cJSON* ktyObject = cJSON_GetObjectItem(jwkObject, "kty");
    if(ktyObject == NULL) return false;
    if(ktyObject->type != cJSON_String) return false;

    EVP_PKEY* publicKey(NULL);
    if(strcmp(ktyObject->valuestring, "EC") == 0) {    
      publicKey = jwkECParse(jwkObject);
    } else if(strcmp(ktyObject->valuestring, "RSA") == 0) {    
      publicKey = jwkRSAParse(jwkObject);
    } else if(strcmp(ktyObject->valuestring, "oct") == 0) {    
      publicKey = jwkOctParse(jwkObject);
    }
    if(publicKey == NULL)
      return false;

    keyHolder.PublicKey(publicKey);

    return true;
  }


  static bool jwkRSASerialize(cJSON* jwkObject, EVP_PKEY* key) {
    if(!key) return false;
    RSA* rsaKey = EVP_PKEY_get0_RSA(key);
    if(!rsaKey) return false;
    BIGNUM const* n(NULL);
    BIGNUM const* e(NULL);
    BIGNUM const* d(NULL);
    RSA_get0_key(rsaKey, &n, &e, &d);
    if((!n) || (!e)) return false;

    std::string modulus;
    modulus.resize(BN_num_bytes(n));
    modulus.resize(BN_bn2bin(n, reinterpret_cast<unsigned char*>(const_cast<char*>(modulus.c_str()))));
    std::string exponent;
    exponent.resize(BN_num_bytes(e));
    exponent.resize(BN_bn2bin(e, reinterpret_cast<unsigned char*>(const_cast<char*>(exponent.c_str()))));
    if(modulus.empty() || exponent.empty()) return false;

    cJSON_AddStringToObject(jwkObject, "n", Base64::encodeURLSafe(modulus.c_str()).c_str());
    cJSON_AddStringToObject(jwkObject, "e", Base64::encodeURLSafe(exponent.c_str()).c_str());

    return true;
  }

  static cJSON* jwkSerialize(JWSEKeyHolder& keyHolder) {
    AutoPointer<cJSON> jwkObject(cJSON_CreateObject(), &cJSON_Delete);
    if(jwkRSASerialize(jwkObject.Ptr(), keyHolder.PublicKey())) {
      return jwkObject.Release();
    };
    return NULL;
  }


  static bool x5cParse(cJSON* x5cObject, JWSEKeyHolder& keyHolder) {
    if(x5cObject->type != cJSON_Array) return false;
    // Collect the chain
    AutoPointer<X509> maincert(NULL, *X509_free);
    AutoPointer<STACK_OF(X509)> certchain(sk_X509_new_null(), &sk_x509_deallocate);
    for(int certN = 0; ; ++certN) {
      cJSON* certObject = cJSON_GetArrayItem(x5cObject, certN);
      if(certObject == NULL) break;
      if(certObject->type != cJSON_String) return NULL;
      // It should be PEM encoded certificate in single string and without header/footer           
      AutoPointer<BIO> mem(BIO_new(BIO_s_mem()), &BIO_deallocate);
      if(!mem) return NULL;
      BIO_puts(mem.Ptr(), "-----BEGIN CERTIFICATE-----\n");
      BIO_puts(mem.Ptr(), certObject->valuestring);
      BIO_puts(mem.Ptr(), "\n-----END CERTIFICATE-----");
      AutoPointer<X509> cert(PEM_read_bio_X509(mem.Ptr(), NULL, NULL, NULL), *X509_free);
      if(!cert) return false;
      if(certN == 0) {
        maincert = cert.Release();
      } else {
        if(sk_X509_insert(certchain.Ptr(), cert.Release(), certN) == 0) return false; // does sk_X509_insert increase reference?
        (void)cert.Release();
      }
    }
    // Verify the chain





    // Remember collected information
    keyHolder.Certificate(maincert.Release());
    if(sk_X509_num(certchain.Ptr()) > 0)
      keyHolder.CertificateChain(certchain.Release());

    return true;
  }

  static cJSON* x5cSerialize(JWSEKeyHolder& keyHolder) {
    X509* cert = keyHolder.Certificate();
    if(!cert) return NULL;
    const STACK_OF(X509)* chain = keyHolder.CertificateChain();

    AutoPointer<cJSON> x5cObject(cJSON_CreateArray(), &cJSON_Delete);
    int certN = 0;
    while(true) {
      AutoPointer<BIO> mem(BIO_new(BIO_s_mem()), &BIO_deallocate);
      if(!PEM_write_bio_X509(mem.Ptr(), cert))
        return NULL;
      std::string certStr;
      std::string::size_type certStrSize = 0;
      while(true) {
        certStr.resize(certStrSize+256);
        int l = BIO_gets(mem.Ptr(), const_cast<char*>(certStr.c_str()+certStrSize), certStr.length()-certStrSize);
        if(l < 0) l = 0;
        certStrSize+=l;
        certStr.resize(certStrSize);
        if(l == 0) break;
      };

      // Must remove header, footer and line breaks
      std::vector<std::string> lines;
      Arc::tokenize(certStr, lines, "\n");
      if(lines.size() < 2) return NULL;
      if(lines[0].find("BEGIN CERTIFICATE") == std::string::npos) return NULL;
      lines.erase(lines.begin());
      if(lines[lines.size()-1].find("END CERTIFICATE") == std::string::npos) return NULL;
      certStr = Arc::join(lines, "");

      cJSON_AddItemToArray(x5cObject.Ptr(), cJSON_CreateString(certStr.c_str()));
      if(!chain) break;
      if(certN >= sk_X509_num(chain)) break;
      cert = sk_X509_value(keyHolder.CertificateChain(), certN);
      if(!cert) break;
    };

    return x5cObject.Release();
  }

  bool JWSE::ExtractPublicKey() const {
    key_.Release();
    AutoPointer<JWSEKeyHolder> key(new JWSEKeyHolder());

    // So far we are going to support only embedded keys - jwk and x5c
    cJSON* x5cObject = cJSON_GetObjectItem(header_.Ptr(), HeaderNameX509CertChain);
    cJSON* jwkObject = cJSON_GetObjectItem(header_.Ptr(), HeaderNameJSONWebKey);
    if(x5cObject != NULL) {
      if(x5cParse(x5cObject, *key)) {
        key_ = key.Release();
        return true;
      }
    } else if(jwkObject != NULL) {
      if(jwkParse(jwkObject, *key)) {
        key_ = key.Release();
        return true;
      }
    } else {
      return true; // No key - no processing
    };

    return false;
  }
  
  bool JWSE::InsertPublicKey() const {
    cJSON_DeleteItemFromObject(header_.Ptr(), HeaderNameX509CertChain);
    cJSON_DeleteItemFromObject(header_.Ptr(), HeaderNameJSONWebKey);
    if(key_ && key_->Certificate()) {
      cJSON* x5cObject = x5cSerialize(*key_);
      if(x5cObject) {
        cJSON_AddItemToObject(header_.Ptr(), HeaderNameX509CertChain, x5cObject);
        return true;
      }
    } else if(key_ && key_->PublicKey()) {
      cJSON* jwkObject = jwkSerialize(*key_);
      if(jwkObject) {
        cJSON_AddItemToObject(header_.Ptr(), HeaderNameJSONWebKey, jwkObject);
        return true;
      }
    } else {
      return true; // No key - no processing
    };

    return false;
  }

  // ---------------------------------------------------------------------------------------------

  JWSEKeyHolder::JWSEKeyHolder(): certificate_(NULL), certificateChain_(NULL), publicKey_(NULL) {
  }

  JWSEKeyHolder::~JWSEKeyHolder() {
    if(publicKey_) EVP_PKEY_free(publicKey_);
    publicKey_ = NULL;
    if(certificate_) X509_free(certificate_);
    certificate_ = NULL;
    if(certificateChain_) sk_x509_deallocate(certificateChain_);
    certificateChain_ = NULL;
  }

  EVP_PKEY* JWSEKeyHolder::PublicKey() {
    return publicKey_;
  }

  void JWSEKeyHolder::PublicKey(EVP_PKEY* publicKey) {
    if(publicKey_) EVP_PKEY_free(publicKey_);
    publicKey_ = publicKey;
    if(certificate_) X509_free(certificate_);
    certificate_ = NULL;
  }

  EVP_PKEY* JWSEKeyHolder::PrivateKey() {
    return privateKey_;
  }

  void JWSEKeyHolder::PrivateKey(EVP_PKEY* privateKey) {
    if(privateKey_) EVP_PKEY_free(privateKey_);
    privateKey_ = privateKey;
    if(certificate_) X509_free(certificate_);
    certificate_ = NULL;
  }

  X509* JWSEKeyHolder::Certificate() {
    return certificate_;
  }

  void JWSEKeyHolder::Certificate(X509* certificate) {
    if(publicKey_) EVP_PKEY_free(publicKey_);
    // call increases reference count
    publicKey_ = certificate ? X509_get_pubkey(certificate) : NULL;
    if(privateKey_) EVP_PKEY_free(privateKey_);
    // call increases reference count
    publicKey_ = certificate ? X509_get_privkey(certificate) : NULL;
    if(certificate_) X509_free(certificate_);
    certificate_ = certificate;
  }

  STACK_OF(X509)* JWSEKeyHolder::CertificateChain() {
    return certificateChain_;
  }

  void JWSEKeyHolder::CertificateChain(STACK_OF(X509)* certificateChain) {
    if(certificateChain_) sk_x509_deallocate(certificateChain_);
    certificateChain_ = certificateChain;
  }


} // namespace Arc
