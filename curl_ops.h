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

#ifndef __WEBSTORE_CURL_OPERATIONS_H__
#define __WEBSTORE_CURL_OPERATIONS_H__

typedef struct {
	long bytecount;
	char *page;	// free() this
	long http_code;
} curlresp_t;

typedef struct {
	unsigned char *data;
	long size;
} curlpost_t;

int ws_curl_get(char *, curlresp_t *);
int ws_curl_post(char *, curlresp_t *, curlpost_t *);

#endif
