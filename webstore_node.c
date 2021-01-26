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
//#include <unistd.h>
#include <ctype.h>

#include "webstore.h"
#include "webstore_ops.h"
#include "webstore_log.h"

static const char* base85 =
{
   "0123456789"
   "abcdefghij"
   "klmnopqrst"
   "uvwxyzABCD"
   "EFGHIJKLMN"
   "OPQRSTUVWX"
   "YZ.-:+=^!/"
   "*?&<>()[]{"
   "}@%$#"
};

// returns TRUE if the incoming digit is a valid z85 digit
static int is_Z85_digit(char digit)
{
	int i;
	if(isalnum(digit)) return 1;	//TRUE
	for(i=62; i<85; i++) {
		if(digit == base85[i]) return 1;	//TRUE
	}
	return 0;	//FALSE
}

// return FALSE if any character in ptr is not a valid z85 digit
static int Z85_validate(const unsigned char *ptr, size_t len)
{
	size_t i;
	for(i=0; i<len; i++) {
		if(!is_Z85_digit(ptr[i])) return 1;
	}
	return 0;	// did not validate properly
}

// Validate proper hex digits in URL and convert to all lowercase
// return strdup(newhash) if valid
// return NULL if NOT VALID
// this must be free()'d
static char* convert_hash(char *input, int len)
{
	int i,j;
	char newhash[256];

	if(len > sizeof(newhash)-1) {
		fprintf(stderr, "SOMETHING WENT HORRIBLY WRONG!\n");
		exit(1);
	}

	memset(&newhash[0], 0, sizeof(newhash));
	for(i=0,j=0; i<len; i++,j++) {
		if(!isxdigit(input[i])) { return NULL; }
		newhash[j] = tolower(input[i]);
	}
	newhash[len] = 0;

	return strdup(newhash);
}

static char* get(wsreq_t *req, wsrt_t *rt, srci_t *ri)
{
	int err = 0;
	char *hash;
	char *page = NULL;
	redisReply *reply;
	rai_t *rc = &rt->rc;

	// Check the URL length
	if(req->urllen != req->hashlen) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request");
	}

	hash = convert_hash(req->url, req->urllen);
	if(!hash) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request");
	}

	//LOCK RAI if redis calls are in their own thread, using a shared context
	if(rt->multithreaded) { rai_lock(rc); }
	reply = redisCommand(rc->c, "GET %s", hash);
	if(!reply) {
		handle_redis_error(rc);
		err = 503;
	} else {
		if(reply->type == REDIS_REPLY_STRING) { page = strdup(reply->str); }
		freeReplyObject(reply);
	}
	if(rt->multithreaded) { rai_unlock(rc); }
	free(hash);

	if(err == 503) {
		srci_set_return_code(ri, MHD_HTTP_SERVICE_UNAVAILABLE);
		return strdup("service unavailable");
	}

	if(!page) {
		srci_set_return_code(ri, MHD_HTTP_NOT_FOUND);
		log_add(WSLOG_INFO, "%s %d GET %s", srci_get_client_ip(ri), MHD_HTTP_NOT_FOUND, req->url);
		return strdup("not found");
	}

	srci_set_return_code(ri, MHD_HTTP_OK);
	log_add(WSLOG_INFO, "%s %d GET %s", srci_get_client_ip(ri), MHD_HTTP_OK, req->url);
	return page;
}

static int do_redis_post(wsrt_t *rt, const char *hash, const unsigned char *dataptr, size_t datalen)
{
	int err = 500;
	char *datastr;
	redisReply *reply;
	rai_t *rc = &rt->rc;

	//Turn our uploaded data into a string with a finalizing NULL
	datastr = calloc(1, datalen+1);
	memcpy(datastr, dataptr, datalen);

	//LOCK RAI if redis calls are in their own thread, using a shared context
	if(rt->multithreaded) { rai_lock(rc); }
	if(rt->expiration > 0) {
		reply = redisCommand(rc->c, "SET %s %s EX %ld", hash, datastr, rt->expiration);
	} else {
		reply = redisCommand(rc->c, "SET %s %s", hash, datastr);
	}
	if(!reply) {
		handle_redis_error(rc);
		err = 503;
	} else {
		if(reply->type == REDIS_REPLY_STATUS) {
			if(strncmp("OK", reply->str, 2) == 0) { err = 0; }
		}
		freeReplyObject(reply);
	}
	if(rt->multithreaded) { rai_unlock(rc); }

	free(datastr);
	return err;
}

static char* post(wsreq_t *req, wsrt_t *rt, srci_t *ri)
{
	int z;
	const unsigned char *dataptr;
	size_t datalen;
	char *hash;

	// Check the URL length
	if(req->urllen != req->hashlen) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request - bad url");
	}

	// Check the length of uploaded data
	datalen = srci_get_post_data_size(ri);
	if(datalen < 5) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request - invalid length");
	}

	// Validate the uploaded data
	dataptr = srci_get_post_data_ptr(ri);
	z = Z85_validate(dataptr, datalen);
	if(z) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request - data did not validate");
	}

#ifdef DEBUG
	//printf("%lu) %s\n", datalen, dataptr);
#endif

	hash = convert_hash(req->url, req->urllen);
	if(!hash) {
		srci_set_return_code(ri, MHD_HTTP_BAD_REQUEST);
		return strdup("malformed request - bad url");
	}

	z = do_redis_post(rt, hash, dataptr, datalen);
	free(hash);
	if(z) {
		srci_set_return_code(ri, z);
		if(z == 503) { return strdup("service unavailable"); }
		else { return strdup("internal server error"); }
	}

	srci_set_return_code(ri, MHD_HTTP_OK);
	log_add(WSLOG_INFO, "%s %d POST %s", srci_get_client_ip(ri), MHD_HTTP_OK, req->url);
	return strdup("ok");
}

static inline char* shutdownmsg(srci_t *ri)
{
	srci_set_return_code(ri, MHD_HTTP_SERVICE_UNAVAILABLE);
	return strdup("service unavailable: shutting down");
}

char* node128(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG128;
	req.hashlen = HASHLEN128;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}

char* node160(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG160;
	req.hashlen = HASHLEN160;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}

char* node224(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG224;
	req.hashlen = HASHLEN224;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}

char* node256(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG256;
	req.hashlen = HASHLEN256;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}

char* node384(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG384;
	req.hashlen = HASHLEN384;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}

char* node512(char *url, int urllen, srci_t *ri, void *sri_user_data, void *node_user_data)
{
	wsreq_t req;
	wsrt_t *rt;
	char *page = NULL;

	if(shutting_down()) { return shutdownmsg(ri); }

	req.type = HASHALG512;
	req.hashlen = HASHLEN512;
	req.url = url;
	req.urllen = urllen;
	rt = (wsrt_t *)sri_user_data;

	switch(METHOD(ri)) {
		case METHOD_GET:
			page = get(&req, rt, ri);
			break;
		case METHOD_POST:
			page = post(&req, rt, ri);
			break;
		/*case METHOD_DEL:
			break;*/
		default:
			srci_set_return_code(ri, MHD_HTTP_METHOD_NOT_ALLOWED);
			log_add(WSLOG_WARN, "%s %d METHOD_NOT_ALLOWED", srci_get_client_ip(ri), ri->return_code);
			page = strdup("method not allowed");
			break;
	}

	return page;
}
