/**Borrow the code about Attribute Certificate from VOMS*/

/**The VOMSAttribute.h and VOMSAttribute.cpp are integration about code written by VOMS project, 
 *so here the original license follows. 
 */

#ifndef ARC_VOMSATTRIBUTE_H
#define ARC_VOMSATTRIBUTE_H

#include <openssl/opensslv.h>

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
// Workaround for broken header in openssl 1.0.0
#include <openssl/safestack.h>
#undef SKM_ASN1_SET_OF_d2i
#define	SKM_ASN1_SET_OF_d2i(type, st, pp, length, d2i_func, free_func, ex_tag, ex_class) \
  (STACK_OF(type) *)d2i_ASN1_SET((STACK_OF(OPENSSL_BLOCK) **)CHECKED_PTR_OF(STACK_OF(type)*, st), \
				pp, length, \
				CHECKED_D2I_OF(type, d2i_func), \
				CHECKED_SK_FREE_FUNC(type, free_func), \
				ex_tag, ex_class)
#undef SKM_ASN1_SET_OF_i2d
#define	SKM_ASN1_SET_OF_i2d(type, st, pp, i2d_func, ex_tag, ex_class, is_set) \
  i2d_ASN1_SET((STACK_OF(OPENSSL_BLOCK) *)CHECKED_STACK_OF(type, st), pp, \
				CHECKED_I2D_OF(type, i2d_func), \
				ex_tag, ex_class, is_set)
#endif

#include <openssl/evp.h>
#include <openssl/asn1_mac.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/stack.h>
#include <openssl/safestack.h>
#include <openssl/err.h>

#define VOMS_AC_HEADER "-----BEGIN VOMS AC-----"
#define VOMS_AC_TRAILER "-----END VOMS AC-----"

namespace ArcCredential {

#define ASN1_F_D2I_AC_ATTR          5000
#define AC_F_ATTR_New               5001
#define ASN1_F_D2I_AC_ROLE          5002
#define AC_F_ROLE_New               5003
#define ASN1_F_D2I_AC_IETFATTR      5004
#define AC_F_IETFATTR_New           5005
#define ASN1_F_D2I_AC_IETFATTRVAL   5006
#define ASN1_F_D2I_AC_DIGEST        5007
#define AC_F_DIGEST_New             5008
#define ASN1_F_D2I_AC_IS            5009
#define AC_F_AC_IS_New              5010
#define ASN1_F_D2I_AC_FORM          5011
#define AC_F_AC_FORM_New            5012
#define ASN1_F_D2I_AC_ACI           5013
#define ASN1_F_AC_ACI_New           5014
#define ASN1_F_D2I_AC_HOLDER        5015
#define ASN1_F_AC_HOLDER_New        5016
#define ASN1_F_AC_VAL_New           5017
#define AC_F_AC_INFO_NEW            5018
#define AC_F_D2I_AC                 5019
#define AC_F_AC_New                 5020
#define ASN1_F_I2D_AC_IETFATTRVAL   5021
#define AC_F_D2I_AC_DIGEST          5022
#define AC_F_AC_DIGEST_New          5023
#define AC_F_D2I_AC_IS              5024
#define AC_ERR_UNSET                5025
#define AC_ERR_SET                  5026
#define AC_ERR_SIGNATURE            5027
#define AC_ERR_VERSION              5028
#define AC_ERR_HOLDER_SERIAL        5029
#define AC_ERR_HOLDER               5030
#define AC_ERR_UID_MISMATCH         5031
#define AC_ERR_ISSUER_NAME          5032
#define AC_ERR_SERIAL               5033
#define AC_ERR_DATES                5034
#define AC_ERR_ATTRIBS              5035
#define AC_F_AC_TARGET_New          5036
#define ASN1_F_D2I_AC_TARGET        5037
#define AC_F_AC_TARGETS_New         5036
#define ASN1_F_D2I_AC_TARGETS       5037
#define ASN1_F_D2I_AC_SEQ           5038
#define AC_F_AC_SEQ_new             5039
#define AC_ERR_ATTRIB_URI           5040
#define AC_ERR_ATTRIB_FQAN          5041
#define AC_ERR_EXTS_ABSENT          5042
#define AC_ERR_MEMORY               5043
#define AC_ERR_EXT_CRIT             5044
#define AC_ERR_EXT_TARGET           5045
#define AC_ERR_EXT_KEY              5046
#define AC_ERR_UNKNOWN              5047

#define AC_ERR_PARAMETERS           5048
#define X509_ERR_ISSUER_NAME        5049
#define X509_ERR_HOLDER_NAME        5050
#define AC_ERR_NO_EXTENSION         5051

#define ASN1_F_D2I_AC_CERTS         5052
#define AC_F_X509_New               5053

#define AC_F_D2I_AC_ATTRIBUTE       5054
#define AC_F_ATTRIBUTE_New          5055
#define ASN1_F_D2I_AC_ATT_HOLDER    5056
#define AC_F_AC_ATT_HOLDER_New      5057
#define ASN1_F_D2I_AC_FULL_ATTRIBUTES 5058
#define AC_F_AC_FULL_ATTRIBUTES_New 5059
#define ASN1_F_D2I_AC_ATTRIBUTEVAL  5060
#define ASN1_F_I2D_AC_ATTRIBUTEVAL  5061
#define AC_F_AC_ATTRIBUTEVAL_New    5062
#define AC_ERR_ATTRIB               5063

typedef struct ACDIGEST {
  ASN1_ENUMERATED *type;
  ASN1_OBJECT     *oid;
  X509_ALGOR      *algor;
  ASN1_BIT_STRING *digest;
} AC_DIGEST;

typedef struct ACIS {
  STACK_OF(GENERAL_NAME) *issuer;
  ASN1_INTEGER  *serial;
  ASN1_BIT_STRING *uid;
} AC_IS;

typedef struct ACFORM {
  STACK_OF(GENERAL_NAME) *names;
  AC_IS         *is;
  AC_DIGEST     *digest;
} AC_FORM;

typedef struct ACACI {
  STACK_OF(GENERAL_NAME) *names;
  AC_FORM       *form;
} AC_ACI;

typedef struct ACHOLDER {
  AC_IS         *baseid;
  STACK_OF(GENERAL_NAME) *name;
  AC_DIGEST     *digest;
} AC_HOLDER;

typedef struct ACVAL {
  ASN1_GENERALIZEDTIME *notBefore;
  ASN1_GENERALIZEDTIME *notAfter;
} AC_VAL;

typedef struct asn1_string_st AC_IETFATTRVAL;

typedef struct ACIETFATTR {
  STACK_OF(GENERAL_NAME)   *names;
  STACK_OF(AC_IETFATTRVAL) *values;
} AC_IETFATTR;

typedef struct ACTARGET {
  GENERAL_NAME *name;
  GENERAL_NAME *group;
  AC_IS        *cert;
} AC_TARGET;
 
typedef struct ACTARGETS {
  STACK_OF(AC_TARGET) *targets;
} AC_TARGETS;

typedef struct ACATTR {
  ASN1_OBJECT * type;
  int get_type;
  STACK_OF(AC_IETFATTR) *ietfattr;
  STACK_OF(AC_FULL_ATTRIBUTES) *fullattributes;
} AC_ATTR;
#define GET_TYPE_FQAN 1
#define GET_TYPE_ATTRIBUTES 2

typedef struct ACINFO {
  ASN1_INTEGER             *version;
  AC_HOLDER                *holder;
  AC_FORM                  *form;
  X509_ALGOR               *alg;
  ASN1_INTEGER             *serial;
  AC_VAL                   *validity;
  STACK_OF(AC_ATTR)        *attrib;
  ASN1_BIT_STRING          *id;
  STACK_OF(X509_EXTENSION) *exts;
} AC_INFO;

typedef struct ACC {
  AC_INFO         *acinfo;
  X509_ALGOR      *sig_alg;
  ASN1_BIT_STRING *signature;
} AC;

typedef struct ACSEQ {
  STACK_OF(AC) *acs;
} AC_SEQ;

typedef struct ACCERTS {
  STACK_OF(X509) *stackcert;
} AC_CERTS;

typedef struct ACATTRIBUTE {
  ASN1_OCTET_STRING *name;
  ASN1_OCTET_STRING *qualifier;
  ASN1_OCTET_STRING *value;
} AC_ATTRIBUTE;

typedef struct ACATTHOLDER {
  STACK_OF(GENERAL_NAME) *grantor;
  STACK_OF(AC_ATTRIBUTE) *attributes;
} AC_ATT_HOLDER;

typedef struct ACFULLATTRIBUTES {
  STACK_OF(AC_ATT_HOLDER) *providers;
} AC_FULL_ATTRIBUTES;

#if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
#define IMPL_STACK(type)						\
  DECLARE_STACK_OF(type)						\
  STACK_OF(type) *sk_##type##_new (int (*cmp)(const type * const *,	\
					      const type * const *))	\
    { return (STACK_OF(type) *)sk_new(CHECKED_SK_CMP_FUNC(type, cmp));}			\
  STACK_OF(type) *sk_##type##_new_null () { return (STACK_OF(type) *)sk_new_null(); }	\
  void   sk_##type##_free (STACK_OF(type) *st) { sk_free(CHECKED_STACK_OF(type, st)); } \
  int    sk_##type##_num (const STACK_OF(type) *st) { return sk_num(CHECKED_STACK_OF(type, st)); } \
  type  *sk_##type##_value (const STACK_OF(type) *st, int i) { return (type *)sk_value(CHECKED_STACK_OF(type, st), i); } \
  type  *sk_##type##_set (STACK_OF(type) *st, int i, type *val) { return (type *)sk_set(CHECKED_STACK_OF(type, st), i, CHECKED_PTR_OF(type, val)); } \
  void   sk_##type##_zero (STACK_OF(type) *st) { sk_zero(CHECKED_STACK_OF(type, st)); } \
  int    sk_##type##_push (STACK_OF(type) *st, type *val) { return sk_push(CHECKED_STACK_OF(type, st), CHECKED_PTR_OF(type, val)); } \
  int    sk_##type##_unshift (STACK_OF(type) *st, type *val) { return sk_unshift(CHECKED_STACK_OF(type, st), CHECKED_PTR_OF(type, val)); } \
  int    sk_##type##_find (STACK_OF(type) *st, type *val) { return sk_find(CHECKED_STACK_OF(type, st), CHECKED_PTR_OF(type, val)); } \
  type  *sk_##type##_delete (STACK_OF(type) *st, int i) { return (type *)sk_delete(CHECKED_STACK_OF(type, st), i); } \
  type  *sk_##type##_delete_ptr (STACK_OF(type) *st, type *ptr) { return (type *)sk_delete_ptr(CHECKED_STACK_OF(type, st), CHECKED_PTR_OF(type, ptr)); } \
  int    sk_##type##_insert (STACK_OF(type) *st, type *val, int i) { return sk_insert(CHECKED_STACK_OF(type, st), CHECKED_PTR_OF(type, val), i); } \
  int (*sk_##type##_set_cmp_func (STACK_OF(type) *st, int (*cmp)(const type * const *, const type * const *)))(const type * const *, const type * const *) \
    { return (int ((*)(const type * const *, const type * const *)))sk_set_cmp_func(CHECKED_STACK_OF(type, st), CHECKED_SK_CMP_FUNC(type, cmp)); } \
  STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *st) { return (STACK_OF(type) *)sk_dup(CHECKED_STACK_OF(type, st)); } \
  void   sk_##type##_pop_free (STACK_OF(type) *st, void (*func)(type *)) { sk_pop_free(CHECKED_STACK_OF(type, st), CHECKED_SK_FREE_FUNC(type, func)); } \
  type  *sk_##type##_shift (STACK_OF(type) *st) { return (type *)sk_shift(CHECKED_STACK_OF(type, st)); } \
  type  *sk_##type##_pop (STACK_OF(type) *st) { return (type *)sk_pop(CHECKED_STACK_OF(type, st)); } \
  void   sk_##type##_sort (STACK_OF(type) *st) { sk_sort(CHECKED_STACK_OF(type, st)); }		\
  STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **st, const unsigned char **pp, long length, type *(*d2ifunc)(type**, const unsigned char**, long), void (*freefunc)(type *), int ex_tag, int ex_class) \
    { return (STACK_OF(type) *)d2i_ASN1_SET((STACK_OF(OPENSSL_BLOCK)**)CHECKED_PTR_OF(STACK_OF(type)*, st), \
				pp, length, \
				CHECKED_D2I_OF(type, d2ifunc), \
				CHECKED_SK_FREE_FUNC(type, freefunc), \
				ex_tag, ex_class); } \
  int i2d_ASN1_SET_OF_##type (STACK_OF(type) *st, unsigned char **pp, int (*i2dfunc)(type*, unsigned char**), int ex_tag, int ex_class, int is_set) \
  { return i2d_ASN1_SET((STACK_OF(OPENSSL_BLOCK)*)CHECKED_STACK_OF(type, st), pp, \
				CHECKED_I2D_OF(type, i2dfunc), \
				ex_tag, ex_class, is_set); }	\
  unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *st, int (*i2dfunc)(type*, unsigned char**), unsigned char **buf, int *len) { return ASN1_seq_pack((STACK_OF(OPENSSL_BLOCK)*)CHECKED_PTR_OF(STACK_OF(type), st), \
			CHECKED_I2D_OF(type, i2dfunc), buf, len); } \
  STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *buf, int len, type *(*d2ifunc)(type**, const unsigned char**, long), void (*freefunc)(type *)) \
       { return (STACK_OF(type) *)ASN1_seq_unpack(buf, len, CHECKED_D2I_OF(type, d2ifunc), CHECKED_SK_FREE_FUNC(type, freefunc)); }

#define DECL_STACK(type)  \
   PREDECLARE_STACK_OF(type) \
   STACK_OF(type) *sk_##type##_new (int (*)(const type * const *, const type * const *)); \
   STACK_OF(type) *sk_##type##_new_null (); \
   void   sk_##type##_free (STACK_OF(type) *); \
   int    sk_##type##_num (const STACK_OF(type) *); \
   type  *sk_##type##_value (const STACK_OF(type) *, int); \
   type  *sk_##type##_set (STACK_OF(type) *, int, type *); \
   void   sk_##type##_zero (STACK_OF(type) *); \
   int    sk_##type##_push (STACK_OF(type) *, type *); \
   int    sk_##type##_unshift (STACK_OF(type) *, type *); \
   int    sk_##type##_find (STACK_OF(type) *, type *); \
   type  *sk_##type##_delete (STACK_OF(type) *, int); \
   type  *sk_##type##_delete_ptr (STACK_OF(type) *, type *); \
   int    sk_##type##_insert (STACK_OF(type) *, type *, int); \
   int (*sk_##type##_set_cmp_func (STACK_OF(type) *, int (*)(const type * const *, const type * const *)))(const type * const *, const type * const *); \
   STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *); \
   void   sk_##type##_pop_free (STACK_OF(type) *, void (*)(type *)); \
   type  *sk_##type##_shift (STACK_OF(type) *); \
   type  *sk_##type##_pop (STACK_OF(type) *); \
   void   sk_##type##_sort (STACK_OF(type) *); \
   STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **, const unsigned char **, long, type *(*)(type**, const unsigned char **, long), void (*)(type *), int, int); \
   int i2d_ASN1_SET_OF_##type (STACK_OF(type) *, unsigned char **, int (*)(type*, unsigned char**), int, int, int); \
   unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *, int (*)(type*, unsigned char**), unsigned char **, int *); \
   STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *, int, type *(*)(), void (*)(type *)) ;

#elif (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
#define IMPL_STACK(type) \
   DECLARE_STACK_OF(type) \
   STACK_OF(type) *sk_##type##_new (int (*cmp)(const type * const *, const type * const *)) \
       { return sk_new ( (int (*)(const char * const *, const char * const *))cmp);} \
   STACK_OF(type) *sk_##type##_new_null () { return sk_new_null(); } \
   void   sk_##type##_free (STACK_OF(type) *st) { sk_free(st); } \
   int    sk_##type##_num (const STACK_OF(type) *st) { return sk_num(st); } \
   type  *sk_##type##_value (const STACK_OF(type) *st, int i) { return (type *)sk_value(st, i); } \
   type  *sk_##type##_set (STACK_OF(type) *st, int i, type *val) { return ((type *)sk_set(st, i, (char *)val)); } \
   void   sk_##type##_zero (STACK_OF(type) *st) { sk_zero(st);} \
   int    sk_##type##_push (STACK_OF(type) *st, type *val) { return sk_push(st, (char *)val); } \
   int    sk_##type##_unshift (STACK_OF(type) *st, type *val) { return sk_unshift(st, (char *)val); } \
   int    sk_##type##_find (STACK_OF(type) *st, type *val) { return sk_find(st, (char *)val); } \
   type  *sk_##type##_delete (STACK_OF(type) *st, int i) { return (type *)sk_delete(st, i); } \
   type  *sk_##type##_delete_ptr (STACK_OF(type) *st, type *ptr) { return (type *)sk_delete_ptr(st, (char *)ptr); } \
   int    sk_##type##_insert (STACK_OF(type) *st, type *val, int i) { return sk_insert(st, (char *)val, i); } \
   int (*sk_##type##_set_cmp_func (STACK_OF(type) *st, int (*cmp)(const type * const *, const type * const *)))(const type * const *, const type * const *) \
       { return (int ((*)(const type * const *, const type * const *)))sk_set_cmp_func (st, (int (*)(const char * const *, const char * const *))cmp); } \
   STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *st) { return sk_dup(st); } \
   void   sk_##type##_pop_free (STACK_OF(type) *st, void (*func)(type *)) { sk_pop_free(st, (void (*)(void *))func); } \
   type  *sk_##type##_shift (STACK_OF(type) *st) { return (type *)sk_shift(st); } \
   type  *sk_##type##_pop (STACK_OF(type) *st) { return (type *)sk_pop(st); } \
   void   sk_##type##_sort (STACK_OF(type) *st) { sk_sort(st); } \
   STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **st, const unsigned char **pp, long length, type *(*d2ifunc)(), void (*freefunc)(type *), int ex_tag, int ex_class) \
       { return d2i_ASN1_SET(st, pp, length, (void*(*)(void**, const unsigned char**, long int))d2ifunc, (void (*)(void *))freefunc, ex_tag, ex_class); } \
   int i2d_ASN1_SET_OF_##type (STACK_OF(type) *st, unsigned char **pp, int (*i2dfunc)(void*, unsigned char**), int ex_tag, int ex_class, int is_set) \
       { return i2d_ASN1_SET(st, pp, i2dfunc, ex_tag, ex_class, is_set); }  \
   unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *st, int (*i2d)(void*, unsigned char**), unsigned char **buf, int *len) { return ASN1_seq_pack(st, i2d, buf, len); } \
   STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *buf, int len, type *(*d2i)(), void (*freefunc)(type *)) \
       { return ASN1_seq_unpack(buf, len, (void*(*)(void**, const unsigned char**, long int))d2i, (void (*)(void *))freefunc); }

#define DECL_STACK(type) \
   PREDECLARE_STACK_OF(type) \
   STACK_OF(type) *sk_##type##_new (int (*)(const type * const *, const type * const *)); \
   STACK_OF(type) *sk_##type##_new_null (); \
   void   sk_##type##_free (STACK_OF(type) *); \
   int    sk_##type##_num (const STACK_OF(type) *); \
   type  *sk_##type##_value (const STACK_OF(type) *, int); \
   type  *sk_##type##_set (STACK_OF(type) *, int, type *); \
   void   sk_##type##_zero (STACK_OF(type) *); \
   int    sk_##type##_push (STACK_OF(type) *, type *); \
   int    sk_##type##_unshift (STACK_OF(type) *, type *); \
   int    sk_##type##_find (STACK_OF(type) *, type *); \
   type  *sk_##type##_delete (STACK_OF(type) *, int); \
   type  *sk_##type##_delete_ptr (STACK_OF(type) *, type *); \
   int    sk_##type##_insert (STACK_OF(type) *, type *, int); \
   int (*sk_##type##_set_cmp_func (STACK_OF(type) *, int (*)(const type * const *, const type * const *)))(const type * const *, const type * const *); \
   STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *); \
   void   sk_##type##_pop_free (STACK_OF(type) *, void (*)(type *)); \
   type  *sk_##type##_shift (STACK_OF(type) *); \
   type  *sk_##type##_pop (STACK_OF(type) *); \
   void   sk_##type##_sort (STACK_OF(type) *); \
   STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **, const unsigned char **, long, type *(*)(), void (*)(type *), int, int); \
   int i2d_ASN1_SET_OF_##type (STACK_OF(type) *, unsigned char **, int (*)(void*, unsigned char**), int, int, int); \
   unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *, int (*)(void*, unsigned char**), unsigned char **, int *); \
   STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *, int, type *(*)(), void (*)(type *)) ;

#else
#define IMPL_STACK(type) \
   DECLARE_STACK_OF(type) \
   STACK_OF(type) *sk_##type##_new (int (*cmp)(const type * const *, const type * const *)) \
       { return sk_new ( (int (*)(const char * const *, const char * const *))cmp);} \
   STACK_OF(type) *sk_##type##_new_null () { return sk_new_null(); } \
   void   sk_##type##_free (STACK_OF(type) *st) { sk_free(st); } \
   int    sk_##type##_num (const STACK_OF(type) *st) { return sk_num(st); } \
   type  *sk_##type##_value (const STACK_OF(type) *st, int i) { return (type *)sk_value(st, i); } \
   type  *sk_##type##_set (STACK_OF(type) *st, int i, type *val) { return ((type *)sk_set(st, i, (char *)val)); } \
   void   sk_##type##_zero (STACK_OF(type) *st) { sk_zero(st);} \
   int    sk_##type##_push (STACK_OF(type) *st, type *val) { return sk_push(st, (char *)val); } \
   int    sk_##type##_unshift (STACK_OF(type) *st, type *val) { return sk_unshift(st, (char *)val); } \
   int    sk_##type##_find (STACK_OF(type) *st, type *val) { return sk_find(st, (char *)val); } \
   type  *sk_##type##_delete (STACK_OF(type) *st, int i) { return (type *)sk_delete(st, i); } \
   type  *sk_##type##_delete_ptr (STACK_OF(type) *st, type *ptr) { return (type *)sk_delete_ptr(st, (char *)ptr); } \
   int    sk_##type##_insert (STACK_OF(type) *st, type *val, int i) { return sk_insert(st, (char *)val, i); } \
   int (*sk_##type##_set_cmp_func (STACK_OF(type) *st, int (*cmp)(const type * const *, const type * const *)))(const type * const *, const type * const *) \
       { return (int ((*)(const type * const *, const type * const *)))sk_set_cmp_func (st, (int (*)(const char * const *, const char * const *))cmp); } \
   STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *st) { return sk_dup(st); } \
   void   sk_##type##_pop_free (STACK_OF(type) *st, void (*func)(type *)) { sk_pop_free(st, (void (*)(void *))func); } \
   type  *sk_##type##_shift (STACK_OF(type) *st) { return (type *)sk_shift(st); } \
   type  *sk_##type##_pop (STACK_OF(type) *st) { return (type *)sk_pop(st); } \
   void   sk_##type##_sort (STACK_OF(type) *st) { sk_sort(st); } \
   STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **st, unsigned char **pp, long length, type *(*d2ifunc)(), void (*freefunc)(type *), int ex_tag, int ex_class) \
       { return d2i_ASN1_SET(st, pp, length, (char *(*)())d2ifunc, (void (*)(void *))freefunc, ex_tag, ex_class); } \
   int i2d_ASN1_SET_OF_##type (STACK_OF(type) *st, unsigned char **pp, int (*i2dfunc)(), int ex_tag, int ex_class, int is_set) \
       { return i2d_ASN1_SET(st, pp, i2dfunc, ex_tag, ex_class, is_set); }  \
   unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *st, int (*i2d)(), unsigned char **buf, int *len) { return ASN1_seq_pack(st, i2d, buf, len); } \
   STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *buf, int len, type *(*d2i)(), void (*freefunc)(type *)) \
       { return ASN1_seq_unpack(buf, len, (char *(*)())d2i, (void (*)(void *))freefunc); }

#define DECL_STACK(type) \
   PREDECLARE_STACK_OF(type) \
   STACK_OF(type) *sk_##type##_new (int (*)(const type * const *, const type * const *)); \
   STACK_OF(type) *sk_##type##_new_null (); \
   void   sk_##type##_free (STACK_OF(type) *); \
   int    sk_##type##_num (const STACK_OF(type) *); \
   type  *sk_##type##_value (const STACK_OF(type) *, int); \
   type  *sk_##type##_set (STACK_OF(type) *, int, type *); \
   void   sk_##type##_zero (STACK_OF(type) *); \
   int    sk_##type##_push (STACK_OF(type) *, type *); \
   int    sk_##type##_unshift (STACK_OF(type) *, type *); \
   int    sk_##type##_find (STACK_OF(type) *, type *); \
   type  *sk_##type##_delete (STACK_OF(type) *, int); \
   type  *sk_##type##_delete_ptr (STACK_OF(type) *, type *); \
   int    sk_##type##_insert (STACK_OF(type) *, type *, int); \
   int (*sk_##type##_set_cmp_func (STACK_OF(type) *, int (*)(const type * const *, const type * const *)))(const type * const *, const type * const *); \
   STACK_OF(type) *sk_##type##_dup (STACK_OF(type) *); \
   void   sk_##type##_pop_free (STACK_OF(type) *, void (*)(type *)); \
   type  *sk_##type##_shift (STACK_OF(type) *); \
   type  *sk_##type##_pop (STACK_OF(type) *); \
   void   sk_##type##_sort (STACK_OF(type) *); \
   STACK_OF(type) *d2i_ASN1_SET_OF_##type (STACK_OF(type) **, unsigned char **, long, type *(*)(), void (*)(type *), int, int); \
   int i2d_ASN1_SET_OF_##type (STACK_OF(type) *, unsigned char **, int (*)(), int, int, int); \
   unsigned char *ASN1_seq_pack_##type (STACK_OF(type) *, int (*)(), unsigned char **, int *); \
   STACK_OF(type) *ASN1_seq_unpack_##type (unsigned char *, int, type *(*)(), void (*)(type *)) ;
#endif

DECL_STACK(AC_TARGET)
DECL_STACK(AC_TARGETS)
DECL_STACK(AC_IETFATTR)
DECL_STACK(AC_IETFATTRVAL)
DECL_STACK(AC_ATTR)
DECL_STACK(AC)
DECL_STACK(AC_INFO)
DECL_STACK(AC_VAL)
DECL_STACK(AC_HOLDER)
DECL_STACK(AC_ACI)
DECL_STACK(AC_FORM)
DECL_STACK(AC_IS)
DECL_STACK(AC_DIGEST)
DECL_STACK(AC_CERTS)
DECL_STACK(AC_ATTRIBUTE)
DECL_STACK(AC_ATT_HOLDER)
DECL_STACK(AC_FULL_ATTRIBUTES)

#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL)
#define SSLCONST const
#else
#define SSLCONST
#endif

int i2d_AC_ATTR(AC_ATTR *a, unsigned char **pp);
AC_ATTR *d2i_AC_ATTR(AC_ATTR **a, SSLCONST unsigned char **p, long length);
AC_ATTR *AC_ATTR_new();
void AC_ATTR_free(AC_ATTR *a);

int i2d_AC_IETFATTR(AC_IETFATTR *a, unsigned char **pp);
AC_IETFATTR *d2i_AC_IETFATTR(AC_IETFATTR **a, SSLCONST unsigned char **p, long length);
AC_IETFATTR *AC_IETFATTR_new();
void AC_IETFATTR_free (AC_IETFATTR *a);

int i2d_AC_IETFATTRVAL(AC_IETFATTRVAL *a, unsigned char **pp);
AC_IETFATTRVAL *d2i_AC_IETFATTRVAL(AC_IETFATTRVAL **a, SSLCONST unsigned char **pp, long length);
AC_IETFATTRVAL *AC_IETFATTRVAL_new();
void AC_IETFATTRVAL_free(AC_IETFATTRVAL *a);

int i2d_AC_DIGEST(AC_DIGEST *a, unsigned char **pp);
AC_DIGEST *d2i_AC_DIGEST(AC_DIGEST **a, SSLCONST unsigned char **pp, long length);
AC_DIGEST *AC_DIGEST_new(void);
void AC_DIGEST_free(AC_DIGEST *a);

int i2d_AC_IS(AC_IS *a, unsigned char **pp);
AC_IS *d2i_AC_IS(AC_IS **a, SSLCONST unsigned char **pp, long length);
AC_IS *AC_IS_new(void);
void AC_IS_free(AC_IS *a);

int i2d_AC_FORM(AC_FORM *a, unsigned char **pp);
AC_FORM *d2i_AC_FORM(AC_FORM **a, SSLCONST unsigned char **pp, long length);
AC_FORM *AC_FORM_new(void);
void AC_FORM_free(AC_FORM *a);

int i2d_AC_ACI(AC_ACI *a, unsigned char **pp);
AC_ACI *d2i_AC_ACI(AC_ACI **a, SSLCONST unsigned char **pp, long length);
AC_ACI *AC_ACI_new(void);
void AC_ACI_free(AC_ACI *a);

int i2d_AC_HOLDER(AC_HOLDER *a, unsigned char **pp);
AC_HOLDER *d2i_AC_HOLDER(AC_HOLDER **a, SSLCONST unsigned char **pp, long length);
AC_HOLDER *AC_HOLDER_new(void);
void AC_HOLDER_free(AC_HOLDER *a);

/* new AC_VAL functions by Valerio */
int i2d_AC_VAL(AC_VAL *a, unsigned char **pp);
AC_VAL *d2i_AC_VAL(AC_VAL **a, SSLCONST unsigned char **pp, long length);
AC_VAL *AC_VAL_new(void);
void AC_VAL_free(AC_VAL *a);
/* end */

int i2d_AC_INFO(AC_INFO *a, unsigned char **pp);
AC_INFO *d2i_AC_INFO(AC_INFO **a, SSLCONST unsigned char **p, long length);
AC_INFO *AC_INFO_new(void);
void AC_INFO_free(AC_INFO *a);

int i2d_AC(AC *a, unsigned char **pp) ;
AC *d2i_AC(AC **a, SSLCONST unsigned char **pp, long length);
AC *AC_new(void);
void AC_free(AC *a);

int i2d_AC_TARGETS(AC_TARGETS *a, unsigned char **pp) ;
AC_TARGETS *d2i_AC_TARGETS(AC_TARGETS **a, SSLCONST unsigned char **pp, long length);
AC_TARGETS *AC_TARGETS_new(void);
void AC_TARGETS_free(AC_TARGETS *a);

int i2d_AC_TARGET(AC_TARGET *a, unsigned char **pp) ;
AC_TARGET *d2i_AC_TARGET(AC_TARGET **a, SSLCONST unsigned char **pp, long length);
AC_TARGET *AC_TARGET_new(void);
void AC_TARGET_free(AC_TARGET *a);

int i2d_AC_SEQ(AC_SEQ *a, unsigned char **pp) ;
AC_SEQ *d2i_AC_SEQ(AC_SEQ **a, SSLCONST unsigned char **pp, long length);
AC_SEQ *AC_SEQ_new(void);
void AC_SEQ_free(AC_SEQ *a);

int i2d_AC_CERTS(AC_CERTS *a, unsigned char **pp) ;
AC_CERTS *d2i_AC_CERTS(AC_CERTS **a, SSLCONST unsigned char **pp, long length);
AC_CERTS *AC_CERTS_new(void);
void AC_CERTS_free(AC_CERTS *a);

int i2d_AC_ATTRIBUTE(AC_ATTRIBUTE *, unsigned char **);
AC_ATTRIBUTE *d2i_AC_ATTRIBUTE(AC_ATTRIBUTE **, SSLCONST unsigned char **, long);
AC_ATTRIBUTE *AC_ATTRIBUTE_new();
void AC_ATTRIBUTE_free(AC_ATTRIBUTE *);

int i2d_AC_ATT_HOLDER(AC_ATT_HOLDER *, unsigned char **);
AC_ATT_HOLDER *d2i_AC_ATT_HOLDER(AC_ATT_HOLDER **, SSLCONST unsigned char **, long);
AC_ATT_HOLDER *AC_ATT_HOLDER_new();
void AC_ATT_HOLDER_free(AC_ATT_HOLDER *);

int i2d_AC_FULL_ATTRIBUTES(AC_FULL_ATTRIBUTES *, unsigned char **);
AC_FULL_ATTRIBUTES *d2i_AC_FULL_ATTRIBUTES(AC_FULL_ATTRIBUTES **, SSLCONST unsigned char **, long);
AC_FULL_ATTRIBUTES *AC_FULL_ATTRIBUTES_new();
void AC_FULL_ATTRIBUTES_free(AC_FULL_ATTRIBUTES *);

X509V3_EXT_METHOD * VOMSAttribute_auth_x509v3_ext_meth();
X509V3_EXT_METHOD * VOMSAttribute_avail_x509v3_ext_meth();
X509V3_EXT_METHOD * VOMSAttribute_targets_x509v3_ext_meth();
X509V3_EXT_METHOD * VOMSAttribute_acseq_x509v3_ext_meth();
X509V3_EXT_METHOD * VOMSAttribute_certseq_x509v3_ext_meth();
X509V3_EXT_METHOD * VOMSAttribute_attribs_x509v3_ext_meth();

} // namespace ArcCredential

#endif
