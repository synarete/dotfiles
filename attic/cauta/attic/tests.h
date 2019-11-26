/*
 * This file is part of CaC
 *
 * Copyright (C) 2016,2017 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CAC_TESTS_H_
#define CAC_TESTS_H_

#include <stdlib.h>
#include <string.h>

#include <cac/cacdefs.h>
#include <cac/infra.h>
#include <cac/strings.h>
#include <cac/hash.h>

void cac_test_types(void);
void cac_test_hash(void);
void cac_test_substr(void);
void cac_test_supstr(void);
void cac_test_strings(void);
void cac_test_wsubstr(void);
void cac_test_wsupstr(void);
void cac_test_wstrings(void);
void cac_test_htbl(void);
void cac_test_vector(void);

#endif /* CAC_TESTS_H_ */


