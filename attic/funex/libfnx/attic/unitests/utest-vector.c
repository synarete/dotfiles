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
#include <fnx/infra.h>

#define MAGIC   ~(0xdead)
#define MARK    'V'
#define COUNT   256


struct fnx_packed element {
	long magic;
	int  index;
	char mark;
};
typedef struct fnx_packed element element_t;

static void
check_equal_elems(const element_t *e1, const element_t *e2)
{
	int cmp;

	cmp = memcmp(e1, e2, sizeof(*e1));
	fnx_assert(cmp == 0);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_push_pop(void)
{
	int rc;
	size_t i, sz, count = COUNT;
	fnx_vector_t vec;
	element_t elem;
	element_t *p_elem = NULL;

	fnx_vector_init(&vec, sizeof(elem), NULL);
	rc = fnx_vector_isempty(&vec);
	fnx_assert(rc);

	for (i = 0; i < count; ++i) {
		elem.magic  = MAGIC;
		elem.index  = (int) i;
		elem.mark   = MARK;

		p_elem = (element_t *) fnx_vector_xpush_back(&vec, &elem);
		fnx_assert(p_elem != NULL);

		check_equal_elems(p_elem, &elem);

		p_elem = (element_t *) fnx_vector_at(&vec, i);
		check_equal_elems(p_elem, &elem);
	}

	for (i = 0; i < count; ++i) {
		p_elem = (element_t *) fnx_vector_at(&vec, i);
		fnx_assert(p_elem != NULL);
		fnx_assert(p_elem->magic == MAGIC);
		fnx_assert(p_elem->index == (int) i);
		fnx_assert(p_elem->mark  == MARK);
	}

	for (i = 0; i < count; ++i) {
		fnx_vector_pop_back(&vec);
		sz = fnx_vector_size(&vec);
		fnx_assert(sz == (size_t)(count - i - 1));
	}


	fnx_vector_destroy(&vec);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_insert(void)
{
	int rc;
	size_t i, count = COUNT;
	fnx_vector_t vec;
	element_t elem;
	element_t *p_elem = NULL;

	fnx_vector_init(&vec, sizeof(elem), NULL);
	rc = fnx_vector_isempty(&vec);
	fnx_assert(rc);

	for (i = 0; i < count; ++i) {
		elem.magic  = MAGIC;
		elem.index  = (int)i;
		elem.mark   = MARK;

		p_elem = (element_t *)fnx_vector_xinsert(&vec, 0, &elem);
		fnx_assert(p_elem != NULL);
		fnx_assert(p_elem->mark == MARK);
		fnx_assert(p_elem->index == (int)i);

		check_equal_elems(p_elem, &elem);

		p_elem = (element_t *)fnx_vector_at(&vec, 0);
		fnx_assert(p_elem != NULL);
		check_equal_elems(p_elem, &elem);
	}

	for (i = count; i > 0; --i) {
		p_elem = (element_t *) fnx_vector_at(&vec, i - 1);
		fnx_assert(p_elem != NULL);
		fnx_assert(p_elem->magic == MAGIC);
		fnx_assert(p_elem->index == (int)(count - i));
		fnx_assert(p_elem->mark  == MARK);
	}

	fnx_vector_destroy(&vec);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
static void test_erase(void)
{
	size_t i, sz, pos, count = COUNT;
	fnx_vector_t vec;
	element_t elem;
	element_t *p_elem = NULL;

	fnx_vector_init(&vec, sizeof(elem), NULL);

	for (i = 0; i < count; ++i) {
		elem.magic  = MAGIC;
		elem.index  = (int) i;
		elem.mark   = MARK;

		p_elem = (element_t *) fnx_vector_xinsert(&vec, 0, &elem);
		fnx_assert(p_elem != NULL);
		fnx_assert(p_elem->magic == MAGIC);
	}

	for (i = 0; i < count; ++i) {
		sz = fnx_vector_size(&vec);
		fnx_assert(sz > 0);
		pos = sz / 2;
		fnx_vector_erase(&vec, pos);
	}
	sz = fnx_vector_size(&vec);
	fnx_assert(sz == 0);

	fnx_vector_destroy(&vec);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
int main(void)
{
	test_push_pop();
	test_insert();
	test_erase();

	return EXIT_SUCCESS;
}


