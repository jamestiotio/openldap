/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1999-2006 The OpenLDAP Foundation.
 * Portions Copyright 2001-2003 Pierangelo Masarati.
 * Portions Copyright 1999-2003 Howard Chu.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by the Howard Chu for inclusion
 * in OpenLDAP Software and subsequently enhanced by Pierangelo
 * Masarati.
 */

#ifndef SLAPD_LDAP_H
#error "include servers/slapd/back-ldap/back-ldap.h before this file!"
#endif /* SLAPD_LDAP_H */

#ifndef SLAPD_META_H
#define SLAPD_META_H

#include "proto-meta.h"

/* String rewrite library */
#include "rewrite.h"
LDAP_BEGIN_DECL

struct slap_conn;
struct slap_op;

/* from back-ldap.h before rwm removal */
struct ldapmap {
	int drop_missing;

	Avlnode *map;
	Avlnode *remap;
};

struct ldapmapping {
	struct berval src;
	struct berval dst;
};

struct ldaprwmap {
	/*
	 * DN rewriting
	 */
#ifdef ENABLE_REWRITE
	struct rewrite_info *rwm_rw;
#else /* !ENABLE_REWRITE */
	/* some time the suffix massaging without librewrite
	 * will be disabled */
	BerVarray rwm_suffix_massage;
#endif /* !ENABLE_REWRITE */

	/*
	 * Attribute/objectClass mapping
	 */
	struct ldapmap rwm_oc;
	struct ldapmap rwm_at;
};

/* Whatever context ldap_back_dn_massage needs... */
typedef struct dncookie {
	struct metatarget_t	*target;

#ifdef ENABLE_REWRITE
	Connection		*conn;
	char			*ctx;
	SlapReply		*rs;
#else
	int			normalized;
	int			tofrom;
#endif
} dncookie;

int ldap_back_dn_massage(dncookie *dc, struct berval *dn,
	struct berval *res);

extern int ldap_back_conn_cmp( const void *c1, const void *c2);
extern int ldap_back_conn_dup( void *c1, void *c2 );
extern void ldap_back_conn_free( void *c );

/* attributeType/objectClass mapping */
int mapping_cmp (const void *, const void *);
int mapping_dup (void *, void *);

void ldap_back_map_init ( struct ldapmap *lm, struct ldapmapping ** );
int ldap_back_mapping ( struct ldapmap *map, struct berval *s,
	struct ldapmapping **m, int remap );
void ldap_back_map ( struct ldapmap *map, struct berval *s, struct berval *m,
	int remap );
#define BACKLDAP_MAP	0
#define BACKLDAP_REMAP	1
char *
ldap_back_map_filter(
	struct ldapmap *at_map,
	struct ldapmap *oc_map,
	struct berval *f,
	int remap );

int
ldap_back_map_attrs(
	struct ldapmap *at_map,
	AttributeName *a,
	int remap,
	char ***mapped_attrs );

extern int ldap_back_map_config(
	struct ldapmap	*oc_map,
	struct ldapmap	*at_map,
	const char	*fname,
	int		lineno,
	int		argc,
	char		**argv );

extern int
ldap_back_filter_map_rewrite(
	dncookie	*dc,
	Filter		*f,
	struct berval	*fstr,
	int		remap );

/* suffix massaging by means of librewrite */
#ifdef ENABLE_REWRITE
extern int
suffix_massage_config( struct rewrite_info *info,
	struct berval *pvnc,
	struct berval *nvnc,
	struct berval *prnc,
	struct berval *nrnc );
#endif /* ENABLE_REWRITE */
extern int
ldap_back_referral_result_rewrite(
	dncookie	*dc,
	BerVarray	a_vals );
extern int
ldap_dnattr_rewrite(
	dncookie	*dc,
	BerVarray	a_vals );
extern int
ldap_dnattr_result_rewrite(
	dncookie	*dc,
	BerVarray	a_vals );

/* (end of) from back-ldap.h before rwm removal */

struct metainfo_t;

typedef struct metasingleconn_t {
	int			msc_candidate;
#define	META_NOT_CANDIDATE	((ber_tag_t)0x0)
#define	META_CANDIDATE		((ber_tag_t)0x1)
#define	META_BINDING		((ber_tag_t)0x2)

#define META_CND_ISSET(rs,f)		( ( (rs)->sr_tag & (f) ) == (f) )
#define META_CND_SET(rs,f)		( (rs)->sr_tag |= (f) )
#define META_CND_CLEAR(rs,f)		( (rs)->sr_tag &= ~(f) )

#define META_CANDIDATE_RESET(rs)	( (rs)->sr_tag = 0 )
#define META_IS_CANDIDATE(rs)		META_CND_ISSET( (rs), META_CANDIDATE )
#define META_CANDIDATE_SET(rs)		META_CND_SET( (rs), META_CANDIDATE )
#define META_CANDIDATE_CLEAR(rs)	META_CND_CLEAR( (rs), META_CANDIDATE )
#define META_IS_BINDING(rs)		META_CND_ISSET( (rs), META_BINDING )
#define META_BINDING_SET(rs)		META_CND_SET( (rs), META_BINDING )
#define META_BINDING_CLEAR(rs)		META_CND_CLEAR( (rs), META_BINDING )
	
	LDAP            	*msc_ld;
	struct berval          	msc_bound_ndn;
	struct berval		msc_cred;
	unsigned		msc_mscflags;
	/* NOTE: lc_lcflags is redefined to msc_mscflags to reuse the macros
	 * defined for back-ldap */
#define	lc_lcflags		msc_mscflags

	struct metainfo_t	*msc_info;
} metasingleconn_t;

typedef struct metaconn_t {
	struct slap_conn	*mc_conn;
	unsigned		mc_refcnt;

	time_t			mc_create_time;
	time_t			mc_time;
	
	struct berval          	mc_local_ndn;
	/* NOTE: msc_mscflags is used to recycle the #define
	 * in metasingleconn_t */
	unsigned		msc_mscflags;

	/*
	 * means that the connection is bound; 
	 * of course only one target actually is ...
	 */
	int             	mc_authz_target;
#define META_BOUND_NONE		(-1)
#define META_BOUND_ALL		(-2)
	/* supersedes the connection stuff */
	metasingleconn_t	mc_conns[ 1 ];
	/* NOTE: mc_conns must be last, because
	 * the required number of conns is malloc'ed
	 * in one block with the metaconn_t structure */
} metaconn_t;

typedef struct metatarget_t {
	char			*mt_uri;
	ldap_pvt_thread_mutex_t	mt_uri_mutex;

	/* TODO: we might want to enable different strategies
	 * for different targets */
	LDAP_REBIND_PROC	*mt_rebind_f;
	LDAP_URLLIST_PROC	*mt_urllist_f;
	void			*mt_urllist_p;

	BerVarray		mt_subtree_exclude;
	int			mt_scope;

	struct berval		mt_psuffix;		/* pretty suffix */
	struct berval		mt_nsuffix;		/* normalized suffix */

	struct berval		mt_binddn;
	struct berval		mt_bindpw;

	struct berval           mt_pseudorootdn;
	struct berval           mt_pseudorootpw;

	int			mt_nretries;
#define META_RETRY_UNDEFINED	(-2)
#define META_RETRY_FOREVER	(-1)
#define META_RETRY_NEVER	(0)
#define META_RETRY_DEFAULT	(3)

	struct ldaprwmap	mt_rwmap;

	sig_atomic_t		mt_isquarantined;
	slap_retry_info_t	mt_quarantine;
	ldap_pvt_thread_mutex_t	mt_quarantine_mutex;
#define	META_BACK_TGT_QUARANTINE(mt)	( (mt)->mt_quarantine.ri_num != NULL )

	unsigned		mt_flags;
#define	META_BACK_TGT_ISSET(mt,f)		( ( (mt)->mt_flags & (f) ) == (f) )
#define	META_BACK_TGT_ISMASK(mt,m,f)		( ( (mt)->mt_flags & (m) ) == (f) )

#define	META_BACK_TGT_T_F(mt)			META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_T_F_MASK, LDAP_BACK_F_T_F )
#define	META_BACK_TGT_T_F_DISCOVER(mt)		META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_T_F_MASK2, LDAP_BACK_F_T_F_DISCOVER )

#define	META_BACK_TGT_ABANDON(mt)		META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_CANCEL_MASK, LDAP_BACK_F_CANCEL_ABANDON )
#define	META_BACK_TGT_IGNORE(mt)		META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_CANCEL_MASK, LDAP_BACK_F_CANCEL_IGNORE )
#define	META_BACK_TGT_CANCEL(mt)		META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_CANCEL_MASK, LDAP_BACK_F_CANCEL_EXOP )
#define	META_BACK_TGT_CANCEL_DISCOVER(mt)	META_BACK_TGT_ISMASK( (mt), LDAP_BACK_F_CANCEL_MASK2, LDAP_BACK_F_CANCEL_EXOP_DISCOVER )

	int			mt_version;
	time_t			mt_network_timeout;
	struct timeval		mt_bind_timeout;
#define META_BIND_TIMEOUT	LDAP_BACK_RESULT_UTIMEOUT
	time_t			mt_timeout[ LDAP_BACK_OP_LAST ];
} metatarget_t;

typedef struct metadncache_t {
	ldap_pvt_thread_mutex_t mutex;
	Avlnode			*tree;

#define META_DNCACHE_DISABLED   (0)
#define META_DNCACHE_FOREVER    ((time_t)(-1))
	time_t			ttl;  /* seconds; 0: no cache, -1: no expiry */
} metadncache_t;

typedef struct metacandidates_t {
	int			mc_ntargets;
	SlapReply		*mc_candidates;
} metacandidates_t;

/*
 * Hook to allow mucking with metainfo_t/metatarget_t when quarantine is over
 */
typedef int (*meta_back_quarantine_f)( struct metainfo_t *, int target, void * );

typedef struct metainfo_t {
	int			mi_ntargets;
	int			mi_defaulttarget;
#define META_DEFAULT_TARGET_NONE	(-1)
	int			mi_nretries;

	metatarget_t		**mi_targets;
	metacandidates_t	*mi_candidates;

	LDAP_REBIND_PROC	*mi_rebind_f;
	LDAP_URLLIST_PROC	*mi_urllist_f;

	metadncache_t		mi_cache;
	
	ldap_avl_info_t		mi_conninfo;

	/* NOTE: quarantine uses the connection mutex */
	slap_retry_info_t	mi_quarantine;

#define	META_BACK_QUARANTINE(mi)	( (mi)->mi_quarantine.ri_num != NULL )
	meta_back_quarantine_f	mi_quarantine_f;
	void			*mi_quarantine_p;

	unsigned		mi_flags;
#define	li_flags		mi_flags
/* uses flags as defined in <back-ldap/back-ldap.h> */
#define	META_BACK_F_ONERR_STOP		(0x00010000U)
#define	META_BACK_F_DEFER_ROOTDN_BIND	(0x00020000U)

#define	META_BACK_ONERR_STOP(mi)	( (mi)->mi_flags & META_BACK_F_ONERR_STOP )
#define	META_BACK_ONERR_CONTINUE(mi)	( !META_BACK_ONERR_CONTINUE( (mi) ) )

#define META_BACK_DEFER_ROOTDN_BIND(mi)	( (mi)->mi_flags & META_BACK_F_DEFER_ROOTDN_BIND )

	int			mi_version;
	time_t			mi_network_timeout;
	time_t			mi_conn_ttl;
	time_t			mi_idle_timeout;
	struct timeval		mi_bind_timeout;
	time_t			mi_timeout[ LDAP_BACK_OP_LAST ];
} metainfo_t;

typedef enum meta_op_type {
	META_OP_ALLOW_MULTIPLE = 0,
	META_OP_REQUIRE_SINGLE,
	META_OP_REQUIRE_ALL
} meta_op_type;

SlapReply *
meta_back_candidates_get( Operation *op );

extern metaconn_t *
meta_back_getconn(
	Operation		*op,
	SlapReply		*rs,
	int			*candidate,
	ldap_back_send_t	sendok );

extern void
meta_back_release_conn_lock(
       	Operation 		*op,
	metaconn_t		*mc,
	int			dolock );
#define meta_back_release_conn(op, mc)	meta_back_release_conn_lock( (op), (mc), 1 )

extern int
meta_back_retry(
	Operation		*op,
	SlapReply		*rs,
	metaconn_t		**mcp,
	int			candidate,
	ldap_back_send_t	sendok );

extern void
meta_back_conn_free(
	void			*v_mc );

extern int
meta_back_init_one_conn(
	Operation		*op,
	SlapReply		*rs,
	metaconn_t		*mc,
	int			candidate,
	int			ispriv,
	ldap_back_send_t	sendok );

extern void
meta_back_quarantine(
	Operation		*op,
	SlapReply		*rs,
	int			candidate );

extern int
meta_back_single_bind(
	Operation		*op,
	SlapReply		*rs,
	metaconn_t		*mc,
	int			candidate,
	int			massage );

extern int
meta_back_dobind(
	Operation		*op,
	SlapReply		*rs,
	metaconn_t		*mc,
	ldap_back_send_t	sendok );

extern int
meta_back_single_dobind(
	Operation		*op,
	SlapReply		*rs,
	metaconn_t		**mcp,
	int			candidate,
	ldap_back_send_t	sendok,
	int			retries,
	int			dolock );

extern int
meta_back_cancel(
	metaconn_t		*mc,
	Operation		*op,
	SlapReply		*rs,
	ber_int_t		msgid,
	int			candidate,
	ldap_back_send_t	sendok );

extern int
meta_back_op_result(
	metaconn_t		*mc,
	Operation		*op,
	SlapReply		*rs,
	int			candidate );

extern int
back_meta_LTX_init_module(
	int			argc,
	char			*argv[] );

extern int
meta_back_conn_cmp(
	const void		*c1,
	const void		*c2 );

extern int
meta_back_conndn_cmp(
	const void		*c1,
	const void		*c2 );

extern int
meta_back_conndn_dup(
	void			*c1,
	void			*c2 );

/*
 * Candidate stuff
 */
extern int
meta_back_is_candidate(
	metatarget_t		*mt,
	struct berval		*ndn,
	int			scope );

extern int
meta_back_select_unique_candidate(
	metainfo_t		*mi,
	struct berval		*ndn );

extern int
meta_clear_unused_candidates(
	Operation		*op,
	int			candidate );

extern int
meta_clear_one_candidate(
	metasingleconn_t	*mc );

extern int
meta_clear_candidates(
	Operation		*op,
	metaconn_t		*mc );

/*
 * Dn cache stuff (experimental)
 */
extern int
meta_dncache_cmp(
	const void		*c1,
	const void		*c2 );

extern int
meta_dncache_dup(
	void			*c1,
	void			*c2 );

#define META_TARGET_NONE	(-1)
#define META_TARGET_MULTIPLE	(-2)
extern int
meta_dncache_get_target(
	metadncache_t		*cache,
	struct berval		*ndn );

extern int
meta_dncache_update_entry(
	metadncache_t		*cache,
	struct berval		*ndn,
	int			target );

extern int
meta_dncache_delete_entry(
	metadncache_t		*cache,
	struct berval		*ndn );

extern void
meta_dncache_free( void *entry );

extern LDAP_REBIND_PROC		meta_back_default_rebind;
extern LDAP_URLLIST_PROC	meta_back_default_urllist;

LDAP_END_DECL

#endif /* SLAPD_META_H */

