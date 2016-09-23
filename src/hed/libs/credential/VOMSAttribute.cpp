#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstring>

#include "VOMSAttribute.h"

#include "VOMSUtil.h"

namespace ArcCredential {


IMPLEMENT_ASN1_FUNCTIONS(AC_DIGEST)
ASN1_SEQUENCE(AC_DIGEST) = {
  ASN1_SIMPLE(AC_DIGEST, type, ASN1_ENUMERATED),
  ASN1_SIMPLE(AC_DIGEST, oid, ASN1_OBJECT),
  ASN1_SIMPLE(AC_DIGEST, algor, X509_ALGOR),
  ASN1_SIMPLE(AC_DIGEST, digest, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(AC_DIGEST)

IMPLEMENT_ASN1_FUNCTIONS(AC_IS)
ASN1_SEQUENCE(AC_IS) = {
  ASN1_SIMPLE(AC_IS, issuer, GENERAL_NAMES),
  ASN1_SIMPLE(AC_IS, serial, ASN1_INTEGER),
  ASN1_IMP_OPT(AC_IS, uid, ASN1_BIT_STRING, V_ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(AC_IS)

IMPLEMENT_ASN1_FUNCTIONS(AC_FORM)
ASN1_SEQUENCE(AC_FORM) = {
  ASN1_SIMPLE(AC_FORM, names, GENERAL_NAMES),
  ASN1_IMP_OPT(AC_FORM, is, AC_IS, 0),
  ASN1_IMP_OPT(AC_FORM, digest, AC_DIGEST, 1)
} ASN1_SEQUENCE_END(AC_FORM)

IMPLEMENT_ASN1_FUNCTIONS(AC_ACI)
ASN1_SEQUENCE(AC_ACI) = {
  ASN1_IMP_OPT(AC_ACI, form, AC_FORM, 0)
} ASN1_SEQUENCE_END(AC_ACI)

IMPLEMENT_ASN1_FUNCTIONS(AC_HOLDER)
ASN1_SEQUENCE(AC_HOLDER) = {
  ASN1_IMP_OPT(AC_HOLDER, baseid, AC_IS, 0),
  ASN1_IMP_OPT(AC_HOLDER, name, GENERAL_NAMES, 1),
  ASN1_IMP_OPT(AC_HOLDER, digest, AC_DIGEST, 2)
} ASN1_SEQUENCE_END(AC_HOLDER)

IMPLEMENT_ASN1_FUNCTIONS(AC_VAL)
ASN1_SEQUENCE(AC_VAL) = {
  ASN1_SIMPLE(AC_VAL, notBefore, ASN1_GENERALIZEDTIME),
  ASN1_SIMPLE(AC_VAL, notAfter,  ASN1_GENERALIZEDTIME)
} ASN1_SEQUENCE_END(AC_VAL)

IMPLEMENT_ASN1_FUNCTIONS(AC_IETFATTR)
ASN1_SEQUENCE(AC_IETFATTR) = {
  ASN1_IMP_SEQUENCE_OF_OPT(AC_IETFATTR, names, GENERAL_NAME, 0),
  ASN1_SEQUENCE_OF(AC_IETFATTR, values, ASN1_ANY)
} ASN1_SEQUENCE_END(AC_IETFATTR)
  /*ASN1_IMP_OPT(AC_IETFATTR, names, GENERAL_NAMES, 0),*/
/*  ASN1_SEQUENCE_OF(AC_IETFATTR, values, AC_IETFATTRVAL) */

IMPLEMENT_ASN1_FUNCTIONS(AC_TARGET)
ASN1_SEQUENCE(AC_TARGET) = {
  ASN1_EXP_OPT(AC_TARGET, name, GENERAL_NAME, 0),
  ASN1_EXP_OPT(AC_TARGET, group, GENERAL_NAME, 1),
  ASN1_EXP_OPT(AC_TARGET, cert, AC_IS, 2)
} ASN1_SEQUENCE_END(AC_TARGET)

IMPLEMENT_ASN1_FUNCTIONS(AC_TARGETS)
ASN1_SEQUENCE(AC_TARGETS) = {
  ASN1_SEQUENCE_OF(AC_TARGETS, targets, AC_TARGET)
} ASN1_SEQUENCE_END(AC_TARGETS)

IMPLEMENT_ASN1_FUNCTIONS(AC_ATTR)
ASN1_SEQUENCE(AC_ATTR) = {
  ASN1_SIMPLE(AC_ATTR, type, ASN1_OBJECT),
  ASN1_SET_OF_OPT(AC_ATTR, ietfattr, AC_IETFATTR)
/*
  if (!i2t_ASN1_OBJECT(text,999,a->type))
    return 0;
  else if (!((strcmp(text, "idacagroup") == 0) || (strcmp(text,"idatcap") == 0)))
    return 0;
  

  ASN1_OBJECT * type;
  int get_type;
  STACK_OF(AC_IETFATTR) *ietfattr;
  STACK_OF(AC_FULL_ATTRIBUTES) *fullattributes;

*/
} ASN1_SEQUENCE_END(AC_ATTR)

IMPLEMENT_ASN1_FUNCTIONS(AC_INFO)
ASN1_SEQUENCE(AC_INFO) = {
  ASN1_SIMPLE(AC_INFO, version, ASN1_INTEGER),
  ASN1_SIMPLE(AC_INFO, holder, AC_HOLDER),
  ASN1_IMP_OPT(AC_INFO, form, AC_FORM, 0), /*V_ASN1_SEQUENCE*/
  ASN1_SIMPLE(AC_INFO, alg, X509_ALGOR),
  ASN1_SIMPLE(AC_INFO, serial, ASN1_INTEGER),
  ASN1_SIMPLE(AC_INFO, validity, AC_VAL),
  ASN1_SEQUENCE_OF(AC_INFO, attrib, AC_ATTR), /* todo */
  ASN1_IMP_OPT(AC_INFO, id, ASN1_BIT_STRING, V_ASN1_BIT_STRING),
  ASN1_SEQUENCE_OF_OPT(AC_INFO, exts, X509_EXTENSION)
} ASN1_SEQUENCE_END(AC_INFO)
//  ASN1_SIMPLE(AC_INFO, version, ASN1_INTEGER),
//  ASN1_SIMPLE(AC_INFO, holder, AC_HOLDER),
//  ASN1_IMP_OPT(AC_INFO, form, AC_FORM, 0),
//  ASN1_SIMPLE(AC_INFO, alg, X509_ALGOR),
//  ASN1_SIMPLE(AC_INFO, serial, ASN1_INTEGER),
//  ASN1_SIMPLE(AC_INFO, validity, AC_VAL),
//  ASN1_SEQUENCE_OF(AC_INFO, attrib, AC_ATTR),
//  ASN1_IMP_OPT(AC_INFO, id, ASN1_BIT_STRING, V_ASN1_BIT_STRING),
//  ASN1_SEQUENCE_OF_OPT(AC_INFO, exts, X509_EXTENSION)

IMPLEMENT_ASN1_FUNCTIONS(AC)
ASN1_SEQUENCE(AC) = {
  ASN1_SIMPLE(AC, acinfo, AC_INFO),
  ASN1_SIMPLE(AC, sig_alg, X509_ALGOR),
  ASN1_SIMPLE(AC, signature, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(AC)

IMPLEMENT_ASN1_FUNCTIONS(AC_SEQ)
ASN1_SEQUENCE(AC_SEQ) = {
  ASN1_SEQUENCE_OF(AC_SEQ, acs, AC)
} ASN1_SEQUENCE_END(AC_SEQ)

IMPLEMENT_ASN1_FUNCTIONS(AC_CERTS)
ASN1_SEQUENCE(AC_CERTS) = {
  ASN1_SEQUENCE_OF(AC_CERTS, stackcert, X509)
} ASN1_SEQUENCE_END(AC_CERTS)

IMPLEMENT_ASN1_FUNCTIONS(AC_ATTRIBUTE)
ASN1_SEQUENCE(AC_ATTRIBUTE) = {
  ASN1_SIMPLE(AC_ATTRIBUTE, name,      ASN1_OCTET_STRING),
  ASN1_SIMPLE(AC_ATTRIBUTE, value,     ASN1_OCTET_STRING),
  ASN1_SIMPLE(AC_ATTRIBUTE, qualifier, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(AC_ATTRIBUTE)

IMPLEMENT_ASN1_FUNCTIONS(AC_ATT_HOLDER)
ASN1_SEQUENCE(AC_ATT_HOLDER) = {
  ASN1_SIMPLE(AC_ATT_HOLDER, grantor, GENERAL_NAMES),
  ASN1_SEQUENCE_OF(AC_ATT_HOLDER, attributes, AC_ATTRIBUTE)
} ASN1_SEQUENCE_END(AC_ATT_HOLDER)

IMPLEMENT_ASN1_FUNCTIONS(AC_FULL_ATTRIBUTES)
ASN1_SEQUENCE(AC_FULL_ATTRIBUTES) = {
  ASN1_SEQUENCE_OF(AC_FULL_ATTRIBUTES, providers, AC_ATT_HOLDER)
} ASN1_SEQUENCE_END(AC_FULL_ATTRIBUTES)

/*
int i2d_AC_ATTR(AC_ATTR *a, unsigned char **pp)
{
  char text[1000];

  M_ASN1_I2D_vars(a);

  if (!i2t_ASN1_OBJECT(text,999,a->type))
    return 0;
  else if (!((strcmp(text, "idacagroup") == 0) || (strcmp(text,"idatcap") == 0)))
    return 0;
  
  M_ASN1_I2D_len(a->type, i2d_ASN1_OBJECT);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SET_type(AC_IETFATTR, a->ietfattr, i2d_AC_IETFATTR);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->type, i2d_ASN1_OBJECT);
  M_ASN1_I2D_put_SET_type(AC_IETFATTR,a->ietfattr, i2d_AC_IETFATTR);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SET_type(AC_IETFATTR, a->ietfattr, (int (*)(void*, unsigned char**))i2d_AC_IETFATTR);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->type, i2d_ASN1_OBJECT);
  M_ASN1_I2D_put_SET_type(AC_IETFATTR,a->ietfattr, (int (*)(void*, unsigned char**))i2d_AC_IETFATTR);
#else
  M_ASN1_I2D_len_SET_type(AC_IETFATTR, a->ietfattr, (int (*)())i2d_AC_IETFATTR);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->type, i2d_ASN1_OBJECT);
  M_ASN1_I2D_put_SET_type(AC_IETFATTR,a->ietfattr, (int (*)())i2d_AC_IETFATTR);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_ATTR *d2i_AC_ATTR(AC_ATTR **a, SSLCONST unsigned char **pp, long length)
{
  char text[1000];

  M_ASN1_D2I_vars(a, AC_ATTR *, AC_ATTR_new);
  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->type, d2i_ASN1_OBJECT);

  if (!i2t_ASN1_OBJECT(text,999, ret->type)) {
    c.error = ASN1_R_NOT_ENOUGH_DATA;
    goto err;
  }

  if (strcmp(text, "idatcap") == 0)
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
    M_ASN1_D2I_get_set_type(AC_IETFATTR, ret->ietfattr, d2i_AC_IETFATTR, AC_IETFATTR_free);
#else
    M_ASN1_D2I_get_set_type(AC_IETFATTR, ret->ietfattr, (AC_IETFATTR* (*)())d2i_AC_IETFATTR, AC_IETFATTR_free);
#endif
  M_ASN1_D2I_Finish(a, AC_ATTR_free, ASN1_F_D2I_AC_ATTR);
}

AC_ATTR *AC_ATTR_new()
{
  AC_ATTR *ret = NULL;
  ASN1_CTX c;
  M_ASN1_New_Malloc(ret, AC_ATTR);
  M_ASN1_New(ret->type,  ASN1_OBJECT_new);
  M_ASN1_New(ret->ietfattr, sk_AC_IETFATTR_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_ATTR_New);
}

void AC_ATTR_free(AC_ATTR *a)
{
  if (!a)
    return;

  ASN1_OBJECT_free(a->type);
  sk_AC_IETFATTR_pop_free(a->ietfattr, AC_IETFATTR_free);
  OPENSSL_free(a);
}

int i2d_AC_IETFATTR(AC_IETFATTR *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len_IMP_opt(a->names, i2d_GENERAL_NAMES);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC_IETFATTRVAL, a->values, i2d_AC_IETFATTRVAL);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC_IETFATTRVAL, a->values, (int (*)(void*, unsigned char**))i2d_AC_IETFATTRVAL);
#else
  M_ASN1_I2D_len_SEQUENCE(a->values,  (int (*)())i2d_AC_IETFATTRVAL);
#endif
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put_IMP_opt(a->names, i2d_GENERAL_NAMES, 0);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_put_SEQUENCE_type(AC_IETFATTRVAL, a->values, i2d_AC_IETFATTRVAL);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_put_SEQUENCE_type(AC_IETFATTRVAL, a->values, (int (*)(void*, unsigned char**))i2d_AC_IETFATTRVAL);
#else
  M_ASN1_I2D_put_SEQUENCE(a->values,  (int (*)())i2d_AC_IETFATTRVAL);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_IETFATTR *d2i_AC_IETFATTR(AC_IETFATTR **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_IETFATTR *, AC_IETFATTR_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get_IMP_opt(ret->names, d2i_GENERAL_NAMES, 0, V_ASN1_SEQUENCE);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC_IETFATTRVAL, ret->values, d2i_AC_IETFATTRVAL, AC_IETFATTRVAL_free);
#else
  M_ASN1_D2I_get_seq_type(AC_IETFATTRVAL, ret->values, (AC_IETFATTRVAL* (*)())d2i_AC_IETFATTRVAL, AC_IETFATTRVAL_free);
#endif
  M_ASN1_D2I_Finish(a, AC_IETFATTR_free, ASN1_F_D2I_AC_IETFATTR);
}

AC_IETFATTR *AC_IETFATTR_new()
{
  AC_IETFATTR *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret,  AC_IETFATTR);
  M_ASN1_New(ret->values, sk_AC_IETFATTRVAL_new_null);
  M_ASN1_New(ret->names,  sk_GENERAL_NAME_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_IETFATTR_New);
}

void AC_IETFATTR_free (AC_IETFATTR *a)
{
  if (a==NULL) return;

  sk_GENERAL_NAME_pop_free(a->names, GENERAL_NAME_free);
  sk_AC_IETFATTRVAL_pop_free(a->values, AC_IETFATTRVAL_free);
  OPENSSL_free(a);
}

    
int i2d_AC_IETFATTRVAL(AC_IETFATTRVAL *a, unsigned char **pp)
{
  if (a->type == V_ASN1_OCTET_STRING || a->type == V_ASN1_OBJECT ||
      a->type == V_ASN1_UTF8STRING)
    return (i2d_ASN1_bytes((ASN1_STRING *)a, pp, a->type, V_ASN1_UNIVERSAL));

  ASN1err(ASN1_F_I2D_AC_IETFATTRVAL,ASN1_R_WRONG_TYPE);
  return -1;
}

AC_IETFATTRVAL *d2i_AC_IETFATTRVAL(AC_IETFATTRVAL **a, SSLCONST unsigned char **pp, long length)
{
  unsigned char tag;
  tag = **pp & ~V_ASN1_CONSTRUCTED;
  if (tag == (V_ASN1_OCTET_STRING|V_ASN1_UNIVERSAL))
    return d2i_ASN1_OCTET_STRING(a, pp, length);
  if (tag == (V_ASN1_OBJECT|V_ASN1_UNIVERSAL))
    return (AC_IETFATTRVAL *)d2i_ASN1_OBJECT((ASN1_OBJECT **)a, pp, length);
  if (tag == (V_ASN1_UTF8STRING|V_ASN1_UNIVERSAL))
    return d2i_ASN1_UTF8STRING(a, pp, length);
  ASN1err(ASN1_F_D2I_AC_IETFATTRVAL, ASN1_R_WRONG_TYPE);
  return (NULL);
}

AC_IETFATTRVAL *AC_IETFATTRVAL_new()
{
  return ASN1_STRING_type_new(V_ASN1_UTF8STRING);
}

void AC_IETFATTRVAL_free(AC_IETFATTRVAL *a)
{
  ASN1_STRING_free(a);
}

int i2d_AC_DIGEST(AC_DIGEST *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len(a->type,          i2d_ASN1_ENUMERATED);
  M_ASN1_I2D_len(a->oid,           i2d_ASN1_OBJECT);
  M_ASN1_I2D_len(a->algor,         i2d_X509_ALGOR);
  M_ASN1_I2D_len(a->digest,        i2d_ASN1_BIT_STRING);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->type,         i2d_ASN1_ENUMERATED);
  M_ASN1_I2D_put(a->oid,          i2d_ASN1_OBJECT);
  M_ASN1_I2D_put(a->algor,        i2d_X509_ALGOR);
  M_ASN1_I2D_put(a->digest,       i2d_ASN1_BIT_STRING);
  M_ASN1_I2D_finish();
  return 0;
}

AC_DIGEST *d2i_AC_DIGEST(AC_DIGEST **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_DIGEST *, AC_DIGEST_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->type,        d2i_ASN1_ENUMERATED);
  M_ASN1_D2I_get(ret->oid,         d2i_ASN1_OBJECT);
  M_ASN1_D2I_get(ret->algor,       d2i_X509_ALGOR);
  M_ASN1_D2I_get(ret->digest,      d2i_ASN1_BIT_STRING);
  M_ASN1_D2I_Finish(a, AC_DIGEST_free, AC_F_D2I_AC_DIGEST);
}

AC_DIGEST *AC_DIGEST_new(void)
{
  AC_DIGEST *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_DIGEST);
  M_ASN1_New(ret->type, M_ASN1_ENUMERATED_new);
  ret->oid = NULL;
  ret->algor = NULL;
  M_ASN1_New(ret->algor,  X509_ALGOR_new);
  M_ASN1_New(ret->digest, M_ASN1_BIT_STRING_new);
  return(ret);
  M_ASN1_New_Error(AC_F_AC_DIGEST_New);
}

void AC_DIGEST_free(AC_DIGEST *a)
{
  if (a==NULL) return;

  ASN1_ENUMERATED_free(a->type);
  ASN1_OBJECT_free(a->oid);
  X509_ALGOR_free(a->algor);
  ASN1_BIT_STRING_free(a->digest);
  OPENSSL_free(a);
}

int i2d_AC_IS(AC_IS *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len(a->issuer,      i2d_GENERAL_NAMES);
  M_ASN1_I2D_len(a->serial,      i2d_ASN1_INTEGER);
  M_ASN1_I2D_len_IMP_opt(a->uid, i2d_ASN1_BIT_STRING);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->issuer,      i2d_GENERAL_NAMES);
  M_ASN1_I2D_put(a->serial,      i2d_ASN1_INTEGER);
  M_ASN1_I2D_put_IMP_opt(a->uid, i2d_ASN1_BIT_STRING, V_ASN1_BIT_STRING);
  M_ASN1_I2D_finish();
  return 0;
}

AC_IS *d2i_AC_IS(AC_IS **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_IS *, AC_IS_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->issuer,  d2i_GENERAL_NAMES);
  M_ASN1_D2I_get(ret->serial,  d2i_ASN1_INTEGER);
  M_ASN1_D2I_get_opt(ret->uid, d2i_ASN1_BIT_STRING, V_ASN1_BIT_STRING);
  M_ASN1_D2I_Finish(a, AC_IS_free, AC_F_D2I_AC_IS);
}

AC_IS *AC_IS_new(void)
{
  AC_IS *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_IS);
  M_ASN1_New(ret->issuer, GENERAL_NAMES_new);
  M_ASN1_New(ret->serial, M_ASN1_INTEGER_new);
  ret->uid = NULL;
  return(ret);
  M_ASN1_New_Error(AC_F_AC_IS_New);
}

void AC_IS_free(AC_IS *a)
{
  if (a==NULL) return;

  GENERAL_NAMES_free(a->issuer);
  M_ASN1_INTEGER_free(a->serial);
  M_ASN1_BIT_STRING_free(a->uid);
  OPENSSL_free(a);
}

int i2d_AC_FORM(AC_FORM *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);

  M_ASN1_I2D_len(a->names,  i2d_GENERAL_NAMES);
  M_ASN1_I2D_len_IMP_opt(a->is,     i2d_AC_IS);
  M_ASN1_I2D_len_IMP_opt(a->digest, i2d_AC_DIGEST);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->names,  i2d_GENERAL_NAMES);
  M_ASN1_I2D_put_IMP_opt(a->is,     i2d_AC_IS, 0);
  M_ASN1_I2D_put_IMP_opt(a->digest, i2d_AC_DIGEST, 1);
  M_ASN1_I2D_finish();
  return 0;
}

AC_FORM *d2i_AC_FORM(AC_FORM **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_FORM *, AC_FORM_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->names,  d2i_GENERAL_NAMES);
  M_ASN1_D2I_get_IMP_opt(ret->is,     d2i_AC_IS, 0, V_ASN1_SEQUENCE);
  M_ASN1_D2I_get_IMP_opt(ret->digest, d2i_AC_DIGEST, 1, V_ASN1_SEQUENCE);
  M_ASN1_D2I_Finish(a, AC_FORM_free, ASN1_F_D2I_AC_FORM);
}

AC_FORM *AC_FORM_new(void)
{
  AC_FORM *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_FORM);
  ret->names = GENERAL_NAMES_new();
  ret->is = NULL;
  ret->digest = NULL;
  return(ret);
  M_ASN1_New_Error(AC_F_AC_FORM_New);
}

void AC_FORM_free(AC_FORM *a)
{
  if (a==NULL) return;

  GENERAL_NAMES_free(a->names);
  AC_IS_free(a->is);
  AC_DIGEST_free(a->digest);
  OPENSSL_free(a);

}

int i2d_AC_ACI(AC_ACI *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len_IMP_opt(a->form, i2d_AC_FORM);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put_IMP_opt(a->form, i2d_AC_FORM, 0);
  M_ASN1_I2D_finish();
  return 0;
}

AC_ACI *d2i_AC_ACI(AC_ACI **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_ACI *, AC_ACI_new);
  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get_IMP_opt(ret->form, d2i_AC_FORM, 0, V_ASN1_SEQUENCE);
  M_ASN1_D2I_Finish(a, AC_ACI_free, ASN1_F_D2I_AC_ACI);
}

AC_ACI *AC_ACI_new(void)
{
  AC_ACI *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_ACI);
  ret->form = AC_FORM_new();
  ret->names = NULL;
  return (ret);
  M_ASN1_New_Error(ASN1_F_AC_ACI_New);
}

void AC_ACI_free(AC_ACI *a)
{
  if (a==NULL) return;
  GENERAL_NAMES_free(a->names);
  AC_FORM_free(a->form);
  OPENSSL_free(a);
}

int i2d_AC_HOLDER(AC_HOLDER *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);

  M_ASN1_I2D_len_IMP_opt(a->baseid, i2d_AC_IS);
  M_ASN1_I2D_len_IMP_opt(a->name, i2d_GENERAL_NAMES);
  M_ASN1_I2D_len_IMP_opt(a->digest, i2d_AC_DIGEST);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put_IMP_opt(a->baseid, i2d_AC_IS, 0);	       
  M_ASN1_I2D_put_IMP_opt(a->name, i2d_GENERAL_NAMES, 1);
  M_ASN1_I2D_put_IMP_opt(a->digest, i2d_AC_DIGEST, 2);
  M_ASN1_I2D_finish();
  return 0;
}

AC_HOLDER *d2i_AC_HOLDER(AC_HOLDER **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_HOLDER *, AC_HOLDER_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get_IMP_opt(ret->baseid, d2i_AC_IS, 0, V_ASN1_SEQUENCE);
  M_ASN1_D2I_get_IMP_opt(ret->name, d2i_GENERAL_NAMES, 1, V_ASN1_SEQUENCE);
  M_ASN1_D2I_get_IMP_opt(ret->digest, d2i_AC_DIGEST, 2, V_ASN1_SEQUENCE);
  M_ASN1_D2I_Finish(a, AC_HOLDER_free, ASN1_F_D2I_AC_HOLDER);
}

AC_HOLDER *AC_HOLDER_new(void)
{
  AC_HOLDER *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_HOLDER);
  M_ASN1_New(ret->baseid, AC_IS_new);
  ret->name = NULL;
  ret->digest = NULL;
  return(ret);
  M_ASN1_New_Error(ASN1_F_AC_HOLDER_New);
}

void AC_HOLDER_free(AC_HOLDER *a)
{
  if (!a) return;

  AC_IS_free(a->baseid);
  GENERAL_NAMES_free(a->name);
  AC_DIGEST_free(a->digest);
  OPENSSL_free(a);
}

AC_VAL *AC_VAL_new(void)
{
  AC_VAL *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_VAL);

  ret->notBefore = NULL;
  ret->notAfter = NULL;
  
  return(ret);
  M_ASN1_New_Error(ASN1_F_AC_VAL_New);
}

int i2d_AC_VAL(AC_VAL *a, unsigned char **pp) 
{
  M_ASN1_I2D_vars(a);

  M_ASN1_I2D_len(a->notBefore, i2d_ASN1_GENERALIZEDTIME);
  M_ASN1_I2D_len(a->notAfter,  i2d_ASN1_GENERALIZEDTIME);

  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->notBefore, i2d_ASN1_GENERALIZEDTIME);
  M_ASN1_I2D_put(a->notAfter,  i2d_ASN1_GENERALIZEDTIME);

  M_ASN1_I2D_finish();
  return 0;
}

AC_VAL *d2i_AC_VAL(AC_VAL **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_VAL *, AC_VAL_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();

  M_ASN1_D2I_get(ret->notBefore, d2i_ASN1_GENERALIZEDTIME);
  M_ASN1_D2I_get(ret->notAfter,  d2i_ASN1_GENERALIZEDTIME);

  M_ASN1_D2I_Finish(a, AC_VAL_free, AC_F_D2I_AC);
}

void AC_VAL_free(AC_VAL *a)
{

  if (a==NULL) return;

  M_ASN1_GENERALIZEDTIME_free(a->notBefore);
  M_ASN1_GENERALIZEDTIME_free(a->notAfter);

  OPENSSL_free(a);
}

int i2d_AC_INFO(AC_INFO *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);

  M_ASN1_I2D_len(a->version,  i2d_ASN1_INTEGER);
  M_ASN1_I2D_len(a->holder,   i2d_AC_HOLDER);
  M_ASN1_I2D_len_IMP_opt(a->form, i2d_AC_FORM);
  M_ASN1_I2D_len(a->alg,      i2d_X509_ALGOR);
  M_ASN1_I2D_len(a->serial,   i2d_ASN1_INTEGER);
  M_ASN1_I2D_len(a->validity, i2d_AC_VAL);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATTR, a->attrib, i2d_AC_ATTR);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATTR, a->attrib, (int (*)(void*, unsigned char**))i2d_AC_ATTR);
#else
  M_ASN1_I2D_len_SEQUENCE    (a->attrib, (int(*)())i2d_AC_ATTR);
#endif
  M_ASN1_I2D_len_IMP_opt     (a->id, i2d_ASN1_BIT_STRING);
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_opt_type(X509_EXTENSION, a->exts, i2d_X509_EXTENSION);
#else
  M_ASN1_I2D_len_SEQUENCE_opt(a->exts,   (int(*)())i2d_X509_EXTENSION);
#endif
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->version,  i2d_ASN1_INTEGER);
  M_ASN1_I2D_put(a->holder,   i2d_AC_HOLDER);
  M_ASN1_I2D_put_IMP_opt(a->form,   i2d_AC_FORM, 0);
  M_ASN1_I2D_put(a->alg,      i2d_X509_ALGOR);
  M_ASN1_I2D_put(a->serial,   i2d_ASN1_INTEGER);
  M_ASN1_I2D_put(a->validity, i2d_AC_VAL);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATTR, a->attrib, i2d_AC_ATTR);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATTR, a->attrib, (int (*)(void*, unsigned char**))i2d_AC_ATTR);
#else
  M_ASN1_I2D_put_SEQUENCE(a->attrib, (int(*)())i2d_AC_ATTR);
#endif
  M_ASN1_I2D_put_IMP_opt(a->id, i2d_ASN1_BIT_STRING, V_ASN1_BIT_STRING);
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_put_SEQUENCE_opt_type(X509_EXTENSION, a->exts, i2d_X509_EXTENSION);
#else
  M_ASN1_I2D_put_SEQUENCE_opt(a->exts, (int(*)())i2d_X509_EXTENSION);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_INFO *d2i_AC_INFO(AC_INFO **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_INFO *, AC_INFO_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->version,    d2i_ASN1_INTEGER);
  M_ASN1_D2I_get(ret->holder,     d2i_AC_HOLDER);
  M_ASN1_D2I_get_IMP_opt(ret->form,     d2i_AC_FORM, 0, V_ASN1_SEQUENCE);
  M_ASN1_D2I_get(ret->alg,        d2i_X509_ALGOR);
  M_ASN1_D2I_get(ret->serial,     d2i_ASN1_INTEGER);
  M_ASN1_D2I_get(ret->validity, d2i_AC_VAL);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC_ATTR, ret->attrib, d2i_AC_ATTR, AC_ATTR_free);
#else
  M_ASN1_D2I_get_seq_type(AC_ATTR, ret->attrib, (AC_ATTR* (*)())d2i_AC_ATTR, AC_ATTR_free);
#endif
  M_ASN1_D2I_get_opt(ret->id,     d2i_ASN1_BIT_STRING, V_ASN1_BIT_STRING);
  M_ASN1_D2I_get_seq_opt_type(X509_EXTENSION, ret->exts, d2i_X509_EXTENSION, X509_EXTENSION_free);
  M_ASN1_D2I_Finish(a, AC_INFO_free, AC_F_D2I_AC);
}

AC_INFO *AC_INFO_new(void)
{
  AC_INFO *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_INFO);
  M_ASN1_New(ret->version,  ASN1_INTEGER_new);
  M_ASN1_New(ret->holder,   AC_HOLDER_new);
  M_ASN1_New(ret->form,     AC_FORM_new);
  M_ASN1_New(ret->alg,      X509_ALGOR_new);
  M_ASN1_New(ret->serial,   ASN1_INTEGER_new);
  M_ASN1_New(ret->validity, AC_VAL_new);
  M_ASN1_New(ret->attrib,   sk_AC_ATTR_new_null);
  ret->id = NULL;
  M_ASN1_New(ret->exts,     sk_X509_EXTENSION_new_null);
*   ret->exts=NULL; *
  return(ret);
  M_ASN1_New_Error(AC_F_AC_INFO_NEW);
}

void AC_INFO_free(AC_INFO *a)
{
  if (a==NULL) return;
  ASN1_INTEGER_free(a->version);
  AC_HOLDER_free(a->holder);
  AC_FORM_free(a->form);
  X509_ALGOR_free(a->alg);
  ASN1_INTEGER_free(a->serial);
  AC_VAL_free(a->validity);
  sk_AC_ATTR_pop_free(a->attrib, AC_ATTR_free);
  ASN1_BIT_STRING_free(a->id);
  sk_X509_EXTENSION_pop_free(a->exts, X509_EXTENSION_free);
  OPENSSL_free(a);
}

int i2d_AC(AC *a, unsigned char **pp) 
{
  M_ASN1_I2D_vars(a);

  M_ASN1_I2D_len(a->acinfo,    i2d_AC_INFO);
  M_ASN1_I2D_len(a->sig_alg,   i2d_X509_ALGOR);
  M_ASN1_I2D_len(a->signature, i2d_ASN1_BIT_STRING);

  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->acinfo,    i2d_AC_INFO);
  M_ASN1_I2D_put(a->sig_alg,   i2d_X509_ALGOR);
  M_ASN1_I2D_put(a->signature, i2d_ASN1_BIT_STRING);

  M_ASN1_I2D_finish();
  return 0;
}

AC *d2i_AC(AC **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC *, AC_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->acinfo,    d2i_AC_INFO);
  M_ASN1_D2I_get(ret->sig_alg,   d2i_X509_ALGOR);
  M_ASN1_D2I_get(ret->signature, d2i_ASN1_BIT_STRING);
  M_ASN1_D2I_Finish(a, AC_free, AC_F_D2I_AC);
}

AC *AC_new(void)
{
  AC *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC);
  M_ASN1_New(ret->acinfo,    AC_INFO_new);
  M_ASN1_New(ret->sig_alg,   X509_ALGOR_new);
  M_ASN1_New(ret->signature, M_ASN1_BIT_STRING_new);
  return(ret);
  M_ASN1_New_Error(AC_F_AC_New);
}

void AC_free(AC *a)
{
  if (a==NULL) return;

  AC_INFO_free(a->acinfo);
  X509_ALGOR_free(a->sig_alg);
  M_ASN1_BIT_STRING_free(a->signature);
  OPENSSL_free(a);
}

////////////////////////////////////////////
int i2d_AC_SEQ(AC_SEQ *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC, a->acs, i2d_AC);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC, a->acs, i2d_AC);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC, a->acs, (int (*)(void*, unsigned char**))i2d_AC);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC, a->acs, (int (*)(void*, unsigned char**))i2d_AC);
#else
  M_ASN1_I2D_len_SEQUENCE(a->acs, (int (*)())i2d_AC);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE(a->acs, (int (*)())i2d_AC);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_SEQ *d2i_AC_SEQ(AC_SEQ **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_SEQ *, AC_SEQ_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC, ret->acs, d2i_AC, AC_free);
#else
  M_ASN1_D2I_get_seq_type(AC, ret->acs, (AC* (*)())d2i_AC, AC_free);
#endif
  M_ASN1_D2I_Finish(a, AC_SEQ_free, ASN1_F_D2I_AC_SEQ);
}

AC_SEQ *AC_SEQ_new()
{
  AC_SEQ *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_SEQ);
  M_ASN1_New(ret->acs, sk_AC_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_AC_SEQ_new);
}

void AC_SEQ_free(AC_SEQ *a)
{
  if (a==NULL) return;

  sk_AC_pop_free(a->acs, AC_free);
  OPENSSL_free(a);
}

int i2d_AC_TARGETS(AC_TARGETS *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC_TARGET, a->targets, i2d_AC_TARGET);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC_TARGET, a->targets, i2d_AC_TARGET);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC_TARGET, a->targets, (int (*)(void*, unsigned char**))i2d_AC_TARGET);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC_TARGET, a->targets, (int (*)(void*, unsigned char**))i2d_AC_TARGET);
#else
  M_ASN1_I2D_len_SEQUENCE(a->targets, (int (*)())i2d_AC_TARGET);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE(a->targets, (int (*)())i2d_AC_TARGET);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_TARGETS *d2i_AC_TARGETS(AC_TARGETS **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_TARGETS *, AC_TARGETS_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC_TARGET, ret->targets, d2i_AC_TARGET, AC_TARGET_free);
#else
  M_ASN1_D2I_get_seq_type(AC_TARGET, ret->targets, (AC_TARGET* (*)())d2i_AC_TARGET, AC_TARGET_free);
#endif
  M_ASN1_D2I_Finish(a, AC_TARGETS_free, ASN1_F_D2I_AC_TARGETS);
}
AC_TARGETS *AC_TARGETS_new()
{
  AC_TARGETS *ret=NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_TARGETS);
  M_ASN1_New(ret->targets, sk_AC_TARGET_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_AC_TARGETS_New);
}

void AC_TARGETS_free(AC_TARGETS *a)
{
  if (a==NULL) return;

  sk_AC_TARGET_pop_free(a->targets, AC_TARGET_free);
  OPENSSL_free(a);
}

int i2d_AC_TARGET(AC_TARGET *a, unsigned char **pp)
{
  int v1=0, v2=0, v3=0;

  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len_EXP_opt(a->name, i2d_GENERAL_NAME, 0, v1);
  M_ASN1_I2D_len_EXP_opt(a->group, i2d_GENERAL_NAME, 1, v2);
  M_ASN1_I2D_len_EXP_opt(a->cert, i2d_AC_IS, 2, v3);
  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put_EXP_opt(a->name, i2d_GENERAL_NAME, 0, v1);
  M_ASN1_I2D_put_EXP_opt(a->group, i2d_GENERAL_NAME, 1, v2);
  M_ASN1_I2D_put_EXP_opt(a->cert, i2d_AC_IS, 2, v3);
  M_ASN1_I2D_finish();
  return 0;
}

AC_TARGET *d2i_AC_TARGET(AC_TARGET **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_TARGET *, AC_TARGET_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get_EXP_opt(ret->name, d2i_GENERAL_NAME, 0);
  M_ASN1_D2I_get_EXP_opt(ret->group, d2i_GENERAL_NAME, 1);
  M_ASN1_D2I_get_EXP_opt(ret->cert, d2i_AC_IS, 2);
  M_ASN1_D2I_Finish(a, AC_TARGET_free, ASN1_F_D2I_AC_TARGET);
}

AC_TARGET *AC_TARGET_new(void)
{
  AC_TARGET *ret=NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_TARGET);
  ret->name = ret->group = NULL;
  ret->cert = NULL;
  return ret;
  M_ASN1_New_Error(AC_F_AC_TARGET_New);
}

void AC_TARGET_free(AC_TARGET *a)
{
  if (a==NULL) return;
  GENERAL_NAME_free(a->name);
  GENERAL_NAME_free(a->group);
  AC_IS_free(a->cert);
  OPENSSL_free(a);
}

int i2d_AC_CERTS(AC_CERTS *a, unsigned char **pp)
{
  //int v1=0, v2=0, v3=0;

  M_ASN1_I2D_vars(a);
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(X509, a->stackcert, i2d_X509);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(X509, a->stackcert, i2d_X509);
#else
  M_ASN1_I2D_len_SEQUENCE(a->stackcert, (int (*)())i2d_X509);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE(a->stackcert, (int (*)())i2d_X509);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_CERTS *d2i_AC_CERTS(AC_CERTS **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_CERTS *, AC_CERTS_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get_seq_type(X509, ret->stackcert, d2i_X509, X509_free);
  M_ASN1_D2I_Finish(a, AC_CERTS_free, ASN1_F_D2I_AC_CERTS);
}

AC_CERTS *AC_CERTS_new()
{
  AC_CERTS *ret=NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_CERTS);
  M_ASN1_New(ret->stackcert, sk_X509_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_X509_New);
}

void AC_CERTS_free(AC_CERTS *a)
{
  if (a==NULL) return;

  sk_X509_pop_free(a->stackcert, X509_free);
  OPENSSL_free(a);
}

int i2d_AC_ATTRIBUTE(AC_ATTRIBUTE *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len(a->name,      i2d_ASN1_OCTET_STRING);
  M_ASN1_I2D_len(a->value,     i2d_ASN1_OCTET_STRING);
  M_ASN1_I2D_len(a->qualifier, i2d_ASN1_OCTET_STRING);

  M_ASN1_I2D_seq_total();

  M_ASN1_I2D_put(a->name,      i2d_ASN1_OCTET_STRING);
  M_ASN1_I2D_put(a->value,     i2d_ASN1_OCTET_STRING);
  M_ASN1_I2D_put(a->qualifier, i2d_ASN1_OCTET_STRING);

  M_ASN1_I2D_finish();
  return 0;
}

AC_ATTRIBUTE *d2i_AC_ATTRIBUTE(AC_ATTRIBUTE **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_ATTRIBUTE *, AC_ATTRIBUTE_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->name,      d2i_ASN1_OCTET_STRING);
  M_ASN1_D2I_get(ret->value,     d2i_ASN1_OCTET_STRING);
  M_ASN1_D2I_get(ret->qualifier, d2i_ASN1_OCTET_STRING);

  M_ASN1_D2I_Finish(a, AC_ATTRIBUTE_free, AC_F_D2I_AC_ATTRIBUTE);
}

AC_ATTRIBUTE *AC_ATTRIBUTE_new()
{
  AC_ATTRIBUTE *ret = NULL;
  ASN1_CTX c;
  M_ASN1_New_Malloc(ret, AC_ATTRIBUTE);
  M_ASN1_New(ret->name,      ASN1_OCTET_STRING_new);
  M_ASN1_New(ret->value,     ASN1_OCTET_STRING_new);
  M_ASN1_New(ret->qualifier, ASN1_OCTET_STRING_new);

  return ret;
  M_ASN1_New_Error(AC_F_ATTRIBUTE_New);
}

void AC_ATTRIBUTE_free(AC_ATTRIBUTE *a)
{
  if (a == NULL) return;

  ASN1_OCTET_STRING_free(a->name);
  ASN1_OCTET_STRING_free(a->value);
  ASN1_OCTET_STRING_free(a->qualifier);

  OPENSSL_free(a);
}

int i2d_AC_ATT_HOLDER(AC_ATT_HOLDER *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
  M_ASN1_I2D_len(a->grantor,      i2d_GENERAL_NAMES);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATTRIBUTE, a->attributes, i2d_AC_ATTRIBUTE);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->grantor, i2d_GENERAL_NAMES);
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATTRIBUTE, a->attributes, i2d_AC_ATTRIBUTE);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATTRIBUTE, a->attributes, (int (*)(void*, unsigned char**))i2d_AC_ATTRIBUTE);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->grantor, i2d_GENERAL_NAMES);
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATTRIBUTE, a->attributes, (int (*)(void*, unsigned char**))i2d_AC_ATTRIBUTE);
#else
  M_ASN1_I2D_len_SEQUENCE(a->attributes, (int(*)())i2d_AC_ATTRIBUTE);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put(a->grantor, i2d_GENERAL_NAMES);
  M_ASN1_I2D_put_SEQUENCE(a->attributes, (int(*)())i2d_AC_ATTRIBUTE);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_ATT_HOLDER *d2i_AC_ATT_HOLDER(AC_ATT_HOLDER **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_ATT_HOLDER *, AC_ATT_HOLDER_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
  M_ASN1_D2I_get(ret->grantor, d2i_GENERAL_NAMES);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC_ATTRIBUTE, ret->attributes, d2i_AC_ATTRIBUTE, AC_ATTRIBUTE_free);
#else
  M_ASN1_D2I_get_seq_type(AC_ATTRIBUTE, ret->attributes, (AC_ATTRIBUTE* (*)())d2i_AC_ATTRIBUTE, AC_ATTRIBUTE_free);
#endif
  M_ASN1_D2I_Finish(a, AC_ATT_HOLDER_free, ASN1_F_D2I_AC_ATT_HOLDER);
}

AC_ATT_HOLDER *AC_ATT_HOLDER_new()
{
  AC_ATT_HOLDER *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_ATT_HOLDER);
  M_ASN1_New(ret->grantor, sk_GENERAL_NAME_new_null);
  M_ASN1_New(ret->attributes, sk_AC_ATTRIBUTE_new_null);
  return ret;

  M_ASN1_New_Error(AC_F_AC_ATT_HOLDER_New);
}

void AC_ATT_HOLDER_free(AC_ATT_HOLDER *a)
{
  if (a == NULL) return;

  sk_GENERAL_NAME_pop_free(a->grantor, GENERAL_NAME_free);
  sk_AC_ATTRIBUTE_pop_free(a->attributes, AC_ATTRIBUTE_free);
  OPENSSL_free(a);
}

int i2d_AC_FULL_ATTRIBUTES(AC_FULL_ATTRIBUTES *a, unsigned char **pp)
{
  M_ASN1_I2D_vars(a);
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATT_HOLDER, a->providers, i2d_AC_ATT_HOLDER);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATT_HOLDER, a->providers, i2d_AC_ATT_HOLDER);
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
  M_ASN1_I2D_len_SEQUENCE_type(AC_ATT_HOLDER, a->providers, (int (*)(void*, unsigned char**))i2d_AC_ATT_HOLDER);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE_type(AC_ATT_HOLDER, a->providers, (int (*)(void*, unsigned char**))i2d_AC_ATT_HOLDER);
#else
  M_ASN1_I2D_len_SEQUENCE(a->providers, (int (*)())i2d_AC_ATT_HOLDER);
  M_ASN1_I2D_seq_total();
  M_ASN1_I2D_put_SEQUENCE(a->providers, (int (*)())i2d_AC_ATT_HOLDER);
#endif
  M_ASN1_I2D_finish();
  return 0;
}

AC_FULL_ATTRIBUTES *d2i_AC_FULL_ATTRIBUTES(AC_FULL_ATTRIBUTES **a, SSLCONST unsigned char **pp, long length)
{
  M_ASN1_D2I_vars(a, AC_FULL_ATTRIBUTES *, AC_FULL_ATTRIBUTES_new);

  M_ASN1_D2I_Init();
  M_ASN1_D2I_start_sequence();
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
  M_ASN1_D2I_get_seq_type(AC_ATT_HOLDER, ret->providers, d2i_AC_ATT_HOLDER, AC_ATT_HOLDER_free);
#else
  M_ASN1_D2I_get_seq_type(AC_ATT_HOLDER, ret->providers, (AC_ATT_HOLDER* (*)())d2i_AC_ATT_HOLDER, AC_ATT_HOLDER_free);
#endif
  M_ASN1_D2I_Finish(a, AC_FULL_ATTRIBUTES_free, ASN1_F_D2I_AC_FULL_ATTRIBUTES);
}

AC_FULL_ATTRIBUTES *AC_FULL_ATTRIBUTES_new()
{
  AC_FULL_ATTRIBUTES *ret = NULL;
  ASN1_CTX c;

  M_ASN1_New_Malloc(ret, AC_FULL_ATTRIBUTES);
  M_ASN1_New(ret->providers, sk_AC_ATT_HOLDER_new_null);
  return ret;
  M_ASN1_New_Error(AC_F_AC_FULL_ATTRIBUTES_New);
}

void AC_FULL_ATTRIBUTES_free(AC_FULL_ATTRIBUTES *a)
{
  if (a == NULL) return;

  sk_AC_ATT_HOLDER_pop_free(a->providers, AC_ATT_HOLDER_free);
  OPENSSL_free(a);
}
*/

static char *norep()
{
  static char buffer[] = "";

/*   buffer=malloc(1); */
/*   if (buffer) */
/*     *buffer='\0'; */
  return buffer;
}

/*
char *acseq_i2s(struct v3_ext_method*, void* data)
{
  AC **aclist = NULL;
 
  AC *item = NULL;
  AC_SEQ *seq = (AC_SEQ*)data;
  if(!seq) return NULL;

  int num = sk_AC_num(seq->acs);
  if(num > 0) aclist = (AC **)OPENSSL_malloc(num * sizeof(AC*));
  for (int i =0; i < num; i++) {
    item = sk_AC_value(seq->acs, i);
    // AC itself is not duplicated
    aclist[i] = item;
  }
 
  if(aclist == NULL) return NULL;
  return (char *)aclist;
  // return norep();
}
*/

char *acseq_i2s(struct v3_ext_method*, void* data)
{
  AC_SEQ* acseq = NULL;
  acseq = (AC_SEQ *)data;
  if(!acseq) return NULL;
  std::string encoded_acseq;

  AC *item = NULL;
  int num = sk_AC_num(acseq->acs);
  for (int i =0; i < num; i++) {
    item = sk_AC_value(acseq->acs, i);
    unsigned int len = i2d_AC(item, NULL);
    unsigned char *tmp = (unsigned char *)OPENSSL_malloc(len);
    std::string ac_str;
    if(tmp) {
      unsigned char *ttmp = tmp;
      i2d_AC(item, &ttmp);
      //ac_str = std::string((char *)tmp, len);
      ac_str.append((const char*)tmp, len);
      free(tmp);
    }

    // encode the AC string
    int size;
    char* enc = NULL;
    std::string encodedac;
    enc = Arc::VOMSEncode((char*)(ac_str.c_str()), ac_str.length(), &size);
    if (enc != NULL) {
      encodedac.append(enc, size);
      free(enc);
      enc = NULL;
    }
    encoded_acseq.append(VOMS_AC_HEADER).append("\n");
    encoded_acseq.append(encodedac).append("\n");
    encoded_acseq.append(VOMS_AC_TRAILER).append("\n");
  }
  
  char* ret = NULL;
  int len = encoded_acseq.length();
  if(len) {
    ret = (char*)OPENSSL_malloc(len + 1);
    memset(ret, 0, len + 1);
    memcpy(ret, encoded_acseq.c_str(), len);
/*
    ret = (char*)OPENSSL_malloc(len);
    strncpy(ret, encoded_acseq.c_str(), len);
*/
  }
  return (char *) ret;
}

char *targets_i2s(struct v3_ext_method*, void*)
{
  return norep();
}

char *certs_i2s(struct v3_ext_method*, void*)
{
  return norep();
}

char *null_i2s(struct v3_ext_method*, void*)
{
  return norep();
}

char *attributes_i2s(struct v3_ext_method*, void*)
{
  return norep();
}

/*
void *acseq_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  AC **list = (AC **)data;
  AC_SEQ *a;

  if (!list) return NULL;

  a = AC_SEQ_new();

  while (*list)
    sk_AC_push(a->acs, *list++);

  return (void *)a;
}
*/

void *acseq_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  AC_SEQ* acseq = NULL;
  AC** aclist = NULL;
  std::string acseq_str;
  std::string ac_str;
  if(data == NULL) return NULL;
  acseq_str = data;

  std::string::size_type pos1 = 0, pos2 = 0; 
  while(pos1 < acseq_str.length()) {
    pos1 = acseq_str.find(VOMS_AC_HEADER, pos1);
    if(pos1 == std::string::npos) break;
    pos1 = acseq_str.find_first_of("\r\n", pos1);
    if(pos1 == std::string::npos) break;
    pos2 = acseq_str.find(VOMS_AC_TRAILER, pos1);
    if(pos2 == std::string::npos) break;
    ac_str.clear();
    ac_str = acseq_str.substr(pos1+1, (pos2-1) - (pos1+1));

    pos2 = acseq_str.find_first_of("\r\n", pos2);
    if(pos2 == std::string::npos) pos2 = acseq_str.length();
    pos1 = pos2+1;

    // decode the AC string
    int size;
    char* dec = NULL;
    std::string decodedac;
    dec = Arc::VOMSDecode((char*)(ac_str.c_str()), ac_str.length(), &size);
    if (dec != NULL) {
      decodedac.append(dec, size);
      free(dec);
      dec = NULL;
    }
    // TODO: is the ac order required?
    std::string acorder;      
    Arc::addVOMSAC(aclist, acorder, decodedac);
  }

  if (!aclist) return NULL;

  acseq = AC_SEQ_new();
  while (*aclist)
    sk_AC_push(acseq->acs, *aclist++);

  return (void *)acseq;
}

void *targets_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  char *pos;
  char *list = strdup(data);
  AC_TARGETS *a = AC_TARGETS_new();

  do {
    pos = strchr(list, ',');
    if (pos)
      *pos = '\0';
    {
      GENERAL_NAME *g = GENERAL_NAME_new();
      ASN1_IA5STRING *tmpr = ASN1_IA5STRING_new();
      AC_TARGET *targ = AC_TARGET_new();

      if (!g || !tmpr || !targ) {
        GENERAL_NAME_free(g);
        ASN1_IA5STRING_free(tmpr);
        AC_TARGET_free(targ);
        goto err;
      }
      ASN1_STRING_set(tmpr, list, strlen(list));
      g->type = GEN_URI;
      g->d.ia5 = tmpr;
      targ->name = g;
      sk_AC_TARGET_push(a->targets, targ);
    }
    if (pos)
      list = pos++;
  } while (pos);

  return a;

 err:
  AC_TARGETS_free(a);
  return NULL;    

}

void *certs_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  STACK_OF(X509) *certs =
    (STACK_OF(X509) *)data;
  int i = 0;

  if (data) {
    AC_CERTS *a = AC_CERTS_new();

    sk_X509_pop_free(a->stackcert, X509_free);
    a->stackcert = sk_X509_new_null();

/*     a->stackcert = sk_X509_dup(certs); */
    for (i =0; i < sk_X509_num(certs); i++)
      sk_X509_push(a->stackcert, X509_dup(sk_X509_value(certs, i)));

    return a;
  }

  return NULL;    
}

void *attributes_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  int i = 0;

  STACK_OF(AC_ATT_HOLDER) *stack =
    (STACK_OF(AC_ATT_HOLDER) *)data;

  if (data) {
    AC_FULL_ATTRIBUTES *a = AC_FULL_ATTRIBUTES_new();
    sk_AC_ATT_HOLDER_pop_free(a->providers, AC_ATT_HOLDER_free);
    a->providers = sk_AC_ATT_HOLDER_new_null();
/*     a->providers = sk_AC_ATT_HOLDER_dup(stack); */
    for (i = 0; i < sk_AC_ATT_HOLDER_num(stack); i++)
#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
      sk_AC_ATT_HOLDER_push(a->providers,
           ASN1_dup_of(AC_ATT_HOLDER, i2d_AC_ATT_HOLDER,
           d2i_AC_ATT_HOLDER,
           sk_AC_ATT_HOLDER_value(stack, i)));
#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
      sk_AC_ATT_HOLDER_push(a->providers,
           (AC_ATT_HOLDER *)ASN1_dup((int (*)(void*, unsigned char**))i2d_AC_ATT_HOLDER,
           (void*(*)(void**, const unsigned char**, long int))d2i_AC_ATT_HOLDER,
           (char *)(sk_AC_ATT_HOLDER_value(stack, i))));
#else
      sk_AC_ATT_HOLDER_push(a->providers,
           (AC_ATT_HOLDER *)ASN1_dup((int (*)())i2d_AC_ATT_HOLDER,
           (char * (*)())d2i_AC_ATT_HOLDER,
           (char *)(sk_AC_ATT_HOLDER_value(stack, i))));
#endif
    
    return a;
  }
  return NULL;
}

void *null_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char*)
{
  return ASN1_NULL_new();
}

char *authkey_i2s(struct v3_ext_method*, void*)
{
  return norep();
}

void *authkey_s2i(struct v3_ext_method*, struct v3_ext_ctx*, char *data)
{
  X509       *cert = (X509 *)data;
  char digest[21];

  ASN1_OCTET_STRING *str = ASN1_OCTET_STRING_new();
  AUTHORITY_KEYID *keyid = AUTHORITY_KEYID_new();

  if (str && keyid) {
    ASN1_BIT_STRING* pkeystr = X509_get0_pubkey_bitstr(cert);
    SHA1(pkeystr->data,
	 pkeystr->length,
	 (unsigned char*)digest);
    ASN1_OCTET_STRING_set(str, (unsigned char*)digest, 20);
    ASN1_OCTET_STRING_free(keyid->keyid);
    keyid->keyid = str;
  }
  else {
    if (str) ASN1_OCTET_STRING_free(str);
    if (keyid) AUTHORITY_KEYID_free(keyid);
    keyid = NULL;
  }
  return keyid;
}


/*
IMPL_STACK(AC_IETFATTR)
IMPL_STACK(AC_IETFATTRVAL)
IMPL_STACK(AC_ATTR)
IMPL_STACK(AC)
IMPL_STACK(AC_INFO)
IMPL_STACK(AC_VAL)
IMPL_STACK(AC_HOLDER)
IMPL_STACK(AC_ACI)
IMPL_STACK(AC_FORM)
IMPL_STACK(AC_IS)
IMPL_STACK(AC_DIGEST)
IMPL_STACK(AC_TARGETS)
IMPL_STACK(AC_TARGET)
IMPL_STACK(AC_CERTS)

IMPL_STACK(AC_ATTRIBUTE)
IMPL_STACK(AC_ATT_HOLDER)
IMPL_STACK(AC_FULL_ATTRIBUTES)
*/


X509V3_EXT_METHOD * VOMSAttribute_auth_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_auth_x509v3_ext_meth =
  {
    -1,
    0,  
    NULL, 
    (X509V3_EXT_NEW) AUTHORITY_KEYID_new,
    (X509V3_EXT_FREE) AUTHORITY_KEYID_free,
    (X509V3_EXT_D2I) d2i_AUTHORITY_KEYID,
    (X509V3_EXT_I2D) i2d_AUTHORITY_KEYID,
    (X509V3_EXT_I2S) authkey_i2s, 
    (X509V3_EXT_S2I) authkey_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_auth_x509v3_ext_meth);
}

X509V3_EXT_METHOD * VOMSAttribute_avail_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_avail_x509v3_ext_meth =
  {
    -1,
    0,  
    NULL,
    (X509V3_EXT_NEW) ASN1_NULL_new,
    (X509V3_EXT_FREE) ASN1_NULL_free,
    (X509V3_EXT_D2I) d2i_ASN1_NULL,
    (X509V3_EXT_I2D) i2d_ASN1_NULL,
    (X509V3_EXT_I2S) null_i2s, 
    (X509V3_EXT_S2I) null_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_avail_x509v3_ext_meth);
}  

X509V3_EXT_METHOD * VOMSAttribute_targets_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_targets_x509v3_ext_meth =
  {
    -1,
    0,  
    NULL,
    (X509V3_EXT_NEW) AC_TARGETS_new,
    (X509V3_EXT_FREE) AC_TARGETS_free,
    (X509V3_EXT_D2I) d2i_AC_TARGETS,
    (X509V3_EXT_I2D) i2d_AC_TARGETS,
    (X509V3_EXT_I2S) targets_i2s, 
    (X509V3_EXT_S2I) targets_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_targets_x509v3_ext_meth);
}  

X509V3_EXT_METHOD * VOMSAttribute_acseq_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_acseq_x509v3_ext_meth =
  {
    -1,
    0,  
    NULL,
    (X509V3_EXT_NEW) AC_SEQ_new,
    (X509V3_EXT_FREE) AC_SEQ_free,
    (X509V3_EXT_D2I) d2i_AC_SEQ,
    (X509V3_EXT_I2D) i2d_AC_SEQ,
    (X509V3_EXT_I2S) acseq_i2s, 
    (X509V3_EXT_S2I) acseq_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_acseq_x509v3_ext_meth);
}  

X509V3_EXT_METHOD * VOMSAttribute_certseq_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_certseq_x509v3_ext_meth =
  {
    -1,
    0,  
    NULL,
    (X509V3_EXT_NEW) AC_CERTS_new,
    (X509V3_EXT_FREE) AC_CERTS_free,
    (X509V3_EXT_D2I) d2i_AC_CERTS,
    (X509V3_EXT_I2D) i2d_AC_CERTS,
    (X509V3_EXT_I2S) certs_i2s, 
    (X509V3_EXT_S2I) certs_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_certseq_x509v3_ext_meth);
}  

X509V3_EXT_METHOD * VOMSAttribute_attribs_x509v3_ext_meth() {
  static X509V3_EXT_METHOD vomsattribute_attribs_x509v3_ext_meth =
  {
    -1,
    0,
    NULL,
    (X509V3_EXT_NEW) AC_FULL_ATTRIBUTES_new,
    (X509V3_EXT_FREE) AC_FULL_ATTRIBUTES_free,
    (X509V3_EXT_D2I) d2i_AC_FULL_ATTRIBUTES,
    (X509V3_EXT_I2D) i2d_AC_FULL_ATTRIBUTES,
    (X509V3_EXT_I2S) attributes_i2s,  
    (X509V3_EXT_S2I) attributes_s2i,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
  };
  return (&vomsattribute_attribs_x509v3_ext_meth);
}

} //namespace ArcCredential
