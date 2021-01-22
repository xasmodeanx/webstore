/*
	webstore is a web-based arbitrary data storage service that accepts z85 encoded data 
	Copyright (C) 2021 Brett Kuskie <fullaxx@gmail.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webstore.h"
#include "webstore_ops.h"
#include "futils.h"

sri_t *g_srv = NULL;
wsrt_t g_rt;

//dedicated redis handle for checking IP addresses
//node callbacks will not use this handle
// TEST THIS LATER rai_t raddrh;
	
#ifdef SRNODECHRONOMETRY
void print_avg_nodecb_time(void)
{
	long avg;

	avg = searest_node_get_avg_duration(ws, "/store/md5/");
	if(avg > 0) printf("%6s: %ld\n", "md5", avg);

	avg = searest_node_get_avg_duration(ws, "/store/sha1/");
	if(avg > 0) printf("%6s: %ld\n", "sha1", avg);

	avg = searest_node_get_avg_duration(ws, "/store/sha224/");
	if(avg > 0) printf("%6s: %ld\n", "sha224", avg);

	avg = searest_node_get_avg_duration(ws, "/store/sha256/");
	if(avg > 0) printf("%6s: %ld\n", "sha256", avg);

	avg = searest_node_get_avg_duration(ws, "/store/sha384/");
	if(avg > 0) printf("%6s: %ld\n", "sha384", avg);

	avg = searest_node_get_avg_duration(ws, "/store/sha512/");
	if(avg > 0) printf("%6s: %ld\n", "sha512", avg);
}
#endif

static inline void save_ip(rai_t *rc, char *ip, int found)
{
	redisReply *reply;
	if(found == 0) {
		reply = redisCommand(rc->c, "SET IPS:%s 1 EX %d", ip, REQPERIOD);
	} else {
		reply = redisCommand(rc->c, "INCR IPS:%s", ip);
	}
	freeReplyObject(reply);
}

static inline int check_ip(rai_t *rc, char *ip)
{
	int count, retval = 1;
	redisReply *reply;

	reply = redisCommand(rc->c, "GET IPS:%s", ip);
	if(reply->type == REDIS_REPLY_NIL) {
		// KEY DOES NOT EXIST
#ifdef DEBUG
		printf("%s: 1\n", ip);
#endif
		save_ip(rc, ip, 0);
	} else if(reply->type == REDIS_REPLY_STRING) {
		count = atoi(reply->str);
		if(count < REQCOUNT) {
#ifdef DEBUG
			printf("%s: %d (allowed)\n", ip, count+1);
#endif
			save_ip(rc, ip, 1);
		} else {
#ifdef DEBUG
			printf("%s: %d (DENIED)\n", ip, count+1);
#endif
			retval = 0;
		}
	} else { retval = 0; }	// THIS SHOULD NEVER HAPPEN

	freeReplyObject(reply);
	return retval;
}

static int allow_ip(wsrt_t *lrt, char *ip)
{
	int retval;
	rai_t *rc = &lrt->rc;

	//LOCK RAI if redis calls are in their own thread, using a shared context
	if(lrt->multithreaded) { rai_lock(rc); }
	retval = check_ip(rc, ip);
	if(lrt->multithreaded) { rai_unlock(rc); }

	return retval;
}

// return SR_IP_DENY to DENY a new incoming connection based on IP address
// return SR_IP_ACCEPT to ACCEPT a new incoming connection based on IP address
static int ws_addr_check(char *inc_ip, void *user_data)
{
	int z;
	wsrt_t *lrt = (wsrt_t *)user_data;

	// Blindly accept w/o logging localhost
	if(strcmp(inc_ip, "127.0.0.1") == 0) { return SR_IP_ACCEPT; }

	z = allow_ip(lrt, inc_ip);
	if(z == 0) { return SR_IP_DENY; }

	return SR_IP_ACCEPT;
}

static void activate_https(char *certfile, char *keyfile)
{
	char *cert, *key;

	cert = get_file(certfile);
	if(!cert) { exit(EXIT_FAILURE); }
	key = get_file(keyfile);
	if(!key) { exit(EXIT_FAILURE); }
	searest_set_https_cert(g_srv, cert);
	searest_set_https_key(g_srv, key);
	free(cert);
	free(key);
}

void webstore_start(char *http_ip, unsigned short http_port, int use_threads, char *rdest, unsigned short rport, char *certfile, char *keyfile)
{
	int z;

	memset(&g_rt, 0, sizeof(wsrt_t));
	//memset(&raddrh, 0, sizeof(raddrh));
	g_rt.multithreaded = use_threads;
	z = rai_connect(&g_rt.rc, rdest, rport);
	if(z) {
		if(rport) fprintf(stderr, "Failed to connect to %s:%u!\n", rdest, rport);
		else fprintf(stderr, "Failed to connect to %s!\n", rdest);
		exit(EXIT_FAILURE);
	}
	/*z = rai_connect(&raddrh, redis_sock, 0);
	if(z) {
		fprintf(stderr, "Couldnt get a redis handle!\n");
		exit(EXIT_FAILURE);
	}*/

	g_srv = searest_new(32+11, 128+14, MAXENCMSGSIZ);
	searest_node_add(g_srv, "/store/md5/",		&md5_node,    NULL);
	searest_node_add(g_srv, "/store/sha1/",		&sha1_node,   NULL);
	searest_node_add(g_srv, "/store/sha224/",	&sha224_node, NULL);
	searest_node_add(g_srv, "/store/sha256/",	&sha256_node, NULL);
	searest_node_add(g_srv, "/store/sha384/",	&sha384_node, NULL);
	searest_node_add(g_srv, "/store/sha512/",	&sha512_node, NULL);

	if(certfile && keyfile) { activate_https(certfile, keyfile); }

	searest_set_addr_cb(g_srv, &ws_addr_check);
	if(use_threads == 0) searest_set_internal_select(g_srv);
	z = searest_start(g_srv, http_ip, http_port, &g_rt);
	if(z) {
		if(!http_ip) { http_ip="*"; }
		fprintf(stderr, "searest_start() failed to bind to %s:%u!\n", http_ip, http_port);
		exit(EXIT_FAILURE);
	}
}

void webstore_stop(void)
{
	if(g_srv) {
		searest_stop(g_srv);
		searest_del(g_srv);
		//rai_disconnect(&raddrh);
		rai_disconnect(&g_rt.rc);
	}
}