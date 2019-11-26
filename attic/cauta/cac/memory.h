/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC. If not, see <https://www.gnu.org/licenses/gpl>.
 */
#ifndef CAC_MEMORY_H_
#define CAC_MEMORY_H_

/* Initializes GC-memory sub-system */
void gc_init(void);

/* Finalize GC-memory sub-system */
void gc_fini(void);

/* GC-based malloc */
void *gc_malloc(size_t sz);

/* GC-based string functions */
char *gc_strdup(const char *str);

#endif /* CAC_MEMORY_H_ */



