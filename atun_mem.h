
/*
 * File:   atun_mem.h
 * Author: 19020107
 *
 * Created on April 11, 2018
 */

#ifndef ATUN_MEMORY_H
#define ATUN_MEMORY_H

#include "atun_sys.h"

void atun_mem_init();
void *atun_alloc(size_t size);
void atun_alloc_free(void *p);
void atun_free_large();
void atun_mem_free();

#endif
