/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <utility>
#include <algorithm>
#include <vector>
#include <infra/infra.h>
#include "randutil.h"

// Defs:
#define MAGIC               (0xbeef1)
#define MARK                '@'


struct Element {
	long magic;
	int  index;
	char mark;
	long value;
};
typedef struct Element    element_t;


typedef std::vector<Element>    ElementsVector;


// Local vars:
static size_t s_max_size = 4096;


static void check_equal(const Element &elem1, const element_t *elem2)
{
	fnx_assert(elem1.magic  == MAGIC);
	fnx_assert(elem2->magic == MAGIC);

	fnx_assert(elem1.index  == elem2->index);
	fnx_assert(elem1.magic  == elem2->magic);
	fnx_assert(elem1.value  == elem2->value);
	fnx_assert(elem1.mark   == elem2->mark);
}

static void check_equal(const ElementsVector &vector1,
                        const fnx_vector_t *vector2)
{
	void *p_elem = 0;
	const size_t sz1 = vector1.size();
	const size_t sz2 = fnx_vector_size(vector2);
	fnx_assert(sz1 == sz2);

	for (size_t i = 0; i < sz1; ++i) {
		const Element &elem1   = vector1[i];

		p_elem = fnx_vector_at(vector2, i);
		fnx_assert(p_elem != 0);
		const element_t *elem2 = static_cast<const element_t *>(p_elem);

		check_equal(elem1, elem2);
	}
}

static void test_insert_erase(void)
{
	void *p;
	int rnd;
	size_t i, sz, pos, max_sz = s_max_size;
	ElementsVector  vector1;
	fnx_vector_t vector2;

	Element elem;
	element_t *p_elem;

	fnx_vector_init(&vector2, sizeof(element_t), NULL);

	for (i = 0; i < max_sz; ++i) {
		rnd = ((i > 0) ? (rand() % int(i)) : 0);
		pos = static_cast<size_t>(rnd);

		elem.index  = static_cast<int>(i);
		elem.value  = long(pos);
		elem.mark   = MARK;
		elem.magic  = MAGIC;
		p_elem = &elem;

		vector1.insert(vector1.begin() + int(pos), elem);

		p = fnx_vector_xinsert(&vector2, pos, p_elem);
		fnx_assert(p != 0);
		p_elem = static_cast<element_t *>(p);
		fnx_assert(p_elem->index == static_cast<int>(i));

		check_equal(vector1, &vector2);
	}

	for (i = 0; i < max_sz; ++i) {
		sz  = vector1.size();
		rnd = rand() % int(sz);
		pos = static_cast<size_t>(rnd);

		vector1.erase(vector1.begin() + int(pos));
		fnx_vector_erase(&vector2, pos);

		check_equal(vector1, &vector2);
	}

	fnx_vector_destroy(&vector2);
}


// Compare Funex vector container agains C++STL std::vector.
int main(int argc, char *argv[])
{
	int rc;
	long sz;
	time_t t;

	t = time(0);
	srand(static_cast<unsigned int>(t));

	if (argc > 1) {
		rc = sscanf(argv[1], "%ld", &sz);
		if ((rc == 0) && (sz > 0)) {
			s_max_size = static_cast<size_t>(sz);
		}
	}
	test_insert_erase();

	return 0;
}


