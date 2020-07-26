/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#ifndef VOLUTA_TYPES_H_
#define VOLUTA_TYPES_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>


/* Standard types orward declarations */
struct ucred;
struct timespec;

/* Types forward declarations */
struct voluta_list_head;
struct voluta_listq;
struct voluta_avl;
struct voluta_avl_node;
struct voluta_thread;
struct voluta_mutex;
struct voluta_qalloc;


/* Linked-list */
struct voluta_list_head {
	struct voluta_list_head *prev;
	struct voluta_list_head *next;
};


/* Sized linked-list queue */
struct voluta_listq {
	struct voluta_list_head ls;
	size_t sz;
};


/* Get key-ref of tree-node */
typedef const void *(*voluta_avl_getkey_fn)(const struct voluta_avl_node *);

/* 3-way compare function-pointer */
typedef long (*voluta_avl_keycmp_fn)(const void *, const void *);

/* Node operator */
typedef void (*voluta_avl_node_fn)(struct voluta_avl_node *, void *);

/* AVL-tree node */
struct voluta_avl_node {
	struct voluta_avl_node *parent;
	struct voluta_avl_node *left;
	struct voluta_avl_node *right;
	int balance;
	int magic;
};

/* "Iterators" range a-la STL pair */
struct voluta_avl_range {
	struct voluta_avl_node *first;
	struct voluta_avl_node *second;
};

/*
 * AVL: self-balancing binary-search-tree. Holds reference to user-provided
 * nodes (intrusive container).
 */
struct voluta_avl {
	voluta_avl_getkey_fn getkey;
	voluta_avl_keycmp_fn keycmp;
	struct voluta_avl_node head;
	size_t size;
	void *userp;
};

/* Threading */
typedef void (*voluta_execute_fn)(struct voluta_thread *);

/* Wrapper of pthread_t */
struct voluta_thread {
	pthread_t thread;
	time_t start_time;
	time_t finish_time;
	voluta_execute_fn exec;
	void *arg;
	int status;
	int pad;
};

/* Wrapper of pthread_mutex_t */
struct voluta_mutex {
	pthread_mutex_t mutex;
};

/* Network & sockets wrappers */
union voluta_sockaddr_u {
	struct sockaddr     sa;
	struct sockaddr_in  sa_in;
	struct sockaddr_in6 sa_in6;
	struct sockaddr_un  sa_un;
	struct sockaddr_storage sas;
};

struct voluta_sockaddr {
	union voluta_sockaddr_u u;
};

struct voluta_socket {
	int     fd;
	short   family;
	short   type;
	short   proto;
	short   pad_[3];
};


/* Strings & buffer */
struct voluta_str {
	const char *str;
	size_t len;
};

struct voluta_qstr {
	struct voluta_str str;
	uint64_t hash;
};

struct voluta_buf {
	void  *buf;
	size_t len;
	size_t bsz;
};

/* Pair of ino and dir-type */
struct voluta_ino_dt {
	ino_t  ino;
	mode_t dt;
	int    pad;
};

/* Hash */
struct voluta_hash256 {
	uint8_t hash[VOLUTA_HASH256_LEN];
};

struct voluta_hash512 {
	uint8_t hash[VOLUTA_HASH512_LEN];
};

/* Pass-phrase + salt buffer */
struct voluta_passphrase {
	char pass[VOLUTA_PASSPHRASE_MAX + 1];
	char salt[VOLUTA_PASSPHRASE_MAX + 1];
};

/* Quick memory allocator */
struct voluta_qastat {
	size_t memsz_data;      /* Size in bytes of data memory */
	size_t memsz_meta;      /* Size in bytes of meta memory */
	size_t npages;          /* Total number of memory-mapped pages */
	size_t npages_used;     /* Number of currently-allocated pages */
	size_t nbytes_used;     /* Current number of allocated bytes */
};

struct voluta_memref {
	void  *mem;             /* Referenced memory */
	void  *page;            /* Containing memory page */
	void  *cookie;          /* Internal object */
	size_t len;             /* Memory length */
	loff_t off;             /* Offset of memory in underlying file */
	int    fd;              /* Underlying file-descriptor */
	int    pad;
};

struct voluta_slab {
	struct voluta_list_head free_list;
	size_t sindex;
	size_t elemsz;
	size_t nfree;
	size_t nused;
};

struct voluta_qalloc {
	int memfd_data;
	int memfd_meta;
	void *mem_data;
	void *mem_meta;
	struct voluta_qastat st;
	struct voluta_list_head free_list;
	struct voluta_slab slabs[8];
	int pedantic_mode;
	int pad;
};


#endif /* VOLUTA_TYPES_H_ */


