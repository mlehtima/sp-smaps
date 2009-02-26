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
 * File: symtab.c
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

#include <string.h>
#include <stdlib.h>

#include "symtab.h"

/* ========================================================================= *
 * symbol_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * symbol_ctor  --  constructor
 * ------------------------------------------------------------------------- */

void
symbol_ctor(symbol_t *self, const char *key, int val)
{
  self->symbol_key = strdup(key);
  self->symbol_val = val;
}

/* ------------------------------------------------------------------------- *
 * symbol_dtor  --  destructor
 * ------------------------------------------------------------------------- */

void
symbol_dtor(symbol_t *self)
{
  free(self->symbol_key);
}

/* ------------------------------------------------------------------------- *
 * symbol_create  --  create method
 * ------------------------------------------------------------------------- */

symbol_t *
symbol_create(const char *key, int val)
{
  symbol_t *self = calloc(1, sizeof *self);
  symbol_ctor(self, key, val);
  return self;
}

/* ------------------------------------------------------------------------- *
 * symbol_delete  --  delete method
 * ------------------------------------------------------------------------- */

void
symbol_delete(symbol_t *self)
{
  if( self != 0 )
  {
    symbol_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * symbol_delete_cb  --  delete callback for containers
 * ------------------------------------------------------------------------- */

void
symbol_delete_cb(void *self)
{
  symbol_delete(self);
}

/* ========================================================================= *
 * symtab_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * symtab_ctor
 * ------------------------------------------------------------------------- */

void
symtab_ctor(symtab_t *self)
{
  self->symtab_count = 0;
  self->symtab_alloc = 256;
  self->symtab_entry = malloc(self->symtab_alloc *
                              sizeof *self->symtab_entry);
}

/* ------------------------------------------------------------------------- *
 * symtab_dtor
 * ------------------------------------------------------------------------- */

void
symtab_dtor(symtab_t *self)
{
  for( size_t i = 0; i < self->symtab_count; ++i )
  {
    symbol_dtor(&self->symtab_entry[i]);
  }
  free(self->symtab_entry);
}

/* ------------------------------------------------------------------------- *
 * symtab_create
 * ------------------------------------------------------------------------- */

symtab_t *
symtab_create(void)
{
  symtab_t *self = calloc(1, sizeof *self);
  symtab_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * symtab_delete
 * ------------------------------------------------------------------------- */

void
symtab_delete(symtab_t *self)
{
  if( self != 0 )
  {
    symtab_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * symtab_delete_cb  --  delete callback for containers
 * ------------------------------------------------------------------------- */

void
symtab_delete_cb(void *self)
{
  symtab_delete(self);
}

/* ------------------------------------------------------------------------- *
 * symtab_get  --  get symbol value by key or default if not defined
 * ------------------------------------------------------------------------- */

int
symtab_get(symtab_t *self, const char *str, int def)
{
  size_t lo = 0;
  size_t hi = self->symtab_count;

  while( lo < hi )
  {
    size_t i = (lo + hi)/2;
    symbol_t *s = &self->symtab_entry[i];
    int r = strcmp(s->symbol_key, str);
    if( r < 0 ) { lo = i + 1; continue; }
    if( r > 0 ) { hi = i + 0; continue; }
    return s->symbol_val;
  }
  return def;
}

/* ------------------------------------------------------------------------- *
 * symtab_set  --  assign value for symbol
 * ------------------------------------------------------------------------- */

void
symtab_set(symtab_t *self, const char *str, int val)
{
  size_t lo = 0;
  size_t hi = self->symtab_count;
  symbol_t *s = 0;

  for( ;; )
  {
    if( lo == hi )
    {
      if( self->symtab_count == self->symtab_alloc )
      {
        self->symtab_alloc *= 2;
        self->symtab_entry = realloc(self->symtab_entry,
                                      self->symtab_alloc *
                                      sizeof *self->symtab_entry);
      }

      s = &self->symtab_entry[lo];
      memmove(s+1, s+0, (self->symtab_count - lo) * sizeof *s);
      self->symtab_count += 1;
      symbol_ctor(s, str, val);
      break;
    }
    else
    {
      size_t i = (lo + hi)/2;
      s = &self->symtab_entry[i];
      int r = strcmp(s->symbol_key, str);
      if( r < 0 ) { lo = i + 1; continue; }
      if( r > 0 ) { hi = i + 0; continue; }
      s->symbol_val = val;
      break;
    }
  }
}

/* ------------------------------------------------------------------------- *
 * symtab_enumerate  --  new keys will get enumerated values
 * ------------------------------------------------------------------------- */

int
symtab_enumerate(symtab_t *self, const char *str)
{
  size_t lo = 0;
  size_t hi = self->symtab_count;
  symbol_t *s = 0;

  for( ;; )
  {
    if( lo == hi )
    {
      if( self->symtab_count == self->symtab_alloc )
      {
        self->symtab_alloc *= 2;
        self->symtab_entry = realloc(self->symtab_entry,
                                      self->symtab_alloc *
                                      sizeof *self->symtab_entry);
      }
      s = &self->symtab_entry[lo];
      memmove(s+1, s+0, (self->symtab_count - lo) * sizeof *s);
      symbol_ctor(s, str, (int)self->symtab_count++);
      break;
    }
    else
    {
      size_t i = (lo + hi)/2;
      s = &self->symtab_entry[i];
      int r = strcmp(s->symbol_key, str);
      if( r < 0 ) { lo = i + 1; continue; }
      if( r > 0 ) { hi = i + 0; continue; }
      break;
    }
  }
  return s->symbol_val;
}

/* ------------------------------------------------------------------------- *
 * symtab_emit  --  debugging output
 * ------------------------------------------------------------------------- */

void symtab_emit(symtab_t *self, FILE *file)
{
  for( int i = 0; i < self->symtab_count; ++i )
  {
    symbol_t *s = &self->symtab_entry[i];
    //fprintf(file, "[%03d] '%s' = %d\n", i, s->symbol_key, s->symbol_val);
    fprintf(file, "%03d %s\n", s->symbol_val, s->symbol_key);
  }
}

/* ------------------------------------------------------------------------- *
 * symtab_renum  -- renumerate symbol values 0 ... count-1
 * ------------------------------------------------------------------------- */

void symtab_renum(symtab_t *self)
{
  for( int i = 0; i < self->symtab_count; ++i )
  {
    symbol_t *s = &self->symtab_entry[i];
    s->symbol_val = i;
  }
}
