/*
 * Copyright 1998-2003 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 *
 * Copyright 2001, Pierangelo Masarati, All rights reserved. <ando@sys-net.it>
 *
 * This work has been developed to fulfill the requirements
 * of SysNet s.n.c. <http:www.sys-net.it> and it has been donated
 * to the OpenLDAP Foundation in the hope that it may be useful
 * to the Open Source community, but WITHOUT ANY WARRANTY.
 *
 * Permission is granted to anyone to use this software for any purpose
 * on any computer system, and to alter it and redistribute it, subject
 * to the following restrictions:
 *
 * 1. The author and SysNet s.n.c. are not responsible for the consequences
 *    of use of this software, no matter how awful, even if they arise from 
 *    flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits should appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits should appear in the documentation.
 *    SysNet s.n.c. cannot be responsible for the consequences of the
 *    alterations.
 * 
 * 4. This notice may not be removed or altered.
 *
 *
 * This software is based on the backend back-ldap, implemented
 * by Howard Chu <hyc@highlandsun.com>, and modified by Mark Valence
 * <kurash@sassafras.com>, Pierangelo Masarati <ando@sys-net.it> and other
 * contributors. The contribution of the original software to the present
 * implementation is acknowledged in this copyright statement.
 *
 * A special acknowledgement goes to Howard for the overall architecture
 * (and for borrowing large pieces of code), and to Mark, who implemented
 * from scratch the attribute/objectclass mapping.
 *
 * The original copyright statement follows.
 *
 * Copyright 1999, Howard Chu, All rights reserved. <hyc@highlandsun.com>
 *
 * Permission is granted to anyone to use this software for any purpose
 * on any computer system, and to alter it and redistribute it, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits should appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits should appear in the
 *    documentation.
 *
 * 4. This notice may not be removed or altered.
 *
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "../back-ldap/back-ldap.h"
#include "back-meta.h"

int
meta_back_compare( Operation *op, SlapReply *rs )
{
	struct metainfo	*li = ( struct metainfo * )op->o_bd->be_private;
	struct metaconn *lc;
	struct metasingleconn *lsc;
	char *match = NULL, *err = NULL;
	struct berval mmatch = { 0, NULL };
	int candidates = 0, last = 0, i, count = 0, rc;
       	int cres = LDAP_SUCCESS, rres = LDAP_SUCCESS;
	int *msgid;
	dncookie dc;

	lc = meta_back_getconn( op, rs, META_OP_ALLOW_MULTIPLE,
			&op->o_req_ndn, NULL );
	if ( !lc ) {
 		send_ldap_result( op, rs );
		return -1;
	}
	
	if ( !meta_back_dobind( lc, op ) ) {
		rs->sr_err = LDAP_OTHER;
 		send_ldap_result( op, rs );
		return -1;
	}

	msgid = ch_calloc( sizeof( int ), li->ntargets );
	if ( msgid == NULL ) {
		return -1;
	}

	/*
	 * start an asynchronous compare for each candidate target
	 */
	dc.conn = op->o_conn;
	dc.rs = rs;
	dc.ctx = "compareDn";

	for ( i = 0, lsc = lc->conns; !META_LAST(lsc); ++i, ++lsc ) {
		struct berval mdn = { 0, NULL };
		struct berval mapped_attr = op->oq_compare.rs_ava->aa_desc->ad_cname;
		struct berval mapped_value = op->oq_compare.rs_ava->aa_value;

		if ( lsc->candidate != META_CANDIDATE ) {
			msgid[ i ] = -1;
			continue;
		}

		/*
		 * Rewrite the compare dn, if needed
		 */
		dc.rwmap = &li->targets[ i ]->rwmap;

		switch ( ldap_back_dn_massage( &dc, &op->o_req_dn, &mdn ) ) {
		case LDAP_UNWILLING_TO_PERFORM:
			rc = 1;
			goto finish;

		default:
			break;
		}

		/*
		 * if attr is objectClass, try to remap the value
		 */
		if ( op->oq_compare.rs_ava->aa_desc == slap_schema.si_ad_objectClass ) {
			ldap_back_map( &li->targets[ i ]->rwmap.rwm_oc,
					&op->oq_compare.rs_ava->aa_value,
					&mapped_value, BACKLDAP_MAP );

			if ( mapped_value.bv_val == NULL || mapped_value.bv_val[0] == '\0' ) {
				continue;
			}
		/*
		 * else try to remap the attribute
		 */
		} else {
			ldap_back_map( &li->targets[ i ]->rwmap.rwm_at,
				&op->oq_compare.rs_ava->aa_desc->ad_cname,
				&mapped_attr, BACKLDAP_MAP );
			if ( mapped_attr.bv_val == NULL || mapped_attr.bv_val[0] == '\0' ) {
				continue;
			}
		}
		
		/*
		 * the compare op is spawned across the targets and the first
		 * that returns determines the result; a constraint on unicity
		 * of the result ought to be enforced
		 */
		msgid[ i ] = ldap_compare( lc->conns[ i ].ld, mdn.bv_val,
				mapped_attr.bv_val, mapped_value.bv_val );
		if ( mdn.bv_val != op->o_req_dn.bv_val ) {
			free( mdn.bv_val );
			mdn.bv_val = NULL;
		}
		if ( mapped_attr.bv_val != op->oq_compare.rs_ava->aa_desc->ad_cname.bv_val ) {
			free( mapped_attr.bv_val );
		}
		if ( mapped_value.bv_val != op->oq_compare.rs_ava->aa_value.bv_val ) {
			free( mapped_value.bv_val );
		}

		if ( msgid[ i ] == -1 ) {
			continue;
		}

		++candidates;
	}

	/*
	 * wait for replies
	 */
	for ( rc = 0, count = 0; candidates > 0; ) {

		/*
		 * FIXME: should we check for abandon?
		 */
		for ( i = 0, lsc = lc->conns; !META_LAST(lsc); lsc++, i++ ) {
			int		lrc;
			LDAPMessage	*res = NULL;

			if ( msgid[ i ] == -1 ) {
				continue;
			}

			lrc = ldap_result( lsc->ld, msgid[ i ],
					0, NULL, &res );

			if ( lrc == 0 ) {
				/*
				 * FIXME: should we yield?
				 */
				if ( res ) {
					ldap_msgfree( res );
				}
				continue;

			} else if ( lrc == LDAP_RES_COMPARE ) {
				if ( count > 0 ) {
					rres = LDAP_OTHER;
					rc = -1;
					goto finish;
				}
				
				rs->sr_err = ldap_result2error( lsc->ld, res, 1 );
				switch ( rs->sr_err ) {
				case LDAP_COMPARE_TRUE:
				case LDAP_COMPARE_FALSE:

					/*
					 * true or flase, got it;
					 * sending to cache ...
					 */
					if ( li->cache.ttl != META_DNCACHE_DISABLED ) {
						( void )meta_dncache_update_entry( &li->cache, &op->o_req_ndn, i );
					}

					count++;
					rc = 0;
					break;

				default:
					rres = ldap_back_map_result( rs );

					if ( err != NULL ) {
						free( err );
					}
					ldap_get_option( lsc->ld,
						LDAP_OPT_ERROR_STRING, &err );

					if ( match != NULL ) {
						free( match );
					}
					ldap_get_option( lsc->ld,
						LDAP_OPT_MATCHED_DN, &match );
					
					last = i;
					break;
				}
				msgid[ i ] = -1;
				--candidates;

			} else {
				msgid[ i ] = -1;
				--candidates;
				if ( res ) {
					ldap_msgfree( res );
				}
				break;
			}
		}
	}

finish:;

	/*
	 * Rewrite the matched portion of the search base, if required
	 * 
	 * FIXME: only the last one gets caught!
	 */
	if ( count == 1 ) {
		if ( match != NULL ) {
			free( match );
			match = NULL;
		}
		
		/*
		 * the result of the compare is assigned to the res code
		 * that will be returned
		 */
		rres = cres;
		
		/*
		 * At least one compare failed with matched portion,
		 * and none was successful
		 */
	} else if ( match != NULL &&  match[0] != '\0' ) {
		struct berval matched;

		matched.bv_val = match;
		matched.bv_len = strlen( match );

		dc.ctx = "matchedDn";
		ldap_back_dn_massage( &dc, &matched, &mmatch );
	}

	rs->sr_err = rres;
	rs->sr_matched = mmatch.bv_val;
	send_ldap_result( op, rs );
	rs->sr_matched = NULL;

	if ( match != NULL ) {
		if ( mmatch.bv_val != match ) {
			free( mmatch.bv_val );
		}
		free( match );
	}

	if ( msgid ) {
		free( msgid );
	}
	
	return rc;
}

