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

#ifndef __WEBSTORE_H__
#define __WEBSTORE_H__

#define HASHLEN128 ((128/8)*2)
#define HASHLEN160 ((160/8)*2)
#define HASHLEN224 ((224/8)*2)
#define HASHLEN256 ((256/8)*2)
#define HASHLEN384 ((384/8)*2)
#define HASHLEN512 ((512/8)*2)

#define HASHALG128 (1)
#define HASHALG160 (2)
#define HASHALG224 (3)
#define HASHALG256 (4)
#define HASHALG384 (5)
#define HASHALG512 (6)

#endif
