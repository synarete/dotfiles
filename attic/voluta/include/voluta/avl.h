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
#ifndef VOLUTA_AVL_H_
#define VOLUTA_AVL_H_

#include <stdlib.h>

struct voluta_avl;
struct voluta_avl_node;



void voluta_avl_node_init(struct voluta_avl_node *x);

void voluta_avl_node_fini(struct voluta_avl_node *x);


void voluta_avl_init(struct voluta_avl *avl, void *userp,
		     voluta_avl_getkey_fn getkey, voluta_avl_keycmp_fn keycmp);

void voluta_avl_fini(struct voluta_avl *avl);

size_t voluta_avl_size(const struct voluta_avl *avl);

bool voluta_avl_isempty(const struct voluta_avl *avl);

struct voluta_avl_node *voluta_avl_begin(const struct voluta_avl *avl);

struct voluta_avl_node *voluta_avl_end(const struct voluta_avl *avl);

struct voluta_avl_node *
voluta_avl_next(const struct voluta_avl *avl, const struct voluta_avl_node *x);

struct voluta_avl_node *
voluta_avl_prev(const struct voluta_avl *avl, const struct voluta_avl_node *x);


struct voluta_avl_node *
voluta_avl_find(const struct voluta_avl *avl, const void *k);

struct voluta_avl_node *
voluta_avl_find_first(const struct voluta_avl *avl, const void *k);

size_t voluta_avl_count(const struct voluta_avl *avl, const void *k);



struct voluta_avl_node *
voluta_avl_lower_bound(const struct voluta_avl *avl, const void *k);

struct voluta_avl_node *
voluta_avl_upper_bound(const struct voluta_avl *avl, const void *k);


void voluta_avl_equal_range(const struct voluta_avl *avl, const void *k,
			    struct voluta_avl_range *out_r);

void voluta_avl_insert(struct voluta_avl *avl, struct voluta_avl_node *z);

int voluta_avl_insert_unique(struct voluta_avl *avl,
			     struct voluta_avl_node *z);

struct voluta_avl_node *
voluta_avl_insert_replace(struct voluta_avl *avl, struct voluta_avl_node *z);

void voluta_avl_remove(struct voluta_avl *avl, struct voluta_avl_node *x);

void voluta_avl_remove_range(struct voluta_avl *avl,
			     struct voluta_avl_node *first,
			     struct voluta_avl_node *last);

void voluta_avl_clear(struct voluta_avl *avl, voluta_avl_node_fn fn, void *p);



#endif /* VOLUTA_AVL_H_ */




