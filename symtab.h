/*
 * This file is part of sp-smaps
 *
 * Copyright (C) 2004-2007 Nokia Corporation.
 *
 * Contact: Eero Tamminen <eero.tamminen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/* ========================================================================= *
 * File: symtab.h
 *
 * Author: Simo Piiroinen
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 18-Jan-2007 Simo Piiroinen
 * - some comments added
 *
 * 2006-Nov-09 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef SYMTAB_H_
#define SYMTAB_H_

//#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

/* ========================================================================= *
 * Custom Object Classes
 * ========================================================================= */

typedef struct symtab_t symtab_t;
typedef struct symbol_t symbol_t;

/* ------------------------------------------------------------------------- *
 * symbol_t
 * ------------------------------------------------------------------------- */

struct symbol_t
{
  char *symbol_key;     // symbol name
  int   symbol_val;     // symbol value
};

void      symbol_ctor     (symbol_t *self, const char *key, int val);
void      symbol_dtor     (symbol_t *self);
symbol_t *symbol_create   (const char *key, int val);
void      symbol_delete   (symbol_t *self);
void      symbol_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * symtab_t
 * ------------------------------------------------------------------------- */

struct symtab_t
{
  symbol_t *symtab_entry; // symbol array
  size_t    symtab_count; // num used
  size_t    symtab_alloc; // allocated for
};

void      symtab_ctor     (symtab_t *self);
void      symtab_dtor     (symtab_t *self);
symtab_t *symtab_create   (void);
void      symtab_delete   (symtab_t *self);
void      symtab_delete_cb(void *self);
int       symtab_get      (symtab_t *self, const char *str, int def);
void      symtab_set      (symtab_t *self, const char *str, int val);
int       symtab_enumerate(symtab_t *self, const char *str);
void      symtab_emit(symtab_t *self, FILE *file);
void      symtab_renum(symtab_t *self);

#ifdef __cplusplus
};
#endif

#endif /* SYMTAB_H_ */
