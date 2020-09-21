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
 * File: sp_smaps_filter.c
 *
 * Author: Simo Piiroinen
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 25-Jun-2009
 * - Ignore & give a warning for smaps entries that do not match
 *   "==> /proc/[0-9]+/smaps <==", instead of dying in assert. This fixes cases
 *   where we have "==> /proc/self/smaps <==".
 * - Dont try to remove threads from old style capture files.
 *
 * 25-Feb-2009 Simo Piiroinen
 * - lists unrecognized keys without exiting
 *
 * 18-Jan-2007 Simo Piiroinen
 * - fixed usage output
 * - code cleanup
 *
 * 17-Oct-2006 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libsysperf/csv_table.h>
#include <libsysperf/array.h>

#include <libsysperf/msg.h>
#include <libsysperf/argvec.h>
#include <libsysperf/str_array.h>

#include "symtab.h"

#if 0
# define INLINE static inline
# define STATIC static
#else
# define INLINE static
# define STATIC static
#endif

#define TOOL_NAME "sp_smaps_filter"
#include "release.h"

#define HTML_ELLIPSIS   "&#0133;"
#define TITLE_MAX_LEN   60

/* ------------------------------------------------------------------------- *
 * Runtime Manual
 * ------------------------------------------------------------------------- */

static const manual_t app_man[]=
{
  MAN_ADD("NAME",
          TOOL_NAME"  --  smaps capture file analysis tool\n"
          )
  MAN_ADD("SYNOPSIS",
          ""TOOL_NAME" [options] <capture file> ... \n"
          )
  MAN_ADD("DESCRIPTION",
          "This tool is used for processing capture files.\n"
          "The following processing modes are available:\n"
          "\n"
          "flatten:\n"
          "  heuristically detect and remove threads\n"
          "  input  - capture file\n"
          "  output - capture file\n"
          "\n"
          "normalize:\n"
          "  thread removal and conversion to csv format\n"
          "  input  - capture file\n"
          "  output - csv file\n"
          "\n"
          "appvals:\n"
          "  thread removal and output main per application values\n"
          "  input  - capture file\n"
          "  output - csv file\n"
          "\n"
          "analyze:\n"
          "  thread removal and conversion to html format\n"
          "  input  - capture file\n"
          "  output - html index + sub pages in separate dir\n"
          "\n"
          "diff:\n"
          "  thread removal and comparison of memory usage values\n"
          "  input  - capture files\n"
          "  output - csv or html file\n"
          )
  MAN_ADD("OPTIONS", 0)

  MAN_ADD("EXAMPLES",
          "% "TOOL_NAME" -m flatten *.cap\n"
          "  writes capture format output without threads -> *.flat\n"
          "\n"
          "% "TOOL_NAME" -m normalize *.cap\n"
          "  writes csv format output -> *.csv\n"
          "\n"
          "% "TOOL_NAME" -m appcals *.cap\n"
          "  writes csv format summary -> *.apps\n"
          "\n"
          "% "TOOL_NAME" -m analyze *.cap\n"
          "  writes browsable html analysis index -> *.html\n"
          "\n"
          "% "TOOL_NAME" -m diff *.cap -o diff.sys.csv\n"
          "  difference report in csv minimum details\n"
          "\n"
          "% "TOOL_NAME" -m diff *.cap -o diff.obj.csv\n"
          "  difference report in csv maximum details\n"
          "\n"
          "% "TOOL_NAME" -m diff *.cap -o diff.pid.html -tapp\n"
          "  difference report in html details to pid.level\n"
          "                            appcolumn output trimmed\n"
          )

  MAN_ADD("NOTES",
          "The filtering mode defaults to analyze, unless the program\n"
          "is invoked via symlink in which case the mode determined\n"
          "after the last underscore in invocation name, i.e.\n"
          "  % ln -s sp_smaps_filter sp_smaps_diff\n"
          "  % sp_smaps_diff ...\n"
          "is equal to\n"
          "  % sp_smaps_filter -mdiff ...\n"
          "")

  MAN_ADD("COPYRIGHT",
          "Copyright (C) 2004-2007 Nokia Corporation.\n\n"
          "This is free software.  You may redistribute copies of it under the\n"
          "terms of the GNU General Public License v2 included with the software.\n"
          "There is NO WARRANTY, to the extent permitted by law.\n"
          )
  MAN_ADD("SEE ALSO",
          "sp_smaps_snapshot (1)\n"
          )
  MAN_END
};
/* ------------------------------------------------------------------------- *
 * Commandline Arguments
 * ------------------------------------------------------------------------- */

enum
{
  opt_noswitch = -1,
  opt_help,
  opt_vers,

  opt_verbose,
  opt_quiet,
  opt_silent,

  opt_input,
  opt_output,

  opt_filtmode,

  opt_difflevel,
  opt_trimlevel,

};
static const option_t app_opt[] =
{
  /* - - - - - - - - - - - - - - - - - - - *
   * Standard usage, version & verbosity
   * - - - - - - - - - - - - - - - - - - - */

  OPT_ADD(opt_help,
          "h", "help", 0,
          "This help text\n"),

  OPT_ADD(opt_vers,
          "V", "version", 0,
          "Tool version\n"),

  OPT_ADD(opt_verbose,
          "v", "verbose", 0,
          "Enable diagnostic messages\n"),

  OPT_ADD(opt_quiet,
          "q", "quiet", 0,
          "Disable warning messages\n"),

  OPT_ADD(opt_silent,
          "s", "silent", 0,
          "Disable all messages\n"),

  /* - - - - - - - - - - - - - - - - - - - *
   * Application options
   * - - - - - - - - - - - - - - - - - - - */

  OPT_ADD(opt_input,
          "f", "input", "<source path>",
          "Add capture file for processing.\n" ),

  OPT_ADD(opt_output,
          "o", "output", "<destination path>",
          "Override default output path.\n" ),

  OPT_ADD(opt_filtmode,
          "m", "mode", "<filter mode>",
          "One of:\n"
          "  flatten\n"
          "  normalize\n"
          "  analyze\n"
          "  appvals\n"
          "  diff\n"),

  /* - - - - - - - - - - - - - - - - - - - *
   * diff options
   * - - - - - - - - - - - - - - - - - - - */

  OPT_ADD(opt_difflevel,
          "l", "difflevel", "<level>",
          "Display results at level:\n"
          "  0 = capture\n"
          "  1 = command\n"
          "  2 = command, pid\n"
          "  3 = command, pid, type\n"
          "  4 = command, pid, type, path\n"),

  OPT_ADD(opt_trimlevel,
          "t", "trimlevel", "<level>",
          "Omit repeating classification data:\n"
          "  1 = command\n"
          "  2 = command, pid\n"
          "  3 = command, pid, type\n"
          "  4 = command, pid, type, path\n"),

  /* - - - - - - - - - - - - - - - - - - - *
   * Sentinel
   * - - - - - - - - - - - - - - - - - - - */

  OPT_END
};

#include <argz.h>

static const char *abbr_title(const char *title)
{
  static char buf[512];
  size_t tlen = strlen(title);
  if( tlen < TITLE_MAX_LEN )
  {
    return title;
  }
  else
  {
    snprintf(buf, sizeof(buf), "<abbr title=\"%s\">%s%s</abbr>",
        title, HTML_ELLIPSIS, &title[tlen-TITLE_MAX_LEN]);
    buf[sizeof(buf)-1] = '\0';
    return buf;
  }
}

typedef struct unknown_t unknown_t;

/* ------------------------------------------------------------------------- *
 * unknown_t
 * ------------------------------------------------------------------------- */

struct unknown_t
{
  char  *un_data;
  size_t un_size;
};

#define UNKNOWN_INIT { 0, 0 }

/* ========================================================================= *
 * unknown_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * unknown_ctor
 * ------------------------------------------------------------------------- */

void
unknown_ctor(unknown_t *self)
{
  self->un_data = 0;
  self->un_size = 0;
}

/* ------------------------------------------------------------------------- *
 * unknown_dtor
 * ------------------------------------------------------------------------- */

void
unknown_dtor(unknown_t *self)
{
  free(self->un_data);
}

/* ------------------------------------------------------------------------- *
 * unknown_create
 * ------------------------------------------------------------------------- */

unknown_t *
unknown_create(void)
{
  unknown_t *self = calloc(1, sizeof *self);
  unknown_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * unknown_delete
 * ------------------------------------------------------------------------- */

void
unknown_delete(unknown_t *self)
{
  if( self != 0 )
  {
    unknown_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * unknown_delete_cb
 * ------------------------------------------------------------------------- */

void
unknown_delete_cb(void *self)
{
  unknown_delete(self);
}

/* ------------------------------------------------------------------------- *
 * unknown_add
 * ------------------------------------------------------------------------- */

int
unknown_add(unknown_t *self, const char *txt)
{
  char *entry = 0;
  while( (entry = argz_next(self->un_data, self->un_size, entry)) != 0 )
  {
    if( !strcmp(entry, txt) )
    {
      return 0;
    }
  }
  argz_add(&self->un_data, &self->un_size, txt);
  return 1;
}

/* ========================================================================= *
 * utilities
 * ========================================================================= */

void cstring_set(char **dest, char *srce)
{
  free(*dest); *dest = srce ? strdup(srce) : 0;
}

INLINE int path_isdir(const char *path)
{
  struct stat st;
  if( stat(path, &st) == 0 && S_ISDIR(st.st_mode) )
  {
    return 1;
  }
  return 0;
}

INLINE char *path_basename(const char *path)
{
  char *r = strrchr(path, '/');
  return r ? (r+1) : (char *)path;
}

INLINE char *path_extension(const char *path)
{
  char *b = path_basename(path);
  char *e = strrchr(b, '.');
  return e ? e : strchr(b,0);
}

INLINE int path_compare(const char *p1, const char *p2)
{
  int r;

  r = (*p1 != '[') - (*p2 != '[');
  if (r) return r;

  r = (strstr(p1,".cache")!=0) - (strstr(p2,".cache")!=0);

  if( r == 0 )
  {
    r = strcasecmp(path_basename(p1), path_basename(p2));
  }

  return r ? r : strcasecmp(p1, p2);
// QUARANTINE   return strcmp(path_basename(p1), path_basename(p2));
}

// QUARANTINE INLINE unsigned umax(unsigned a, unsigned b)
// QUARANTINE {
// QUARANTINE   return (a>b)?a:b;
// QUARANTINE }

// QUARANTINE INLINE unsigned umax3(unsigned a, unsigned b, unsigned c)
// QUARANTINE {
// QUARANTINE   return umax(umax(a,b),c);
// QUARANTINE }

INLINE double fmax3(unsigned a, unsigned b, unsigned c)
{
  return fmax(fmax(a,b),c);
}

INLINE double pow2(double a)
{
  return a*a;
}

// QUARANTINE INLINE unsigned usum(unsigned a, unsigned b)
// QUARANTINE {
// QUARANTINE   return a+b;
// QUARANTINE }
// QUARANTINE INLINE unsigned umax(unsigned a, unsigned b)
// QUARANTINE {
// QUARANTINE   return (a>b)?a:b;
// QUARANTINE }

INLINE void pusum(unsigned *a, unsigned b)
{
  *a += b;
}
INLINE void pumax(unsigned *a, unsigned b)
{
  if( *a < b ) *a=b;
}

/* Pass additional data to qsort() comparison function. */
static void *qsort_cmp_data;

/* ------------------------------------------------------------------------- *
 * array_find_lower  --  bsearch lower bound from sorted array_t
 * ------------------------------------------------------------------------- */

int
array_find_lower(array_t *self, int lo, int hi, int (*fn)(const void*))
{
  while( lo < hi )
  {
    int i = (lo + hi) / 2;
    int r = fn(self->data[i]);
    if( r >= 0 ) { hi = i+0; } else { lo = i+1; }
  }
  return lo;
}

/* ------------------------------------------------------------------------- *
 * array_find_upper  --  bsearch upper bound from sorted array_t
 * ------------------------------------------------------------------------- */

int
array_find_upper(array_t *self, int lo, int hi, int (*fn)(const void*))
{
  while( lo < hi )
  {
    int i = (lo + hi) / 2;
    int r = fn(self->data[i]);
    if( r > 0 ) { hi = i+0; } else { lo = i+1; }
  }
  return lo;
}

/* ------------------------------------------------------------------------- *
 * uval  --  return number as string or "-" for zero values
 * ------------------------------------------------------------------------- */

const char *uval(unsigned n)
{
  static char temp[512];
  snprintf(temp, sizeof temp, "%u", n);
  return n ? temp : "-";
}

/* ------------------------------------------------------------------------- *
 * array_move  --  TODO: this should be in libsysperf
 * ------------------------------------------------------------------------- */

INLINE void
array_move(array_t *self, array_t *from)
{
  array_minsize(self, self->size + from->size);
  for( size_t i = 0; i < from->size; ++i )
  {
    self->data[self->size++] = from->data[i];
  }
  from->size = 0;
}

/* ------------------------------------------------------------------------- *
 * xstrfmt  --  sprintf to dynamically allocated buffer
 * ------------------------------------------------------------------------- */

STATIC char *
xstrfmt(char **pstr, const char *fmt, ...)
{
  char *res = 0;
  char tmp[256];
  va_list va;
  int nc;

  va_start(va, fmt);
  nc = vsnprintf(tmp, sizeof tmp, fmt, va);
  va_end(va);

  if( nc >= 0 )
  {
    if( nc < sizeof tmp )
    {
      res = strdup(tmp);
    }
    else
    {
      res = malloc(nc + 1);
      va_start(va, fmt);
      vsnprintf(res, nc+1, fmt, va);
      va_end(va);
    }
  }

  free(*pstr);
  return *pstr = res;
}

/* ------------------------------------------------------------------------- *
 * slice  --  split string at separator char
 * ------------------------------------------------------------------------- */

STATIC char *
slice(char **ppos, int sep)
{
  char *pos = *ppos;

  while( (*pos > 0) && (*pos <= 32) )
  {
    ++pos;
  }
  char *res = pos;

  if( sep < 0 )
  {
    for( ; *pos; ++pos )
    {
      if( *(unsigned char *)pos <= 32 )
      {
        *pos++ = 0; break;
      }
    }
  }
  else
  {
    for( ; *pos; ++pos )
    {
      if( *(unsigned char *)pos == sep )
      {
        *pos++ = 0; break;
      }
    }
  }

  *ppos = pos;
  return res;
}

/* ========================================================================= *
 * Custom Objects
 * ========================================================================= */

typedef struct analyze_t analyze_t;

/* - - - - - - - - - - - - - - - - - - - *
 * container classes
 * - - - - - - - - - - - - - - - - - - - */

typedef struct smapsfilt_t smapsfilt_t;
typedef struct smapssnap_t smapssnap_t;
typedef struct smapsproc_t smapsproc_t;
typedef struct smapsmapp_t smapsmapp_t;

/* - - - - - - - - - - - - - - - - - - - *
 * rawdata classes
 * - - - - - - - - - - - - - - - - - - - */

typedef struct pidinfo_t pidinfo_t; // Name,Pid,PPid, ...
typedef struct mapinfo_t mapinfo_t; // head,tail,prot, ...
typedef struct meminfo_t meminfo_t; // Size,RSS,Shared_Clean, ...

/* ------------------------------------------------------------------------- *
 * meminfo_t
 * ------------------------------------------------------------------------- */

struct meminfo_t
{
  unsigned Size;
  unsigned Rss;
  unsigned Shared_Clean;
  unsigned Shared_Dirty;
  unsigned Private_Clean;
  unsigned Private_Dirty;
  unsigned Pss;
  unsigned Swap;
  unsigned Referenced;
  unsigned Anonymous;
  unsigned Locked;
};

void       meminfo_ctor              (meminfo_t *self);
void       meminfo_dtor              (meminfo_t *self);
void       meminfo_accumulate_appdata(meminfo_t *self, const meminfo_t *that);
void       meminfo_accumulate_libdata(meminfo_t *self, const meminfo_t *that);
void       meminfo_parse             (meminfo_t *self, char *line);

meminfo_t *meminfo_create            (void);
void       meminfo_delete            (meminfo_t *self);
void       meminfo_delete_cb         (void *self);

/* ------------------------------------------------------------------------- *
 * meminfo_total
 * ------------------------------------------------------------------------- */

INLINE unsigned
meminfo_total(const meminfo_t *self)
{
  return (self->Shared_Clean  +
          self->Shared_Dirty  +
          self->Private_Clean +
          self->Private_Dirty);
}

/* ------------------------------------------------------------------------- *
 * mapinfo_t
 * ------------------------------------------------------------------------- */

struct mapinfo_t
{
  unsigned   head;
  unsigned   tail;
  char      *prot;
  unsigned   offs;
  char      *node;
  unsigned   flgs;
  char      *path;
  char      *type;
};

void       mapinfo_ctor     (mapinfo_t *self);
void       mapinfo_dtor     (mapinfo_t *self);

mapinfo_t *mapinfo_create   (void);
void       mapinfo_delete   (mapinfo_t *self);
void       mapinfo_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * pidinfo_t
 * ------------------------------------------------------------------------- */

struct pidinfo_t
{
  char    *Name;
  int      Pid;
  int      PPid;
  int      Threads;
  unsigned VmPeak;
  unsigned VmSize;
  unsigned VmLck;
  unsigned VmHWM;
  unsigned VmRSS;
  unsigned VmData;
  unsigned VmStk;
  unsigned VmExe;
  unsigned VmLib;
  unsigned VmPTE;
};

void       pidinfo_ctor     (pidinfo_t *self);
void       pidinfo_dtor     (pidinfo_t *self);
void       pidinfo_parse    (pidinfo_t *self, char *line);

pidinfo_t *pidinfo_create   (void);
void       pidinfo_delete   (pidinfo_t *self);
void       pidinfo_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * smapsmapp_t
 * ------------------------------------------------------------------------- */

struct smapsmapp_t
{
  int       smapsmapp_uid; // create -> load order enumeration

  mapinfo_t smapsmapp_map;
  meminfo_t smapsmapp_mem;

  int       smapsmapp_AID;
  int       smapsmapp_PID;
  int       smapsmapp_LID;
  int       smapsmapp_TID;
  int       smapsmapp_EID;
};

void         smapsmapp_ctor     (smapsmapp_t *self);
void         smapsmapp_dtor     (smapsmapp_t *self);

smapsmapp_t *smapsmapp_create   (void);
void         smapsmapp_delete   (smapsmapp_t *self);
void         smapsmapp_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * smapsproc_t
 * ------------------------------------------------------------------------- */

struct smapsproc_t
{
  int          smapsproc_uid;

  int          smapsproc_AID;
  int          smapsproc_PID;

  /* - - - - - - - - - - - - - - - - - - - *
   * smaps data & hierarchy
   * - - - - - - - - - - - - - - - - - - - */

  pidinfo_t    smapsproc_pid;
  array_t      smapsproc_mapplist; // -> smapsmapp_t *

  /* - - - - - - - - - - - - - - - - - - - *
   * process hierarchy info
   *
   * Note: Owenership of childprocess data
   *       remains with smapssnap_t!
   * - - - - - - - - - - - - - - - - - - - */

  smapsproc_t *smapsproc_parent;
  array_t      smapsproc_children; // -> smapsproc_t *
};

void         smapsproc_ctor     (smapsproc_t *self);
void         smapsproc_dtor     (smapsproc_t *self);

smapsproc_t *smapsproc_create   (void);
void         smapsproc_delete   (smapsproc_t *self);
void         smapsproc_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * smapssnap_t
 * ------------------------------------------------------------------------- */

struct smapssnap_t
{
  char       *smapssnap_source;
  int         smapssnap_format;
  array_t     smapssnap_proclist; // -> smapsproc_t *
  smapsproc_t smapssnap_rootproc;
};

enum {
  SNAPFORMAT_OLD, // head /proc/[1-9]*/smaps > snapshot.cap
  SNAPFORMAT_NEW, // sp_smaps_snapshot -o snapshot.cap
};

void         smapssnap_ctor     (smapssnap_t *self);
void         smapssnap_dtor     (smapssnap_t *self);

smapssnap_t *smapssnap_create   (void);
void         smapssnap_delete   (smapssnap_t *self);
void         smapssnap_delete_cb(void *self);

/* ------------------------------------------------------------------------- *
 * analyze_t  --  temporary book keeping structure for smaps snapshot analysis
 * ------------------------------------------------------------------------- */

struct analyze_t
{
  // smaps data for all processes will be collected here

  array_t  *mapp_tab;

  // enumeration tables

  symtab_t *appl_tab;  // application names
  symtab_t *type_tab;  // mapping types: code, data, anon, ...
  symtab_t *path_tab;  // mapping paths
  symtab_t *summ_tab;  // app instance + mapping path

  int ntypes;          // enumeration counts
  int nappls;
  int npaths;
  int groups;

  const char **stype;  // enumeration -> string lookup tables
  const char **sappl;
  const char **spath;

  int *grp_app;        // group enum -> appid / libid lookup tables
  int *grp_lib;

  // memory usage accumulation tables

  meminfo_t *grp_mem; // [groups * ntypes];
  meminfo_t *app_mem; // [nappls * ntypes];
  meminfo_t *lib_mem; // [npaths * ntypes];
  meminfo_t *sysest;  // [ntypes]
  meminfo_t *sysmax;  // [ntypes]
  meminfo_t *appmax;  // [ntypes]
};

void       analyze_ctor                  (analyze_t *self);
void       analyze_dtor                  (analyze_t *self);
analyze_t *analyze_create                (void);
void       analyze_delete                (analyze_t *self);
void       analyze_delete_cb             (void *self);
void       analyze_enumerate_data        (analyze_t *self, smapssnap_t *snap);
void       analyze_accumulate_data       (analyze_t *self);
void       analyze_get_apprange          (analyze_t *self, int lo, int hi, int *plo, int *phi, int aid);
void       analyze_get_librange          (analyze_t *self, int lo, int hi, int *plo, int *phi, int lid);
int        analyze_emit_lib_html         (analyze_t *self, smapssnap_t *snap, const char *work);
int        analyze_emit_app_html         (analyze_t *self, smapssnap_t *snap, const char *work);
void       analyze_emit_smaps_table      (analyze_t *self, FILE *file, meminfo_t *v);
void       analyze_emit_process_hierarchy(analyze_t *self, FILE *file, smapsproc_t *proc, const char *work, int recursion_depth);
int        analyze_emit_main_page        (analyze_t *self, smapssnap_t *snap, const char *path);

/* ------------------------------------------------------------------------- *
 * smapsfilt_t
 * ------------------------------------------------------------------------- */

enum
{
  FM_FLATTEN,
  FM_NORMALIZE,
  FM_ANALYZE,
  FM_APPVALS,
  FM_DIFF,
};

struct smapsfilt_t
{
  int         smapsfilt_filtmode;
  int         smapsfilt_difflevel;
  int         smapsfilt_trimlevel;
  str_array_t smapsfilt_inputs;
  char       *smapsfilt_output;

  array_t smapsfilt_snaplist; // -> smapssnap_t *
};

void         smapsfilt_ctor     (smapsfilt_t *self);
void         smapsfilt_dtor     (smapsfilt_t *self);

smapsfilt_t *smapsfilt_create   (void);
void         smapsfilt_delete   (smapsfilt_t *self);
void         smapsfilt_delete_cb(void *self);

/* ========================================================================= *
 * meminfo_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * meminfo_ctor
 * ------------------------------------------------------------------------- */

void
meminfo_ctor(meminfo_t *self)
{
  memset(self, 0, sizeof(*self));
}

/* ------------------------------------------------------------------------- *
 * meminfo_dtor
 * ------------------------------------------------------------------------- */

void
meminfo_dtor(meminfo_t *self)
{
}

/* ------------------------------------------------------------------------- *
 * meminfo_parse
 * ------------------------------------------------------------------------- */

void
meminfo_parse(meminfo_t *self, char *line)
{
  char *key = slice(&line, ':');
  char *val = slice(&line,  -1);

  if( !strcmp(key, "Size") )
  {
    self->Size  = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Rss") )
  {
    self->Rss   = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Shared_Clean") )
  {
    self->Shared_Clean  = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Shared_Dirty") )
  {
    self->Shared_Dirty  = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Private_Clean") )
  {
    self->Private_Clean = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Private_Dirty") )
  {
    self->Private_Dirty = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Pss") )
  {
    self->Pss   = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Swap") )
  {
    self->Swap   = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Referenced") )
  {
    self->Referenced   = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Anonymous") )
  {
    self->Anonymous = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "Locked") )
  {
    self->Locked = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "KernelPageSize")
        || !strcmp(key, "MMUPageSize")
      )
  {
  }
  else
  {
    static unknown_t unkn = UNKNOWN_INIT;
    if( unknown_add(&unkn, key) )
    {
      fprintf(stderr, "%s: Unknown key: '%s' = '%s'\n", __FUNCTION__, key, val);
    }
  }
}

static int
meminfo_all_zeroes(const meminfo_t *self)
{
  return self->Size == 0
    && self->Rss == 0
    && self->Shared_Clean == 0
    && self->Shared_Dirty == 0
    && self->Private_Clean == 0
    && self->Private_Dirty == 0
    && self->Pss == 0
    && self->Swap == 0
    && self->Referenced == 0
    && self->Anonymous == 0
    && self->Locked == 0
    ;
}

/* ------------------------------------------------------------------------- *
 * meminfo_accumulate_appdata
 * ------------------------------------------------------------------------- */

void
meminfo_accumulate_appdata(meminfo_t *self, const meminfo_t *that)
{
  pusum(&self->Size,          that->Size);
  pusum(&self->Rss,           that->Rss);
  pusum(&self->Shared_Clean,  that->Shared_Clean);
  pusum(&self->Shared_Dirty,  that->Shared_Dirty);
  pusum(&self->Private_Clean, that->Private_Clean);
  pusum(&self->Private_Dirty, that->Private_Dirty);
  pusum(&self->Pss,           that->Pss);
  pusum(&self->Swap,          that->Swap);
  pusum(&self->Referenced,    that->Referenced);
  pusum(&self->Anonymous,     that->Anonymous);
  pusum(&self->Locked,        that->Locked);
}

/* ------------------------------------------------------------------------- *
 * meminfo_accumulate_libdata
 * ------------------------------------------------------------------------- */

void
meminfo_accumulate_libdata(meminfo_t *self, const meminfo_t *that)
{
  pumax(&self->Size,          that->Size);
  pumax(&self->Rss,           that->Rss);
  pumax(&self->Shared_Clean,  that->Shared_Clean);
  pumax(&self->Shared_Dirty,  that->Shared_Dirty);
  pusum(&self->Private_Clean, that->Private_Clean);
  pusum(&self->Private_Dirty, that->Private_Dirty);
  pusum(&self->Pss,           that->Pss);
  pumax(&self->Swap,          that->Swap);
  pumax(&self->Referenced,    that->Referenced);
  pusum(&self->Anonymous,     that->Anonymous);
  pusum(&self->Locked,        that->Locked);
}

/* ------------------------------------------------------------------------- *
 * meminfo_accumulate_maxdata
 * ------------------------------------------------------------------------- */

void
meminfo_accumulate_maxdata(meminfo_t *self, const meminfo_t *that)
{
  pumax(&self->Size,          that->Size);
  pumax(&self->Rss,           that->Rss);
  pumax(&self->Shared_Clean,  that->Shared_Clean);
  pumax(&self->Shared_Dirty,  that->Shared_Dirty);
  pumax(&self->Private_Clean, that->Private_Clean);
  pumax(&self->Private_Dirty, that->Private_Dirty);
  pumax(&self->Pss,           that->Pss);
  pumax(&self->Swap,          that->Swap);
  pumax(&self->Referenced,    that->Referenced);
  pumax(&self->Anonymous,     that->Anonymous);
  pumax(&self->Locked,        that->Locked);
}

/* ------------------------------------------------------------------------- *
 * meminfo_create
 * ------------------------------------------------------------------------- */

meminfo_t *
meminfo_create(void)
{
  meminfo_t *self = calloc(1, sizeof *self);
  meminfo_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * meminfo_delete
 * ------------------------------------------------------------------------- */

void
meminfo_delete(meminfo_t *self)
{
  if( self != 0 )
  {
    meminfo_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * meminfo_delete_cb
 * ------------------------------------------------------------------------- */

void
meminfo_delete_cb(void *self)
{
  meminfo_delete(self);
}

/* ========================================================================= *
 * mapinfo_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * mapinfo_ctor
 * ------------------------------------------------------------------------- */

void
mapinfo_ctor(mapinfo_t *self)
{
  self->head    = 0;
  self->tail    = 0;
  self->prot    = 0;
  self->offs    = 0;
  self->node    = 0;
  self->flgs    = 0;
  self->path    = 0;
  self->type    = 0;
}

/* ------------------------------------------------------------------------- *
 * mapinfo_dtor
 * ------------------------------------------------------------------------- */

void
mapinfo_dtor(mapinfo_t *self)
{
  free(self->prot);
  free(self->node);
  free(self->path);
  free(self->type);
}

/* ------------------------------------------------------------------------- *
 * mapinfo_create
 * ------------------------------------------------------------------------- */

mapinfo_t *
mapinfo_create(void)
{
  mapinfo_t *self = calloc(1, sizeof *self);
  mapinfo_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * mapinfo_delete
 * ------------------------------------------------------------------------- */

void
mapinfo_delete(mapinfo_t *self)
{
  if( self != 0 )
  {
    mapinfo_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * mapinfo_delete_cb
 * ------------------------------------------------------------------------- */

void
mapinfo_delete_cb(void *self)
{
  mapinfo_delete(self);
}

/* ========================================================================= *
 * pidinfo_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * pidinfo_ctor
 * ------------------------------------------------------------------------- */

void
pidinfo_ctor(pidinfo_t *self)
{
  memset(self, 0, sizeof(*self));
  self->Name = strdup("<noname>");
}

/* ------------------------------------------------------------------------- *
 * pidinfo_dtor
 * ------------------------------------------------------------------------- */

void
pidinfo_dtor(pidinfo_t *self)
{
  free(self->Name);
}

/* ------------------------------------------------------------------------- *
 * pidinfo_parse
 * ------------------------------------------------------------------------- */

void
pidinfo_parse(pidinfo_t *self, char *line)
{
  char *key = slice(&line, ':');
  char *val = slice(&line,  -1);

  if( !strcmp(key, "Name") )
  {
    while( *val == '-' ) ++val;
    xstrset(&self->Name, val);
  }
  else if( !strcmp(key, "Pid") )
  {
    self->Pid   = strtol(val, 0, 10);
  }
  else if( !strcmp(key, "PPid") )
  {
    self->PPid  = strtol(val, 0, 10);
  }
  else if( !strcmp(key, "Threads") )
  {
    self->Threads       = strtol(val, 0, 10);
  }
  else if( !strcmp(key, "VmPeak") )
  {
    self->VmPeak        = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmSize") )
  {
    self->VmSize        = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmLck") )
  {
    self->VmLck = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmHWM") )
  {
    self->VmHWM = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmRSS") )
  {
    self->VmRSS = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmData") )
  {
    self->VmData        = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmStk") )
  {
    self->VmStk = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmExe") )
  {
    self->VmExe = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmLib") )
  {
    self->VmLib = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "VmPTE") )
  {
    self->VmPTE = strtoul(val, 0, 10);
  }
  else if( !strcmp(key, "State")
        || !strcmp(key, "Tgid")
        || !strcmp(key, "TracerPid")
        || !strcmp(key, "Uid")
        || !strcmp(key, "Gid")
        || !strcmp(key, "FDSize")
        || !strcmp(key, "Groups")
        || !strcmp(key, "SigQ")
        || !strcmp(key, "SigPnd")
        || !strcmp(key, "ShdPnd")
        || !strcmp(key, "SigBlk")
        || !strcmp(key, "SigCgt")
        || !strcmp(key, "SigIgn")
        || !strcmp(key, "CapInh")
        || !strcmp(key, "CapPrm")
        || !strcmp(key, "CapEff")
        || !strcmp(key, "CapBnd")
        || !strcmp(key, "voluntary_ctxt_switches")
        || !strcmp(key, "nonvoluntary_ctxt_switches")
      )
  {
  }
  else
  {
    static unknown_t unkn = UNKNOWN_INIT;
    if( unknown_add(&unkn, key) )
    {
      fprintf(stderr, "%s: Unknown key: '%s' = '%s'\n", __FUNCTION__, key, val);
    }
  }
}

/* ------------------------------------------------------------------------- *
 * pidinfo_create
 * ------------------------------------------------------------------------- */

pidinfo_t *
pidinfo_create(void)
{
  pidinfo_t *self = calloc(1, sizeof *self);
  pidinfo_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * pidinfo_delete
 * ------------------------------------------------------------------------- */

void
pidinfo_delete(pidinfo_t *self)
{
  if( self != 0 )
  {
    pidinfo_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * pidinfo_delete_cb
 * ------------------------------------------------------------------------- */

void
pidinfo_delete_cb(void *self)
{
  pidinfo_delete(self);
}

/* ========================================================================= *
 * smapsmapp_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * smapsmapp_ctor
 * ------------------------------------------------------------------------- */

void
smapsmapp_ctor(smapsmapp_t *self)
{
  static int uid = 0;
  self->smapsmapp_uid = uid++;

  self->smapsmapp_AID = -1;
  self->smapsmapp_PID = -1;
  self->smapsmapp_LID = -1;
  self->smapsmapp_TID = -1;
  self->smapsmapp_EID = -1;

  mapinfo_ctor(&self->smapsmapp_map);
  meminfo_ctor(&self->smapsmapp_mem);
}

/* ------------------------------------------------------------------------- *
 * smapsmapp_dtor
 * ------------------------------------------------------------------------- */

void
smapsmapp_dtor(smapsmapp_t *self)
{
  mapinfo_dtor(&self->smapsmapp_map);
  meminfo_dtor(&self->smapsmapp_mem);
}

/* ------------------------------------------------------------------------- *
 * smapsmapp_create
 * ------------------------------------------------------------------------- */

smapsmapp_t *
smapsmapp_create(void)
{
  smapsmapp_t *self = calloc(1, sizeof *self);
  smapsmapp_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * smapsmapp_delete
 * ------------------------------------------------------------------------- */

void
smapsmapp_delete(smapsmapp_t *self)
{
  if( self != 0 )
  {
    smapsmapp_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * smapsmapp_delete_cb
 * ------------------------------------------------------------------------- */

void
smapsmapp_delete_cb(void *self)
{
  smapsmapp_delete(self);
}

/* ========================================================================= *
 * smapsproc_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * smapsproc_ctor
 * ------------------------------------------------------------------------- */

void
smapsproc_ctor(smapsproc_t *self)
{
  static int uid = 0;
  self->smapsproc_uid = uid++;

  self->smapsproc_AID = -1;
  self->smapsproc_PID = -1;

  pidinfo_ctor(&self->smapsproc_pid);
  array_ctor(&self->smapsproc_mapplist, smapsmapp_delete_cb);

  self->smapsproc_parent = 0;
  array_ctor(&self->smapsproc_children, 0);
}

/* ------------------------------------------------------------------------- *
 * smapsproc_dtor
 * ------------------------------------------------------------------------- */

void
smapsproc_dtor(smapsproc_t *self)
{
  pidinfo_dtor(&self->smapsproc_pid);
  array_dtor(&self->smapsproc_mapplist);
  array_dtor(&self->smapsproc_children);
}

/* ------------------------------------------------------------------------- *
 * smapsproc_are_same
 * ------------------------------------------------------------------------- */

int
smapsproc_are_same(smapsproc_t *self, smapsproc_t *that)
{
  if( strcmp(self->smapsproc_pid.Name, that->smapsproc_pid.Name) ) return 0;

#if 01
# define cp(v) if( self->smapsproc_pid.v != that->smapsproc_pid.v ) return 0;
  cp(VmPeak)
  cp(VmSize)
  cp(VmLck)
  cp(VmHWM)
  cp(VmRSS)
  cp(VmData)
  cp(VmStk)
  cp(VmExe)
  cp(VmLib)
  cp(VmPTE)
# undef cp
#endif

  return 1;
}

/* ------------------------------------------------------------------------- *
 * smapsproc_adopt_children
 * ------------------------------------------------------------------------- */

void
smapsproc_adopt_children(smapsproc_t *self, smapsproc_t *that)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * fix parent pointers
   * - - - - - - - - - - - - - - - - - - - */

  for( size_t i = 0; i < that->smapsproc_children.size; ++i )
  {
    smapsproc_t *child = that->smapsproc_children.data[i];
    child->smapsproc_parent = self;
    child->smapsproc_pid.PPid = self->smapsproc_pid.Pid;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * move children to new parent
   * - - - - - - - - - - - - - - - - - - - */

  array_move(&self->smapsproc_children, &that->smapsproc_children);
}

/* ------------------------------------------------------------------------- *
 * smapsproc_collapse_threads
 * ------------------------------------------------------------------------- */

void
smapsproc_collapse_threads(smapsproc_t *self)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * recurse depth first
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->smapsproc_children.size; ++i )
  {
    smapsproc_t *that = self->smapsproc_children.data[i];
    smapsproc_collapse_threads(that);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * heuristic: children that are similar
   * enough to parent are actually threads
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->smapsproc_children.size; ++i )
  {
    smapsproc_t *that = self->smapsproc_children.data[i];

    if( smapsproc_are_same(self, that) )
    {
      fprintf(stderr, "REPARENT: %d\n", that->smapsproc_pid.Pid);

      /* - - - - - - - - - - - - - - - - - - - *
       * adopt grandchildren
       * - - - - - - - - - - - - - - - - - - - */

      smapsproc_adopt_children(self, that);

      /* - - - - - - - - - - - - - - - - - - - *
       * disassosiate parent & child
       * - - - - - - - - - - - - - - - - - - - */

      self->smapsproc_children.data[i] = 0;
      self->smapsproc_pid.Threads += that->smapsproc_pid.Threads;
      that->smapsproc_parent = 0;
    }
  }
  array_compact(&self->smapsproc_children);

}

/* ------------------------------------------------------------------------- *
 * smapsproc_add_mapping
 * ------------------------------------------------------------------------- */

smapsmapp_t  *
smapsproc_add_mapping(smapsproc_t *self,
                      unsigned head,
                      unsigned tail,
                      const char *prot,
                      unsigned offs,
                      const char *node,
                      unsigned flgs,
                      const char *path)
{

  smapsmapp_t *mapp = smapsmapp_create();

  mapp->smapsmapp_map.head = head;
  mapp->smapsmapp_map.tail = tail;
  mapp->smapsmapp_map.offs = offs;
  mapp->smapsmapp_map.flgs = flgs;

  if( path == 0 || *path == 0 )
  {
    path = "[anon]";
  }

  xstrset(&mapp->smapsmapp_map.prot, prot);
  xstrset(&mapp->smapsmapp_map.node, node);
  xstrset(&mapp->smapsmapp_map.path, path);

  if( *path == '[' )
  {
    char temp[32];
    ++path;
    snprintf(temp, sizeof temp, "%.*s", (int)strcspn(path,"]"), path);
    xstrset(&mapp->smapsmapp_map.type, temp);
  }
  else
  {
    xstrset(&mapp->smapsmapp_map.type, strchr(prot, 'x') ? "code" : "data");
  }

  array_add(&self->smapsproc_mapplist, mapp);
  return mapp;
}

/* ------------------------------------------------------------------------- *
 * smapsproc_create
 * ------------------------------------------------------------------------- */

smapsproc_t *
smapsproc_create(void)
{
  smapsproc_t *self = calloc(1, sizeof *self);
  smapsproc_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * smapsproc_delete
 * ------------------------------------------------------------------------- */

void
smapsproc_delete(smapsproc_t *self)
{
  if( self != 0 )
  {
    smapsproc_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * smapsproc_delete_cb
 * ------------------------------------------------------------------------- */

void
smapsproc_delete_cb(void *self)
{
  smapsproc_delete(self);
}

/* ------------------------------------------------------------------------- *
 * smapsproc_compare_pid_cb
 * ------------------------------------------------------------------------- */

int smapsproc_compare_pid_cb(const void *a1, const void *a2)
{
  const smapsproc_t *p1 = *(const smapsproc_t **)a1;
  const smapsproc_t *p2 = *(const smapsproc_t **)a2;
  return p1->smapsproc_pid.Pid - p2->smapsproc_pid.Pid;
}

/* ------------------------------------------------------------------------- *
 * smapsproc_compare_name_pid_cb
 * ------------------------------------------------------------------------- */

int smapsproc_compare_name_pid_cb(const void *a1, const void *a2)
{
  const smapsproc_t *p1 = *(const smapsproc_t **)a1;
  const smapsproc_t *p2 = *(const smapsproc_t **)a2;
  int r = strcasecmp(p1->smapsproc_pid.Name, p2->smapsproc_pid.Name);
  return r ? r : (p1->smapsproc_pid.Pid - p2->smapsproc_pid.Pid);
}

/* ========================================================================= *
 * smapssnap_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * smapssnap_ctor
 * ------------------------------------------------------------------------- */

void
smapssnap_ctor(smapssnap_t *self)
{
  self->smapssnap_source = strdup("<unset>");
  self->smapssnap_format = SNAPFORMAT_OLD;

  array_ctor(&self->smapssnap_proclist, smapsproc_delete_cb);
  smapsproc_ctor(&self->smapssnap_rootproc);
}

/* ------------------------------------------------------------------------- *
 * smapssnap_dtor
 * ------------------------------------------------------------------------- */

void
smapssnap_dtor(smapssnap_t *self)
{
  free(self->smapssnap_source);
  array_dtor(&self->smapssnap_proclist);
  smapsproc_dtor(&self->smapssnap_rootproc);
}

/* ------------------------------------------------------------------------- *
 * smapssnap_get_source
 * ------------------------------------------------------------------------- */

const char *
smapssnap_get_source(smapssnap_t *self)
{
  return self->smapssnap_source;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_set_source
 * ------------------------------------------------------------------------- */

void
smapssnap_set_source(smapssnap_t *self, const char *path)
{
  xstrset(&self->smapssnap_source, path);
}

/* ------------------------------------------------------------------------- *
 * smapssnap_create_hierarchy
 * ------------------------------------------------------------------------- */

/* - - - - - - - - - - - - - - - - - - - *
 * binsearch utility function
 * - - - - - - - - - - - - - - - - - - - */

static smapsproc_t *
proc_find(smapssnap_t *self, int pid)
{
  int lo = 0, hi = self->smapssnap_proclist.size;
  while( lo < hi )
  {
    int i = (lo + hi) / 2;
    smapsproc_t *p = self->smapssnap_proclist.data[i];

    if( p->smapsproc_pid.Pid > pid ) { hi = i+0; continue; }
    if( p->smapsproc_pid.Pid < pid ) { lo = i+1; continue; }
    return p;
  }
  return 0;
}

void
smapssnap_create_hierarchy(smapssnap_t *self)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * sort processes by PID
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(&self->smapssnap_proclist, smapsproc_compare_pid_cb);

  /* - - - - - - - - - - - - - - - - - - - *
   * find parent for every process
   * - - - - - - - - - - - - - - - - - - - */

  for( size_t i = 0; i < self->smapssnap_proclist.size; ++i )
  {
    smapsproc_t *cur = self->smapssnap_proclist.data[i];
    smapsproc_t *par = proc_find(self, cur->smapsproc_pid.PPid);

    assert( cur->smapsproc_parent == 0 );

    if( par == 0 || par == cur )
    {
      if( cur->smapsproc_pid.PPid != 0 )
      {
        fprintf(stderr, "PPID %d not found\n", cur->smapsproc_pid.PPid);
      }
      par = &self->smapssnap_rootproc;
    }

    cur->smapsproc_parent = par;
    array_add(&par->smapsproc_children, cur);
  }
}

/* ------------------------------------------------------------------------- *
 * smapssnap_collapse_threads
 * ------------------------------------------------------------------------- */

void
smapssnap_collapse_threads(smapssnap_t *self)
{
  smapsproc_collapse_threads(&self->smapssnap_rootproc);

  for( size_t i = 0; i < self->smapssnap_proclist.size; ++i )
  {
    smapsproc_t *cur = self->smapssnap_proclist.data[i];

    if( cur->smapsproc_parent == 0 )
    {
      self->smapssnap_proclist.data[i] = 0;
      smapsproc_delete(cur);
    }
  }
  array_compact(&self->smapssnap_proclist);
}

/* ------------------------------------------------------------------------- *
 * smapssnap_add_process
 * ------------------------------------------------------------------------- */

smapsproc_t *
smapssnap_add_process(smapssnap_t *self, int pid)
{
  smapsproc_t *proc;

  for( int i = self->smapssnap_proclist.size; i-- > 0; )
  {
    proc = self->smapssnap_proclist.data[i];
    if( proc->smapsproc_pid.Pid == pid )
    {
      return proc;
    }
  }
  proc = smapsproc_create();
  proc->smapsproc_pid.Pid = pid;
  array_add(&self->smapssnap_proclist, proc);
  return proc;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_load_cap
 * ------------------------------------------------------------------------- */

static int
hexterm(const char *s)
{
  while( isxdigit(*s) )
  {
    ++s;
  }
  return *s;
}

int
smapssnap_load_cap(smapssnap_t *self, const char *path)
{
  int          error = -1;
  FILE        *file  = 0;
  smapsproc_t *proc  = 0;
  smapsmapp_t *mapp  = 0;
  char        *data  = 0;
  size_t       size  = 0;

  smapssnap_set_source(self, path);

  if( (file = fopen(path, "r")) == 0 )
  {
    perror(path); goto cleanup;
  }

  while( getline(&data, &size, file) >= 0 )
  {
    data[strcspn(data, "\r\n")] = 0;

    if( *data == 0 )
    {
      // ignore empty lines
    }
    else if( !strncmp(data, "==>", 3) )
    {
      // ==> /proc/1/smaps <==

      proc = 0;
      mapp = 0;

      char *backup = strdup(data); // save a copy for good error messages
      char *pos = data;

      while( *pos && strcmp(slice(&pos, '/'), "proc") ) { }
      int pid = strtol(slice(&pos, '/'), 0, 10);
      if( pid > 0 && !strcmp(slice(&pos, -1), "smaps") )
      {
        proc = smapssnap_add_process(self, pid);
      }
      else
      {
        fprintf(stderr, "%s(): ignoring: %s\n", __FUNCTION__, backup);
      }

      free(backup);
    }
    else if( *data == '#' )
    {
      // #Name: init__2_
      // #Pid: 1
      // #PPid: 0
      // #Threads: 1

      if (proc)
      {
        pidinfo_parse(&proc->smapsproc_pid, data+1);
        self->smapssnap_format = SNAPFORMAT_NEW;
      }
    }
    else if( hexterm(data) == '-' )
    {
      // 08048000-08051000 r-xp 00000000 03:03 2060370    /sbin/init

      if (proc)
      {
        char *pos = data;
        unsigned head = strtoul(slice(&pos, '-'), 0, 16);
        unsigned tail = strtoul(slice(&pos,  -1), 0, 16);
        char    *prot = slice(&pos,  -1);
        unsigned offs = strtoul(slice(&pos,  -1), 0, 16);
        char    *node = slice(&pos,  -1);
        unsigned flgs = strtoul(slice(&pos,  -1), 0, 10);
        char    *path = slice(&pos,  0);

        mapp = smapsproc_add_mapping(proc, head, tail, prot,
                                     offs, node, flgs, path);
      }
    }
    else
    {
      // Size:                36 kB
      // Rss:                 36 kB
      // Shared_Clean:         0 kB
      // Shared_Dirty:         0 kB
      // Private_Clean:       36 kB
      // Private_Dirty:        0 kB

      if (mapp)
      {
        meminfo_parse(&mapp->smapsmapp_mem, data);
      }
    }
  }

  error = 0;

  cleanup:

  free(data);

  if( file ) fclose(file);

  return error;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_save_cap
 * ------------------------------------------------------------------------- */

int
smapssnap_save_cap(smapssnap_t *self, const char *path)
{
  int          error = -1;
  FILE        *file  = 0;

  if( (file = fopen(path, "w")) == 0 )
  {
    perror(path); goto cleanup;
  }

  array_sort(&self->smapssnap_proclist, smapsproc_compare_pid_cb);

  for( int p = 0; p < self->smapssnap_proclist.size; ++p )
  {
    const smapsproc_t *proc = self->smapssnap_proclist.data[p];
    const pidinfo_t   *pi = &proc->smapsproc_pid;

    fprintf(file, "==> /proc/%d/smaps <==\n", pi->Pid);

#define Ps(v) fprintf(file, "#%s: %s\n", #v, pi->v)
#define Pi(v) fprintf(file, "#%s: %d\n", #v, pi->v)
#define Pu(v) fprintf(file, "#%s: %u\n", #v, pi->v)

    Ps(Name);
    Pi(Pid);
    Pi(PPid);
    Pi(Threads);

    if( pi->VmPeak
     || pi->VmSize
     || pi->VmLck
     || pi->VmHWM
     || pi->VmRSS
     || pi->VmData
     || pi->VmStk
     || pi->VmExe
     || pi->VmLib
     || pi->VmPTE
     )
    {
      Pu(VmPeak);
      Pu(VmSize);
      Pu(VmLck);
      Pu(VmHWM);
      Pu(VmRSS);
      Pu(VmData);
      Pu(VmStk);
      Pu(VmExe);
      Pu(VmLib);
      Pu(VmPTE);
    }
#undef Pu
#undef Pi
#undef Ps

    for( int m = 0; m < proc->smapsproc_mapplist.size; ++m )
    {
      const smapsmapp_t *mapp = proc->smapsproc_mapplist.data[m];
      const mapinfo_t   *map  = &mapp->smapsmapp_map;
      const meminfo_t   *mem  = &mapp->smapsmapp_mem;

      fprintf(file, "%08x-%08x %s %08x %s %-10u %s\n",
              map->head, map->tail, map->prot,
              map->offs, map->node, map->flgs,
              map->path);

#define Pu(v) fprintf(file, "%-14s %8u kB\n", #v":", mem->v)

      Pu(Size);
      Pu(Rss);
      Pu(Shared_Clean);
      Pu(Shared_Dirty);
      Pu(Private_Clean);
      Pu(Private_Dirty);
      Pu(Pss);
      Pu(Swap);
      Pu(Referenced);
      Pu(Anonymous);
      Pu(Locked);

#undef Pu
    }
    fprintf(file, "\n");
  }

  error = 0;

  cleanup:

  if( file ) fclose(file);

  return error;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_save_csv
 * ------------------------------------------------------------------------- */

int
smapssnap_save_csv(smapssnap_t *self, const char *path)
{
  int   error = -1;
  FILE *file  = 0;

  if( (file = fopen(path, "w")) == 0 )
  {
    perror(path); goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * arrange data by process name & pid
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(&self->smapssnap_proclist, smapsproc_compare_pid_cb);

// QUARANTINE   array_sort(&self->snapshot_process_list,
// QUARANTINE        smapsproc_compare_name_pid_cb);

  /* - - - - - - - - - - - - - - - - - - - *
   * output csv header
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "generator=%s %s\n", "PROGNAME", "PROGVERS");
  fprintf(file, "\n");

  /* - - - - - - - - - - - - - - - - - - - *
   * output csv labels
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file,
          "name,pid,ppid,threads,"
          "head,tail,prot,offs,node,flag,path,"
          "size,rss,shacln,shadty,pricln,pridty,"
          "pss,swap,referenced,anonymous,locked,"
          "pri,sha,cln\n");

  /* - - - - - - - - - - - - - - - - - - - *
   * output csv table
   * - - - - - - - - - - - - - - - - - - - */

  for( size_t i = 0; i < self->smapssnap_proclist.size; ++i )
  {
    const smapsproc_t *proc = self->smapssnap_proclist.data[i];
    const pidinfo_t   *pid  = &proc->smapsproc_pid;

    for( size_t k = 0; k < proc->smapsproc_mapplist.size; ++k )
    {
      const smapsmapp_t *mapp = proc->smapsproc_mapplist.data[k];
      const mapinfo_t   *map  = &mapp->smapsmapp_map;
      const meminfo_t   *mem  = &mapp->smapsmapp_mem;

      fprintf(file, "%s,%d,%d,%d,",
              pid->Name,
              pid->Pid,
              pid->PPid,
              pid->Threads);

      fprintf(file, "%u,%u,%s,%u,%s,%u,%s,",
              map->head, map->tail, map->prot,
              map->offs, map->node, map->flgs,
              map->path);

      fprintf(file, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
              mem->Size,
              mem->Rss,
              mem->Shared_Clean,
              mem->Shared_Dirty,
              mem->Private_Clean,
              mem->Private_Dirty,
              mem->Pss,
              mem->Swap,
              mem->Referenced,
              mem->Anonymous,
              mem->Locked);

      fprintf(file, "%u,%u,%u\n",
              mem->Private_Dirty,
              mem->Shared_Dirty,
              mem->Private_Clean + mem->Shared_Clean);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * terminate csv table
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "\n");

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  error = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  if( file )
  {
    fclose(file);
  }

  return error;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_save_html
 * ------------------------------------------------------------------------- */

// QUARANTINE int
// QUARANTINE smapssnap_save_html(smapssnap_t *self, const char *path)
// QUARANTINE {
// QUARANTINE   int       error = -1;
// QUARANTINE   analyze_t *az   = analyze_create();
// QUARANTINE
// QUARANTINE   /* - - - - - - - - - - - - - - - - - - - *
// QUARANTINE    * enumerate & accumulate data
// QUARANTINE    * - - - - - - - - - - - - - - - - - - - */
// QUARANTINE
// QUARANTINE   analyze_enumerate_data(az, self);
// QUARANTINE
// QUARANTINE   analyze_accumulate_data(az);
// QUARANTINE
// QUARANTINE   /* - - - - - - - - - - - - - - - - - - - *
// QUARANTINE    * output html pages
// QUARANTINE    * - - - - - - - - - - - - - - - - - - - */
// QUARANTINE
// QUARANTINE   error = analyze_emit_main_page(az, self, path);
// QUARANTINE
// QUARANTINE
// QUARANTINE   analyze_delete(az);
// QUARANTINE   return error;
// QUARANTINE }

/* ------------------------------------------------------------------------- *
 * smapssnap_create
 * ------------------------------------------------------------------------- */

smapssnap_t *
smapssnap_create(void)
{
  smapssnap_t *self = calloc(1, sizeof *self);
  smapssnap_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * smapssnap_delete
 * ------------------------------------------------------------------------- */

void
smapssnap_delete(smapssnap_t *self)
{
  if( self != 0 )
  {
    smapssnap_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * smapssnap_delete_cb
 * ------------------------------------------------------------------------- */

void
smapssnap_delete_cb(void *self)
{
  smapssnap_delete(self);
}

//#include "scrap.txt"

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

enum emit_type {
  EMIT_TYPE_LIBRARY,
  EMIT_TYPE_APPLICATION,
  EMIT_TYPE_OBJECT,
};

/* ========================================================================= *
 * analyze_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * analyze_ctor
 * ------------------------------------------------------------------------- */

void
analyze_ctor(analyze_t *self)
{
  self->mapp_tab = array_create(0); /* ownership of data not taken */

  self->appl_tab = symtab_create();
  self->type_tab = symtab_create();
  self->path_tab = symtab_create();
  self->summ_tab = symtab_create();

  self->ntypes = 0;
  self->nappls = 0;
  self->npaths = 0;
  self->groups = 0;

  self->stype  = 0;
  self->sappl  = 0;
  self->spath  = 0;

  self->grp_app = 0;
  self->grp_lib = 0;

}

/* ------------------------------------------------------------------------- *
 * analyze_dtor
 * ------------------------------------------------------------------------- */

void
analyze_dtor(analyze_t *self)
{
  array_delete(self->mapp_tab);

  symtab_delete(self->appl_tab);
  symtab_delete(self->type_tab);
  symtab_delete(self->path_tab);
  symtab_delete(self->summ_tab);

  free(self->stype);
  free(self->sappl);
  free(self->spath);

  free(self->grp_app);
  free(self->grp_lib);

  free(self->app_mem);
  free(self->grp_mem);
  free(self->lib_mem);
  free(self->sysest);
  free(self->sysmax);
  free(self->appmax);

}

/* ------------------------------------------------------------------------- *
 * analyze_grp_mem
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_grp_mem(analyze_t *self, int gid, int tid)
{
  assert( 0 <= gid && gid < self->groups );
  assert( 0 <= tid && tid < self->ntypes );
  return &self->grp_mem[tid + gid * self->ntypes];
}

/* ------------------------------------------------------------------------- *
 * analyze_lib_mem
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_lib_mem(analyze_t *self, int lid, int tid)
{
  assert( 0 <= lid && lid < self->npaths );
  assert( 0 <= tid && tid < self->ntypes );
  return &self->lib_mem[tid + lid * self->ntypes];
}

/* ------------------------------------------------------------------------- *
 * analyze_app_mem
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_app_mem(analyze_t *self, int aid, int tid)
{
  assert( 0 <= aid && aid < self->nappls );
  assert( 0 <= tid && tid < self->ntypes );
  return &self->app_mem[tid + aid * self->ntypes];
}

INLINE meminfo_t *
analyze_mem(analyze_t *self, int a, int b, enum emit_type type)
{
  if( type == EMIT_TYPE_LIBRARY)
    return analyze_lib_mem(self, a, b);
  if( type == EMIT_TYPE_APPLICATION)
    return analyze_app_mem(self, a, b);
  assert( 0 );
  return NULL;
}

/* ------------------------------------------------------------------------- *
 * analyze_sysest
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_sysest(analyze_t *self, int tid)
{
  assert( 0 <= tid && tid < self->ntypes );
  return &self->sysest[tid];
}

/* ------------------------------------------------------------------------- *
 * analyze_sysmax
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_sysmax(analyze_t *self, int tid)
{
  assert( 0 <= tid && tid < self->ntypes );
  return &self->sysmax[tid];
}

/* ------------------------------------------------------------------------- *
 * analyze_appmax
 * ------------------------------------------------------------------------- */

INLINE meminfo_t *
analyze_appmax(analyze_t *self, int tid)
{
  assert( 0 <= tid && tid < self->ntypes );
  return &self->appmax[tid];
}

/* ------------------------------------------------------------------------- *
 * analyze_create
 * ------------------------------------------------------------------------- */

analyze_t *
analyze_create(void)
{
  analyze_t *self = calloc(1, sizeof *self);
  analyze_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * analyze_delete
 * ------------------------------------------------------------------------- */

void
analyze_delete(analyze_t *self)
{
  if( self != 0 )
  {
    analyze_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * analyze_delete_cb
 * ------------------------------------------------------------------------- */

void
analyze_delete_cb(void *self)
{
  analyze_delete(self);
}

/* ------------------------------------------------------------------------- *
 * analyze_enumerate_data
 * ------------------------------------------------------------------------- */

static int
local_compare_path(const void *a1, const void *a2)
{
  const smapsmapp_t *m1 = *(const smapsmapp_t **)a1;
  const smapsmapp_t *m2 = *(const smapsmapp_t **)a2;
  return path_compare(m1->smapsmapp_map.path, m2->smapsmapp_map.path);
}

void
analyze_enumerate_data(analyze_t *self, smapssnap_t *snap)
{
  char temp[512];

  /* - - - - - - - - - - - - - - - - - - - *
   * sort process list by: app name & pid
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(&snap->smapssnap_proclist, smapsproc_compare_name_pid_cb);

  /* - - - - - - - - - - - - - - - - - - - *
   * Enumerate:
   * - application names
   * - application instances
   * - mapping types
   *
   * Collect all smaps data to one array
   * while at it.
   * - - - - - - - - - - - - - - - - - - - */

  symtab_enumerate(self->type_tab, "total");
  symtab_enumerate(self->type_tab, "code");
  symtab_enumerate(self->type_tab, "data");
  symtab_enumerate(self->type_tab, "heap");
  symtab_enumerate(self->type_tab, "anon");
  symtab_enumerate(self->type_tab, "stack");

  for( size_t i = 0; i < snap->smapssnap_proclist.size; ++i )
  {
    smapsproc_t *proc = snap->smapssnap_proclist.data[i];

    /* - - - - - - - - - - - - - - - - - - - *
     * application <- app name
     * - - - - - - - - - - - - - - - - - - - */

    // QUARANTINE     proc->smapsproc_AID = symtab_enumerate(self->appl_tab, proc->smapsproc_pid.Name);

    /* - - - - - - - - - - - - - - - - - - - *
     * app instance <- app name + PID
     * - - - - - - - - - - - - - - - - - - - */

    snprintf(temp, sizeof temp, "%s (%d)",
             proc->smapsproc_pid.Name,
             proc->smapsproc_pid.Pid);

    proc->smapsproc_PID = symtab_enumerate(self->appl_tab, temp);

    // FIXME: use PID, not AID
    proc->smapsproc_AID = proc->smapsproc_PID;

    for( size_t k = 0; k < proc->smapsproc_mapplist.size; ++k )
    {
      smapsmapp_t *mapp = proc->smapsproc_mapplist.data[k];

      mapp->smapsmapp_AID = proc->smapsproc_AID;
      mapp->smapsmapp_PID = proc->smapsproc_PID;
      mapp->smapsmapp_TID = symtab_enumerate(self->type_tab, mapp->smapsmapp_map.type);
      array_add(self->mapp_tab, mapp);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * sort smaps data by file path
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(self->mapp_tab, local_compare_path);

  /* - - - - - - - - - - - - - - - - - - - *
   * Enumerate:
   * - mapping paths
   * - application instance + path pairs
   * - - - - - - - - - - - - - - - - - - - */

  for( size_t k = 0; k < self->mapp_tab->size; ++k )
  {
    smapsmapp_t *mapp = self->mapp_tab->data[k];

    /* - - - - - - - - - - - - - - - - - - - *
     * mapping path
     * - - - - - - - - - - - - - - - - - - - */

    mapp->smapsmapp_LID = symtab_enumerate(self->path_tab, mapp->smapsmapp_map.path);

    /* - - - - - - - - - - - - - - - - - - - *
     * application + mapping path
     * - - - - - - - - - - - - - - - - - - - */

    snprintf(temp, sizeof temp, "app%03d::lib%03d",
             mapp->smapsmapp_AID,
             mapp->smapsmapp_LID);
    mapp->smapsmapp_EID = symtab_enumerate(self->summ_tab, temp);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * reverse lookup tables for enums
   * - - - - - - - - - - - - - - - - - - - */

  self->ntypes = self->type_tab->symtab_count;
  self->nappls = self->appl_tab->symtab_count;
  self->npaths = self->path_tab->symtab_count;
  self->groups = self->summ_tab->symtab_count;

  self->stype  = calloc(self->ntypes, sizeof *self->stype);
  self->sappl  = calloc(self->nappls, sizeof *self->sappl);
  self->spath  = calloc(self->npaths, sizeof *self->spath);

  for( int i = 0; i < self->ntypes; ++i )
  {
    symbol_t *s = &self->type_tab->symtab_entry[i];
    self->stype[s->symbol_val] = s->symbol_key;
  }
  for( int i = 0; i < self->nappls; ++i )
  {
    symbol_t *s = &self->appl_tab->symtab_entry[i];
    self->sappl[s->symbol_val] = s->symbol_key;
  }
  for( int i = 0; i < self->npaths; ++i )
  {
    symbol_t *s = &self->path_tab->symtab_entry[i];
    self->spath[s->symbol_val] = s->symbol_key;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * group -> appl and/or path mapping
   * - - - - - - - - - - - - - - - - - - - */

  self->grp_app = calloc(self->groups, sizeof *self->grp_app);
  self->grp_lib = calloc(self->groups, sizeof *self->grp_lib);

  for( int g = 0; g < self->groups; ++g )
  {
    self->grp_app[g] = self->grp_lib[g] = -1;
  }

  for( size_t k = 0; k < self->mapp_tab->size; ++k )
  {
    smapsmapp_t *mapp = self->mapp_tab->data[k];
    int g = mapp->smapsmapp_EID;
    int a = mapp->smapsmapp_AID;
    int p = mapp->smapsmapp_LID;

    // QUARANTINE     printf("G:%d -> A:%d, L:%d\n", g, a, p);

    assert( self->grp_app[g] == -1 || self->grp_app[g] == a );
    assert( self->grp_lib[g] == -1 || self->grp_lib[g] == p );

    self->grp_app[g] = a;
    self->grp_lib[g] = p;
  }

  for( int g = 0; g < self->groups; ++g )
  {
    assert( self->grp_app[g] != -1 );
    assert( self->grp_lib[g] != -1 );
  }
}

/* ------------------------------------------------------------------------- *
 * analyze_accumulate_data
 * ------------------------------------------------------------------------- */

void
analyze_accumulate_data(analyze_t *self)
{
  /* - - - - - - - - - - - - - - - - - - - *
   * allocate accumulation tables
   * - - - - - - - - - - - - - - - - - - - */

  self->grp_mem = calloc(self->groups * self->ntypes, sizeof *self->grp_mem);
  self->app_mem = calloc(self->nappls * self->ntypes, sizeof *self->app_mem);
  self->lib_mem = calloc(self->npaths * self->ntypes, sizeof *self->lib_mem);

  self->sysest  = calloc(self->ntypes, sizeof *self->sysest);
  self->sysmax  = calloc(self->ntypes, sizeof *self->sysmax);
  self->appmax  = calloc(self->ntypes, sizeof *self->appmax);

  /* - - - - - - - - - - - - - - - - - - - *
   * accumulate raw smaps data by
   * process + map path by type grouping
   * - - - - - - - - - - - - - - - - - - - */

  for( size_t k = 0; k < self->mapp_tab->size; ++k )
  {
    smapsmapp_t *mapp = self->mapp_tab->data[k];

    meminfo_t   *srce = &mapp->smapsmapp_mem;

    meminfo_t   *dest = analyze_grp_mem(self,
                                        mapp->smapsmapp_EID,
                                        mapp->smapsmapp_TID);

    meminfo_accumulate_appdata(dest, srce);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * accumulate grouped smaps data to
   * application instance & library
   * - - - - - - - - - - - - - - - - - - - */

  for( int g = 0; g < self->groups; ++g )
  {
    int a = self->grp_app[g];
    int p = self->grp_lib[g];

    // Note: t=0 -> "total"
    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t   *srce = analyze_grp_mem(self, g, t);
      meminfo_t   *dest;

      /* - - - - - - - - - - - - - - - - - - - *
       * process+library/type -> process/type
       * - - - - - - - - - - - - - - - - - - - */

      dest = analyze_app_mem(self, a, t);
      meminfo_accumulate_appdata(dest, srce);

      /* - - - - - - - - - - - - - - - - - - - *
       * process+library/type -> library/type
       * - - - - - - - - - - - - - - - - - - - */

      dest = analyze_lib_mem(self, p, t);
      meminfo_accumulate_libdata(dest, srce);
     }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * application instance totals
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->nappls; ++i )
  {
    meminfo_t *dest = analyze_app_mem(self, i, 0);

    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t *srce = analyze_app_mem(self, i, t);
      meminfo_accumulate_appdata(dest, srce);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * library path totals
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->npaths; ++i )
  {
    meminfo_t *dest = analyze_lib_mem(self, i, 0);

    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t *srce = analyze_lib_mem(self, i, t);
      meminfo_accumulate_appdata(dest, srce);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * application data -> appl estimates
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->nappls; ++i )
  {
    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t *dest, *srce;

      srce = analyze_app_mem(self, i, t);

      dest = analyze_appmax(self, t);
      meminfo_accumulate_maxdata(dest, srce);

      dest = analyze_sysmax(self, t);
      meminfo_accumulate_appdata(dest, srce);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * library data -> system estimates
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; i < self->npaths; ++i )
  {
    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t *dest = analyze_sysest(self, t);
      meminfo_t *srce = analyze_lib_mem(self, i, t);
      meminfo_accumulate_appdata(dest, srce);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * system estimate totals
   * - - - - - - - - - - - - - - - - - - - */

  for( int t = 1; t < self->ntypes; ++t )
  {
    meminfo_t *dest, *srce;

    srce = analyze_sysest(self, t);
    dest = analyze_sysest(self, 0);
    meminfo_accumulate_appdata(dest, srce);

    srce = analyze_sysmax(self, t);
    dest = analyze_sysmax(self, 0);
    meminfo_accumulate_appdata(dest, srce);

    srce = analyze_appmax(self, t);
    dest = analyze_appmax(self, 0);
    meminfo_accumulate_appdata(dest, srce);
  }
}

static void
analyze_html_header(FILE *file, const char *title, const char *work)
{
  fprintf(file, "<html>\n");
  fprintf(file, "<head>\n");
  fprintf(file, "<title>%s</title>\n", title);
  fprintf(file, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s/tablesorter.css\" />\n", work);
  fprintf(file, "<style type='text/css'>\n");
  fprintf(file, "  table.tablesorter thead tr .header"
                "  { background-image: url(%s/bg.gif);"
                "    background-repeat: no-repeat;"
                "    background-position: center right; }\n", work);
  fprintf(file, "  table.tablesorter thead tr .headerSortUp"
                "  { background-image: url(%s/asc.gif); }\n", work);
  fprintf(file, "  table.tablesorter thead tr .headerSortDown"
                "  { background-image: url(%s/desc.gif); }\n", work);
  fprintf(file, "</style>\n");
  fprintf(file, "</head>\n");
  fprintf(file, "<body>\n");
  fprintf(file, "<script src=\"%s/jquery.min.js\"></script>\n", work);
  fprintf(file, "<script src=\"%s/jquery.metadata.js\"></script>\n", work);
  fprintf(file, "<script src=\"%s/jquery.tablesorter.js\"></script>\n", work);
  fprintf(file, "<script src=\"%s/expander.js\"></script>\n", work);
  fprintf(file, "<script>$(document).ready(function() "
                "{ $(\".tablesorter\").tablesorter(); } );</script>\n");
}

#define TP " bgcolor=\"#ffffbf\" "
#define LT " bgcolor=\"#bfffff\" "
#define D1 " bgcolor=\"#f4f4f4\" "
#define D2 " bgcolor=\"#ffffff\" "

static const char *const emit_type_titles[] = {
  [EMIT_TYPE_LIBRARY]         = "Library",
  [EMIT_TYPE_APPLICATION]     = "Application",
  [EMIT_TYPE_OBJECT]          = "Object",
};

/* ------------------------------------------------------------------------- *
 * analyze_emit_page_table
 * ------------------------------------------------------------------------- */

static void
analyze_emit_page_table(analyze_t *self, FILE *file, const meminfo_t *mtab, const pidinfo_t *pidinfo)
{
  fprintf(file, "<table border=1>\n");
  fprintf(file, "<tr>\n");
  fprintf(file, "<th rowspan=2>\n");
  fprintf(file, "<th"TP"colspan=2>%s\n", "Dirty");
  fprintf(file, "<th"TP"colspan=2>%s\n", "Clean");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Resident");
  if (pidinfo)
  {
    fprintf(file, "<th"TP"rowspan=2>%s\n",
        "<abbr title='VmHWM field of /proc/pid/status'>Resident Peak</abbr>");
  }
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Size");
  if (pidinfo)
  {
    fprintf(file, "<th"TP"rowspan=2>%s\n",
        "<abbr title='VmPeak field of /proc/pid/status'>Size Peak</abbr>");
  }
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Pss");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Swap");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Referenced");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Anonymous");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Locked");

  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP">%s\n", "Private");
  fprintf(file, "<th"TP">%s\n", "Shared");
  fprintf(file, "<th"TP">%s\n", "Private");
  fprintf(file, "<th"TP">%s\n", "Shared");

  for( int t = 0; t < self->ntypes; ++t )
  {
    const meminfo_t *m = &mtab[t];
    const char *bg = ((t/3)&1) ? D1 : D2;

    fprintf(file, "<tr>\n");
    fprintf(file, "<th"LT" align=left>%s\n", self->stype[t]);
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Private_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Shared_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Private_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Shared_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Rss));
    if (pidinfo)
    {
      fprintf(file, "<td %s align=right>%s\n", bg,
          uval(t==0 ? pidinfo->VmHWM : 0));
    }
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Size));
    if (pidinfo)
    {
      fprintf(file, "<td %s align=right>%s\n", bg,
          uval(t==0 ? pidinfo->VmPeak : 0));
    }
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Pss));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Swap));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Referenced));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Anonymous));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Locked));
  }
  fprintf(file, "</table>\n");
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_xref_header
 * ------------------------------------------------------------------------- */

static void
analyze_emit_xref_header(const analyze_t *self, FILE *file, enum emit_type type)
{
  fprintf(file, "<thead>\n");
  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP">%s\n", emit_type_titles[type]);
  fprintf(file, "<th"TP">%s\n", "Type");
  fprintf(file, "<th"TP">%s\n", "Prot");
  fprintf(file, "<th"TP">%s\n", "Size");
  fprintf(file, "<th"TP">%s\n", "Rss");
  fprintf(file, "<th"TP">%s\n", "Dirty<br>Private");
  fprintf(file, "<th"TP">%s\n", "Dirty<br>Shared");
  fprintf(file, "<th"TP">%s\n", "Clean<br>Private");
  fprintf(file, "<th"TP">%s\n", "Clean<br>Shared");
  fprintf(file, "<th"TP">%s\n", "Pss");
  fprintf(file, "<th"TP">%s\n", "Swap");
  fprintf(file, "<th"TP">%s\n", "Anonymous");
  fprintf(file, "<th"TP">%s\n", "Locked");
}

/* ------------------------------------------------------------------------- *
 * analyze_get_apprange
 * ------------------------------------------------------------------------- */

static int cmp_app_aid;
static int
cmp_app(const void *p)
{
  const smapsmapp_t *m = p;
  return m->smapsmapp_AID - cmp_app_aid;
}

void
analyze_get_apprange(analyze_t *self,int lo, int hi, int *plo, int *phi, int aid)
{
  cmp_app_aid = aid;
  *plo = array_find_lower(self->mapp_tab, lo, hi, cmp_app);
  *phi = array_find_upper(self->mapp_tab, lo, hi, cmp_app);
}

/* ------------------------------------------------------------------------- *
 * analyze_get_librange
 * ------------------------------------------------------------------------- */

static int cmp_lib_lid;
static int
cmp_lib(const void *p)
{
  const smapsmapp_t *m = p;
  return m->smapsmapp_LID - cmp_lib_lid;
}

void
analyze_get_librange(analyze_t *self,int lo, int hi, int *plo, int *phi, int lid)
{
  cmp_lib_lid = lid;
  *plo = array_find_lower(self->mapp_tab, lo, hi, cmp_lib);
  *phi = array_find_upper(self->mapp_tab, lo, hi, cmp_lib);
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_lib_html
 * ------------------------------------------------------------------------- */

static int
local_lib_app_compare(const void *a1, const void *a2)
{
  const smapsmapp_t *m1 = *(const smapsmapp_t **)a1;
  const smapsmapp_t *m2 = *(const smapsmapp_t **)a2;

  int r;

  /* - - - - - - - - - - - - - - - - - - - *
   * primary sorting: must be LID then AID
   * for the range searching below to work!
   * - - - - - - - - - - - - - - - - - - - */

  if( (r = m1->smapsmapp_LID - m2->smapsmapp_LID) != 0 ) return r;
  if( (r = m1->smapsmapp_AID - m2->smapsmapp_AID) != 0 ) return r;

  /* - - - - - - - - - - - - - - - - - - - *
   * secondary sorting: convenience order
   * - - - - - - - - - - - - - - - - - - - */

  if( (r = m1->smapsmapp_TID - m2->smapsmapp_TID) != 0 ) return r;
  if( (r = m2->smapsmapp_mem.Rss - m1->smapsmapp_mem.Rss) ) return r;

  return 0;
}

int
analyze_emit_lib_html(analyze_t *self, smapssnap_t *snap, const char *work)
{
  int   error = -1;
  FILE *file  = 0;

  char  temp[512];

  /* - - - - - - - - - - - - - - - - - - - *
   * sort smaps data to bsearchable order
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(self->mapp_tab, local_lib_app_compare);

  /* - - - - - - - - - - - - - - - - - - - *
   * write html page for each library
   * - - - - - - - - - - - - - - - - - - - */

  for( int l = 0; l < self->npaths; ++l )
  {
    smapsmapp_t *m; int t,a;

    /* - - - - - - - - - - - - - - - - - - - *
     * open output file
     * - - - - - - - - - - - - - - - - - - - */

    snprintf(temp, sizeof temp, "%s/lib%03d.html", work, l);
    //printf(">> %s\n", temp);

    if( (file = fopen(temp, "w")) == 0 )
    {
      goto cleanup;
    }

    analyze_html_header(file, path_basename(self->spath[l]), ".");

    /* - - - - - - - - - - - - - - - - - - - *
     * summary table
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "<h1>%s: %s</h1>\n", emit_type_titles[EMIT_TYPE_LIBRARY], self->spath[l]);
    analyze_emit_page_table(self, file, analyze_lib_mem(self, l, 0), NULL);

    /* - - - - - - - - - - - - - - - - - - - *
     * application xref
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "<h1>%s XREF</h1>\n", emit_type_titles[EMIT_TYPE_APPLICATION]);
    /* Sort initially by 9th column (PSS) in descending order */
    fprintf(file, "<table border=1 class=\"tablesorter { sortlist: [[9,0]] }\">\n");
    analyze_emit_xref_header(self, file, EMIT_TYPE_APPLICATION);
    fprintf(file, "<tbody>\n");

    int alo,ahi, blo,bhi;

    analyze_get_librange(self, 0, self->mapp_tab->size, &alo, &ahi, l);

    for( ; alo < ahi; alo = bhi )
    {
      m = self->mapp_tab->data[alo];
      a = m->smapsmapp_AID;

      analyze_get_apprange(self, alo,ahi,&blo,&bhi,a);

      for( size_t i = blo; i < bhi; ++i )
      {
        m = self->mapp_tab->data[i];
        t = m->smapsmapp_TID;

        fprintf(file,
                "<tr>\n"
                "<th"LT"align=left>"
                "<a href=\"app%03d.html\">%s</a>\n",
                a, abbr_title(path_basename(self->sappl[a])));

        fprintf(file, "<td align=left>%s\n", m->smapsmapp_map.type);
        fprintf(file, "<td align=left style='font-family: monospace;'>%s\n", m->smapsmapp_map.prot);
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Size));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Rss));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Private_Dirty));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Shared_Dirty));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Private_Clean));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Shared_Clean));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Pss));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Swap));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Anonymous));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Locked));
      }
    }

    fprintf(file, "</table>\n");

    /* - - - - - - - - - - - - - - - - - - - *
     * html footer
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");

    fclose(file); file = 0;
  }
  error = 0;
  cleanup:

  if( file != 0 )
  {
    fclose(file);
  }

  return error;
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_app_html
 * ------------------------------------------------------------------------- */

static const pidinfo_t *
pidinfo_from_smapssnap(const smapssnap_t *snap, const char *sappl)
{
  char temp[512];
  size_t i;
  for (i=0; i < array_size(&snap->smapssnap_proclist); ++i)
  {
    smapsproc_t *proc = array_get(&snap->smapssnap_proclist, i);
    if (!proc)
      continue;
    snprintf(temp, sizeof temp, "%s (%d)",
             proc->smapsproc_pid.Name,
             proc->smapsproc_pid.Pid);
    if (strcmp(temp, sappl) == 0)
      return &proc->smapsproc_pid;
  }
  return NULL;
}

static int
local_app_lib_compare(const void *a1, const void *a2)
{
  const smapsmapp_t *m1 = *(const smapsmapp_t **)a1;
  const smapsmapp_t *m2 = *(const smapsmapp_t **)a2;

  int r;

  /* - - - - - - - - - - - - - - - - - - - *
   * primary sorting: must be AID then LID
   * for the range searching below to work!
   * - - - - - - - - - - - - - - - - - - - */

  if( (r = m1->smapsmapp_AID - m2->smapsmapp_AID) != 0 ) return r;
  if( (r = m1->smapsmapp_LID - m2->smapsmapp_LID) != 0 ) return r;

  /* - - - - - - - - - - - - - - - - - - - *
   * secondary sorting: convenience order
   * - - - - - - - - - - - - - - - - - - - */

  if( (r = m1->smapsmapp_TID - m2->smapsmapp_TID) != 0 ) return r;
  if( (r = m2->smapsmapp_mem.Rss - m1->smapsmapp_mem.Rss) ) return r;

  return 0;
}

int
analyze_emit_app_html(analyze_t *self, smapssnap_t *snap, const char *work)
{
  int error = -1;
  char temp[512];
  FILE *file = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * sort smaps data to bsearchable order
   * - - - - - - - - - - - - - - - - - - - */

  array_sort(self->mapp_tab, local_app_lib_compare);

  /* - - - - - - - - - - - - - - - - - - - *
   * write html page for each application
   * - - - - - - - - - - - - - - - - - - - */

  for( int a = 0; a < self->nappls; ++a )
  {
    smapsmapp_t *m; int t,l;

    /* - - - - - - - - - - - - - - - - - - - *
     * open file
     * - - - - - - - - - - - - - - - - - - - */

    snprintf(temp, sizeof temp, "%s/app%03d.html", work, a);
    //printf(">> %s\n", temp);
    if( (file = fopen(temp, "w")) == 0 )
    {
      goto cleanup;
    }

    analyze_html_header(file, self->sappl[a], ".");

    /* - - - - - - - - - - - - - - - - - - - *
     * summary table
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "<h1>%s: %s</h1>\n", emit_type_titles[EMIT_TYPE_APPLICATION], self->sappl[a]);
    analyze_emit_page_table(self, file, analyze_app_mem(self, a, 0),
        pidinfo_from_smapssnap(snap, self->sappl[a]));

    /* - - - - - - - - - - - - - - - - - - - *
     * library xref
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "<h1>%s XREF</h1>\n", "Mapping");
    /* Sort initially by 9th column (PSS) in descending order */
    fprintf(file, "<table border=1 class=\"tablesorter { sortlist: [[9,0]] }\">\n");
    analyze_emit_xref_header(self, file, EMIT_TYPE_OBJECT);
    fprintf(file, "<tbody>\n");

    int alo,ahi, blo,bhi;

    analyze_get_apprange(self, 0, self->mapp_tab->size, &alo, &ahi, a);

    for( ; alo < ahi; alo = bhi )
    {
      m = self->mapp_tab->data[alo];
      l = m->smapsmapp_LID;

      analyze_get_librange(self, alo,ahi,&blo,&bhi,l);

      for( size_t i = blo; i < bhi; ++i )
      {
        m = self->mapp_tab->data[i];
        t = m->smapsmapp_TID;

        fprintf(file,
                "<tr>\n"
                "<th"LT"align=left>"
                "<a href=\"lib%03d.html\">%s</a>\n",
                l, abbr_title(path_basename(self->spath[l])));

        fprintf(file, "<td align=left>%s\n", m->smapsmapp_map.type);
        fprintf(file, "<td align=left style='font-family: monospace;'>%s\n", m->smapsmapp_map.prot);
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Size));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Rss));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Private_Dirty));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Shared_Dirty));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Private_Clean));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Shared_Clean));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Pss));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Swap));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Anonymous));
        fprintf(file, "<td align=right>%s\n", uval(m->smapsmapp_mem.Locked));
      }
    }

    fprintf(file, "</table>\n");

    /* - - - - - - - - - - - - - - - - - - - *
     * html footer
     * - - - - - - - - - - - - - - - - - - - */

    fprintf(file, "</body>\n");
    fprintf(file, "</html>\n");
    fclose(file); file = 0;
  }

  error = 0;
  cleanup:

  if( file != 0 )
  {
    fclose(file);
  }

  return error;
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_smaps_table
 * ------------------------------------------------------------------------- */

void
analyze_emit_smaps_table(analyze_t *self, FILE *file, meminfo_t *v)
{
  fprintf(file, "<table border=1>\n");
  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Class");
  fprintf(file, "<th"TP"colspan=2>%s\n", "Dirty");
  fprintf(file, "<th"TP"colspan=2>%s\n", "Clean");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Resident");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Size");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Pss");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Swap");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Referenced");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Anonymous");
  fprintf(file, "<th"TP"rowspan=2>%s\n", "Locked");

  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP">%s\n", "Private");
  fprintf(file, "<th"TP">%s\n", "Shared");
  fprintf(file, "<th"TP">%s\n", "Private");
  fprintf(file, "<th"TP">%s\n", "Shared");

  for( int t = 0; t < self->ntypes; ++t )
  {
    meminfo_t *m = &v[t];//&app_mem[a][t];
    const char *bg = ((t/3)&1) ? D1 : D2;

    fprintf(file, "<tr>\n");
    fprintf(file, "<th"LT" align=left>%s\n", self->stype[t]);
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Private_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Shared_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Private_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Shared_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Rss));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Size));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Pss));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Swap));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Referenced));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Anonymous));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(m->Locked));
  }

  fprintf(file, "</table>\n");
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_table_header
 * ------------------------------------------------------------------------- */

#define VM_COLUMN_COUNT 5
static const char *const virtual_memory_column_titles[][VM_COLUMN_COUNT] = {
  [EMIT_TYPE_LIBRARY] = {
    "<abbr title=\"Largest value\"><i>RSS</i></abbr>",
    "<abbr title=\"Largest value\"><i>Size</i></abbr>",
    "<abbr title=\"Sum of values\">PSS</abbr> ",
    "<abbr title=\"Largest value\"><i>Swap</i></abbr>",
    "<abbr title=\"Sum of values\">Locked</abbr>",
  },
  [EMIT_TYPE_APPLICATION] = {
    "RSS",
    "Size",
    "PSS ",
    "Swap",
    "Locked",
  },
};

static void
analyze_emit_table_header(const analyze_t *self, FILE *file, enum emit_type type)
{
  fprintf(file, "<thead>\n");
  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP" rowspan=3>%s\n", emit_type_titles[type]);
  fprintf(file, "<th"TP" colspan=4>%s\n", "RSS / Status");
  fprintf(file, "<th"TP" rowspan=2 colspan=%d>%s\n", VM_COLUMN_COUNT, "Virtual<br>Memory");
  if( type == EMIT_TYPE_APPLICATION )
  {
    fprintf(file, "<th"TP" colspan=%d>%s\n", self->ntypes-1, "RSS / Class");
    fprintf(file, "<th"TP" colspan=%d>%s\n", self->ntypes-1, "Size / Class");
  }
  fprintf(file, "<tr>\n");
  fprintf(file, "<th"TP" colspan=2>%s\n", "Dirty");
  fprintf(file, "<th"TP" colspan=2>%s\n", "Clean");
  if( type == EMIT_TYPE_APPLICATION )
  {
    for( int i = 1; i < self->ntypes; ++i )
    {
      fprintf(file, "<th"TP" rowspan=2>%s\n", self->stype[i]);
    }
    for( int i = 1; i < self->ntypes; ++i )
    {
      fprintf(file, "<th"TP" rowspan=2>%s\n", self->stype[i]);
    }
  }
  fprintf(file, "<tr>\n");
  if( type == EMIT_TYPE_LIBRARY )
  {
    fprintf(file, "<th"TP"><abbr title=\"Sum of values\">Private</abbr>\n");
    fprintf(file, "<th"TP"><abbr title=\"Largest value\"><i>Shared</i></abbr>\n");
    fprintf(file, "<th"TP"><abbr title=\"Sum of values\">Private</abbr>\n");
    fprintf(file, "<th"TP"><abbr title=\"Largest value\"><i>Shared</i></abbr>\n");
  }
  else if( type == EMIT_TYPE_APPLICATION )
  {
    fprintf(file, "<th"TP">Private\n");
    fprintf(file, "<th"TP">Shared\n");
    fprintf(file, "<th"TP">Private\n");
    fprintf(file, "<th"TP">Shared\n");
  }
  for( int i=0; i < sizeof(virtual_memory_column_titles[0])/sizeof(char *); ++i )
  {
    fprintf(file, "<th"TP">%s\n", virtual_memory_column_titles[type][i]);
  }
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_process_hierarchy  --  recursively dump clickable process tree
 * ------------------------------------------------------------------------- */

void
analyze_emit_process_hierarchy(analyze_t *self, FILE *file, smapsproc_t *proc,
                               const char *work, int recursion_depth)
{
  if( proc->smapsproc_children.size )
  {
    fprintf(file, "<ul id='children_of_%d' %s>\n",
	proc->smapsproc_AID,
	recursion_depth == 1 ? "style='display:none;'" : "");
    for( int i = 0; i < proc->smapsproc_children.size; ++i )
    {
      smapsproc_t *sub = proc->smapsproc_children.data[i];

      fprintf(file, "<li><a href=\"%s/app%03d.html\">%s (%d)</a>\n",
              work,
              sub->smapsproc_AID,
              sub->smapsproc_pid.Name,
              sub->smapsproc_pid.Pid);

      if (recursion_depth == 0)
      {
	fprintf(file, "<span style=\"text-decoration: underline; "
	    "cursor: pointer;\" "
	    "onClick=\"toggleBlockText('children_of_%d', "
	    "this, '(expand)','(collapse)');\">(expand)</span>\n",
	    sub->smapsproc_AID);
      }

      analyze_emit_process_hierarchy(self, file, sub, work, recursion_depth+1);
    }
    fprintf(file, "</ul>\n");
  }
}

static int
analyze_emit_application_table_cmp(const void *a1, const void *a2)
{
  analyze_t *self = qsort_cmp_data;
  const meminfo_t *m1 = analyze_app_mem(self, *(const int *)a1, 0);
  const meminfo_t *m2 = analyze_app_mem(self, *(const int *)a2, 0);

  int r;
  if( (r = m2->Pss           - m1->Pss) != 0 ) return r;
  if( (r = m2->Private_Dirty - m1->Private_Dirty) != 0 ) return r;
  if( (r = m2->Shared_Dirty  - m1->Shared_Dirty) != 0 ) return r;
  if( (r = m2->Rss           - m1->Rss) != 0 ) return r;

  return m2->Size - m1->Size;
}

static int
analyze_emit_library_table_cmp(const void *a1, const void *a2)
{
  analyze_t *self = qsort_cmp_data;
  const meminfo_t *m1 = analyze_lib_mem(self, *(const int *)a1, 0);
  const meminfo_t *m2 = analyze_lib_mem(self, *(const int *)a2, 0);
  int r;
  if( (r = m2->Pss           - m1->Pss) != 0 ) return r;
  if( (r = m2->Private_Dirty - m1->Private_Dirty) != 0 ) return r;
  if( (r = m2->Shared_Dirty  - m1->Shared_Dirty) != 0 ) return r;
  if( (r = m2->Rss           - m1->Rss) != 0 ) return r;

  return m2->Size - m1->Size;
}

static void
analyze_emit_table(analyze_t *self, FILE *file, const char *work, enum emit_type type)
{
  int omitted_lines = 0;
  int items = 0;
  if( type == EMIT_TYPE_LIBRARY )
    items = self->npaths;
  else if( type == EMIT_TYPE_APPLICATION )
    items = self->nappls;
  int lut[items];

  for( int i = 0; i < items; ++i )
  {
    lut[i] = i;
  }

  qsort_cmp_data = self;
  if( type == EMIT_TYPE_LIBRARY )
    qsort(lut, items, sizeof *lut, analyze_emit_library_table_cmp);
  else if( type == EMIT_TYPE_APPLICATION )
    qsort(lut, items, sizeof *lut, analyze_emit_application_table_cmp);

  /* Sort initially by 7th column (PSS) in descending order */
  fprintf(file, "<table border=1 class=\"tablesorter { sortlist: [[7,0]] }\">\n");
  analyze_emit_table_header(self, file, type);
  fprintf(file, "<tbody>\n");
  for( int i = 0; i < items; ++i )
  {
    int a = lut[i];
    const char *title = NULL;
    const char *bg = ((i/3)&1) ? D1 : D2;
    meminfo_t *s = analyze_mem(self, a, 0, type);

    /* One-page mappings are not interesting, prune them from the Object Values
     * table.
     */
    if (type == EMIT_TYPE_LIBRARY && s->Size <= 4)
    {
      ++omitted_lines;
      continue;
    }
    else if (type == EMIT_TYPE_APPLICATION && meminfo_all_zeroes(s))
    {
      ++omitted_lines;
      continue;
    }

    fprintf(file, "<tr>\n");
    fprintf(file, "<th bgcolor=\"#bfffff\" align=left>");

    if( type == EMIT_TYPE_LIBRARY )
    {
      title = path_basename(self->spath[a]);
      fprintf(file, "<a href=\"%s/lib%03d.html\">%s</a>\n",
          work, a, abbr_title(title));
    }
    else if( type == EMIT_TYPE_APPLICATION )
    {
      title = self->sappl[a];
      fprintf(file, "<a href=\"%s/app%03d.html\">%s</a>\n",
          work, a, abbr_title(title));
    }
    else
    {
      abort();
    }

    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Private_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Shared_Dirty));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Private_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Shared_Clean));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Rss));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Size));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Pss));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Swap));
    fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Locked));

    if (type == EMIT_TYPE_APPLICATION)
    {
      for( int t = 1; t < self->ntypes; ++t )
      {
	meminfo_t *s = analyze_mem(self, a, t, type);
	fprintf(file, "<td %s align=right>%s\n", bg, uval(meminfo_total(s)));
      }
      for( int t = 1; t < self->ntypes; ++t )
      {
	meminfo_t *s = analyze_mem(self, a, t, type);
	fprintf(file, "<td %s align=right>%s\n", bg, uval(s->Size));
      }
    }
  }
  fprintf(file, "</table>\n");
  if (omitted_lines)
  {
    if (type == EMIT_TYPE_LIBRARY)
    {
      fprintf(file,
	"<b>Note:</b> removed %d entries from the table with <i>Size</i> of "
	"at most 4 kilobytes.\n",
	omitted_lines);
    }
    else if (type == EMIT_TYPE_APPLICATION)
    {
      fprintf(file,
        "<b style='color:red'>Note:</b> removed %d applications from the table "
        "that have all entries set to zero. This may indicate that the smaps "
        "capture is incomplete.\n",
	omitted_lines);
    }
    else
    {
      abort();
    }
  }
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_main_page
 * ------------------------------------------------------------------------- */

static int
file_copy(const char *src, const char *dst)
{
  char buf[4096];
  int ret;
  FILE *fin, *fout;
  size_t got, wrote;
  fin = NULL;
  fout = NULL;
  ret = -1;
  fin = fopen(src, "r");
  if (!fin)
  {
    fprintf(stderr, "ERROR: unable to open '%s' for reading: %s\n",
        src, strerror(errno));
    goto out;
  }
  fout = fopen(dst, "w");
  if (!fout)
  {
    fprintf(stderr, "ERROR: unable to open '%s' for writing: %s\n",
        dst, strerror(errno));
    goto out;
  }
  while (!feof(fin))
  {
    if (ferror(fin) || ferror(fout))
      goto out;
    got = fread(buf, 1, sizeof(buf), fin);
    while (got)
    {
      wrote = fwrite(buf, 1, got, fout);
      if (ferror(fout))
        goto out;
      got -= wrote;
    }
  }
  ret = 0;
out:
  if (fin)
    fclose(fin);
  if (fout)
  {
    fclose(fout);
    if (ret < 0)
    {
      unlink(dst);
    }
  }
  return ret;
}

static int
copy_to_workdir(const char *workdir, const char *fn)
{
  char src[PATH_MAX];
  char dst[PATH_MAX];
  snprintf(src, sizeof(src), "/usr/share/sp-smaps-visualize/%s", fn);
  src[sizeof(src)-1] = 0;
  snprintf(dst, sizeof(dst), "%s/%s", workdir, fn);
  dst[sizeof(dst)-1] = 0;
  return file_copy(src, dst);
}

static const char *const html_resources[] =
{
  "jquery.metadata.js",
  "jquery.min.js",
  "jquery.tablesorter.js",
  "tablesorter.css",
  "expander.js",
  "asc.gif",
  "desc.gif",
  "bg.gif",
};

static int
create_javascript_files(const char *workdir)
{
  size_t i;
  int ret;
  for (i=0; i < sizeof(html_resources)/sizeof(*html_resources); ++i)
  {
    ret = copy_to_workdir(workdir, html_resources[i]);
    if (ret < 0)
      goto out;
  }
out:
  return ret;
}

static void *
_array_remove_elem(array_t *self, const void *elem)
{
  size_t i;
  for (i=0; i < array_size(self); ++i)
  {
    if (array_get(self, i) == elem)
    {
      return array_rem(self, i);
    }
  }
  return NULL;
}

static void
analyze_prune_kthreads(smapssnap_t *snap)
{
  size_t i, j;
  smapsproc_t *root = &snap->smapssnap_rootproc;
  for (i=0; i < root->smapsproc_children.size; ++i)
  {
    smapsproc_t *kthreadd = array_get(&root->smapsproc_children, i);
    if (!kthreadd)
      continue;
    const char *subname = kthreadd->smapsproc_pid.Name;
    if (subname && strcmp(subname, "kthreadd") == 0)
    {
      const size_t kthread_cnt = array_size(&kthreadd->smapsproc_children);
      for (j=0; j < kthread_cnt; ++j)
      {
	smapsproc_t *kthread = array_get(&kthreadd->smapsproc_children, j);
	if (!kthread)
	  continue;
	_array_remove_elem(&snap->smapssnap_proclist, kthread);
	smapsproc_delete(kthread);
      }
      array_clear(&kthreadd->smapsproc_children);
      _array_remove_elem(&snap->smapssnap_proclist, kthreadd);
      _array_remove_elem(&snap->smapssnap_rootproc.smapsproc_children, kthreadd);
      smapsproc_delete(kthreadd);
      break;
    }
  }
}

int
analyze_emit_main_page(analyze_t *self, smapssnap_t *snap, const char *path)
{
  int       error = -1;
  FILE     *file  = 0;

  char      work[512];

  /* - - - - - - - - - - - - - - - - - - - *
   * make sure we have directory for
   * library & application specific pages
   * - - - - - - - - - - - - - - - - - - - */

  snprintf(work, sizeof work - 4, "%s", path);
  strcpy(path_extension(work), ".dir");

  if( !path_isdir(work) )
  {
    if( mkdir(work, 0777) != 0 )
    {
      perror(work); goto cleanup;
    }
  }

  if( create_javascript_files(work) < 0 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * open output file
   * - - - - - - - - - - - - - - - - - - - */

  if( (file = fopen(path, "w")) == 0 )
  {
    perror(path); goto cleanup;
  }

  analyze_html_header(file, smapssnap_get_source(snap), work);

  /* - - - - - - - - - - - - - - - - - - - *
   * memory usage tables
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "<a href=\"#system_estimates\">System Estimates</a> | ");
  fprintf(file, "<a href=\"#process_hierarchy\">Process Hierarchy</a> | ");
  fprintf(file, "<a href=\"#application_values\">Application Values</a> | ");
  fprintf(file, "<a href=\"#object_values\">Object Values</a>\n");

  fprintf(file, "<a name=\"system_estimates\"><h1>System Estimates</h1></a>\n");

  fprintf(file, "<h2>System: Memory Use Estimate</h2>\n");
  analyze_emit_smaps_table(self, file, self->sysest);
  fprintf(file, "<p>Private and Size are accurate, the rest are minimums.\n");

  fprintf(file, "<h2>System: Memory Use App Totals</h2>\n");
  analyze_emit_smaps_table(self, file, self->sysmax);
  fprintf(file, "<p>Private is accurate, the rest are maximums.\n");

  fprintf(file, "<h2>System: Memory Use App Maximums</h2>\n");
  analyze_emit_smaps_table(self, file, self->appmax);
  fprintf(file, "<p>No process has values larger than the ones listed above.\n");

  /* - - - - - - - - - - - - - - - - - - - *
   * process hierarchy tree
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "<a name=\"process_hierarchy\"><h1>Process Hierarchy</h1></a>\n");
  analyze_emit_process_hierarchy(self, file, &snap->smapssnap_rootproc, work, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * application table
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "<a name=\"application_values\"><h1>Application Values</h1></a>\n");
  analyze_emit_table(self, file, work, EMIT_TYPE_APPLICATION);

  /* - - - - - - - - - - - - - - - - - - - *
   * library table
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "<a name=\"object_values\"><h1>Object Values</h1></a>\n");
  analyze_emit_table(self, file, work, EMIT_TYPE_LIBRARY);

  /* - - - - - - - - - - - - - - - - - - - *
   * html trailer
   * - - - - - - - - - - - - - - - - - - - */

  fprintf(file, "</body>\n");
  fprintf(file, "</html>\n");
  fclose(file); file = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * application pages
   * - - - - - - - - - - - - - - - - - - - */

  if( analyze_emit_app_html(self, snap, work) )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * library pages
   * - - - - - - - - - - - - - - - - - - - */

  if( analyze_emit_lib_html(self, snap, work) )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  error = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  if( file )
  {
    fclose(file);
  }
  return error;
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_appval_table
 * ------------------------------------------------------------------------- */

static int
analyze_emit_appval_table_cmp(const void *a1, const void *a2)
{
  analyze_t *self = qsort_cmp_data;

  typedef struct { int id; smapsproc_t *pt; } lut_t;
  const lut_t *l1 = a1;
  const lut_t *l2 = a2;

  const meminfo_t *m1 = analyze_app_mem(self, l1->id, 0);
  const meminfo_t *m2 = analyze_app_mem(self, l2->id, 0);

  int r;
  if( (r = m2->Pss           - m1->Pss) != 0 ) return r;
  if( (r = m2->Private_Dirty - m1->Private_Dirty) != 0 ) return r;
  if( (r = m2->Shared_Dirty  - m1->Shared_Dirty) != 0 ) return r;
  if( (r = m2->Rss           - m1->Rss) != 0 ) return r;

  if( (r = m2->Size - m1->Size) != 0 ) return r;

  return l1->pt->smapsproc_AID - l2->pt->smapsproc_AID;
}

void
analyze_emit_appval_table(analyze_t *self, smapssnap_t *snap, FILE *file)
{
  typedef struct { int id; smapsproc_t *pt; } lut_t;
  lut_t lut[self->nappls];

  for( int i = 0; i < self->nappls; ++i )
  {
    lut[i].id = i;
    lut[i].pt = 0;
  }

  for( int i = 0; i < snap->smapssnap_proclist.size; ++i )
  {
    smapsproc_t *proc = snap->smapssnap_proclist.data[i];
    int a = proc->smapsproc_AID;
    assert( 0 <= a && a < self->nappls );

    assert( lut[a].id == a );
    assert( lut[a].pt == 0 );
    lut[a].pt = proc;
  }
  for( int i = 0; i < self->nappls; ++i )
  {
    assert( lut[i].pt != 0 );
  }

  qsort_cmp_data = self;
  qsort(lut, self->nappls, sizeof *lut, analyze_emit_appval_table_cmp);

  fprintf(file, "generator = %s %s\n", TOOL_NAME, TOOL_VERS);
  fprintf(file, "\n");
  fprintf(file, "name,pid,ppid,threads,pri,sha,cln,rss,size,rss,pss,swap,referenced");
  for( int t = 1; t < self->ntypes; ++t )
  {
    fprintf(file, ",%s", self->stype[t]);
  }
  fprintf(file, "\n");

  for( int i = 0; i < self->nappls; ++i )
  {
    int a = lut[i].id;
    smapsproc_t *proc = lut[i].pt;

    fprintf(file, "%s", proc->smapsproc_pid.Name);
    fprintf(file, ",%d", proc->smapsproc_pid.Pid);
    fprintf(file, ",%d", proc->smapsproc_pid.PPid);
    fprintf(file, ",%d", proc->smapsproc_pid.Threads);

    meminfo_t *s = analyze_app_mem(self, a, 0);

    fprintf(file, ",%u", s->Private_Dirty);
    fprintf(file, ",%u", s->Shared_Dirty);
    fprintf(file, ",%u", s->Private_Clean + s->Shared_Clean);
    fprintf(file, ",%u", s->Rss);
    fprintf(file, ",%u", s->Size);
    fprintf(file, ",%u", s->Pss);
    fprintf(file, ",%u", s->Swap);
    fprintf(file, ",%u", s->Referenced);

    for( int t = 1; t < self->ntypes; ++t )
    {
      meminfo_t *s = analyze_app_mem(self, a, t);
      fprintf(file, ",%u", meminfo_total(s));
    }
    fprintf(file, "\n");
  }
}

/* ------------------------------------------------------------------------- *
 * analyze_emit_appvals
 * ------------------------------------------------------------------------- */

int
analyze_emit_appvals(analyze_t *self, smapssnap_t *snap, const char *path)
{
  int       error = -1;
  FILE     *file  = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * open output file
   * - - - - - - - - - - - - - - - - - - - */

  if( (file = fopen(path, "w")) == 0 )
  {
    perror(path); goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * html header
   * - - - - - - - - - - - - - - - - - - - */

// QUARANTINE   fprintf(file, "<html>\n");
// QUARANTINE   fprintf(file, "<head>\n");
// QUARANTINE   fprintf(file, "<title>%s</title>\n", smapssnap_get_source(snap));
// QUARANTINE   fprintf(file, "</head>\n");
// QUARANTINE   fprintf(file, "<body>\n");

  /* - - - - - - - - - - - - - - - - - - - *
   * application table
   * - - - - - - - - - - - - - - - - - - - */

// QUARANTINE   fprintf(file, "<h1>Application Values</h1>\n");
  analyze_emit_appval_table(self, snap, file);

  /* - - - - - - - - - - - - - - - - - - - *
   * html trailer
   * - - - - - - - - - - - - - - - - - - - */

// QUARANTINE   fprintf(file, "</body>\n");
// QUARANTINE   fprintf(file, "</html>\n");
// QUARANTINE   fclose(file); file = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  error = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  if( file )
  {
    fclose(file);
  }
  return error;
}
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

/* ========================================================================= *
 * smapsfilt_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * smapsfilt_ctor
 * ------------------------------------------------------------------------- */

void
smapsfilt_ctor(smapsfilt_t *self)
{
  self->smapsfilt_filtmode  = FM_ANALYZE;
  self->smapsfilt_difflevel = -1;
  self->smapsfilt_trimlevel = 0;

  self->smapsfilt_output = 0;
  str_array_ctor(&self->smapsfilt_inputs);
  array_ctor(&self->smapsfilt_snaplist, smapssnap_delete_cb);
}

/* ------------------------------------------------------------------------- *
 * smapsfilt_dtor
 * ------------------------------------------------------------------------- */

void
smapsfilt_dtor(smapsfilt_t *self)
{
  str_array_dtor(&self->smapsfilt_inputs);
  array_dtor(&self->smapsfilt_snaplist);
}

/* ------------------------------------------------------------------------- *
 * smapsfilt_create
 * ------------------------------------------------------------------------- */

smapsfilt_t *
smapsfilt_create(void)
{
  smapsfilt_t *self = calloc(1, sizeof *self);
  smapsfilt_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * smapsfilt_delete
 * ------------------------------------------------------------------------- */

void
smapsfilt_delete(smapsfilt_t *self)
{
  if( self != 0 )
  {
    smapsfilt_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * smapsfilt_delete_cb
 * ------------------------------------------------------------------------- */

void
smapsfilt_delete_cb(void *self)
{
  smapsfilt_delete(self);
}

typedef struct diffkey_t diffkey_t;
typedef struct diffval_t diffval_t;

/* ------------------------------------------------------------------------- *
 * diffval_t
 * ------------------------------------------------------------------------- */

struct diffval_t
{
  double pri;
  double sha;
  double cln;
};

/* ------------------------------------------------------------------------- *
 * diffkey_t
 * ------------------------------------------------------------------------- */

struct diffkey_t
{
  int       appl;
  int       inst;
  int       type;
  int       path;
  int       cnt;

  //diffval_t val[0];
};

/* ========================================================================= *
 * diffval_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * diffval_ctor
 * ------------------------------------------------------------------------- */

void
diffval_ctor(diffval_t *self)
{
  self->pri = 0;
  self->sha = 0;
  self->cln = 0;
}

/* ------------------------------------------------------------------------- *
 * diffval_dtor
 * ------------------------------------------------------------------------- */

void
diffval_dtor(diffval_t *self)
{
}

/* ------------------------------------------------------------------------- *
 * diffval_add
 * ------------------------------------------------------------------------- */

void
diffval_add(diffval_t *self, const diffval_t *that)
{
  self->pri += that->pri;
  self->sha += that->sha;
  self->cln += that->cln;
}

/* ========================================================================= *
 * diffkey_t  --  methods
 * ========================================================================= */

INLINE diffval_t *diffkey_val(diffkey_t *self, int cap)
{
  assert( 0 <= cap && cap < self->cnt );
  return &((diffval_t *)(self+1))[cap];

}

INLINE int diffkey_compare(const diffkey_t *k1, const diffkey_t *k2)
{
  int r = 0;
  if( (r = k1->appl - k2->appl) ) return r;
  if( (r = k1->inst - k2->inst) ) return r;
  if( (r = k1->type - k2->type) ) return r;
  if( (r = k1->path - k2->path) ) return r;
  return 0;
}

double diffkey_rank(diffkey_t *self, diffval_t *res)
{
  double pri = 0, sha = 0, cln = 0;

  for( int i = 0; i < self->cnt; ++i )
  {
    diffval_t *val = diffkey_val(self, i);
    pri += val->pri;
    sha += val->sha;
    cln += val->cln;
  }

  pri /= self->cnt;
  sha /= self->cnt;
  cln /= self->cnt;

  res->pri = 0;
  res->sha = 0;
  res->cln = 0;

  for( int i = 0; i < self->cnt; ++i )
  {
    diffval_t *val = diffkey_val(self, i);
    res->pri += pow2(val->pri - pri);
    res->sha += pow2(val->sha - sha);
    res->cln += pow2(val->cln - cln);
  }

  res->pri = sqrt(res->pri / self->cnt);
  res->sha = sqrt(res->sha / self->cnt);
  res->cln = sqrt(res->cln / self->cnt);

  return fmax3(res->pri, res->sha, res->cln);
}

/* ------------------------------------------------------------------------- *
 * diffkey_create
 * ------------------------------------------------------------------------- */

diffkey_t *
diffkey_create(const diffkey_t *templ)
{
  size_t     size = sizeof(diffkey_t) + templ->cnt * sizeof(diffval_t);
  diffkey_t *self = calloc(1, size);

  *self = *templ;

// QUARANTINE   self->appl = -1;
// QUARANTINE   self->inst = -1;
// QUARANTINE   self->type = -1;
// QUARANTINE   self->path = -1;
// QUARANTINE   self->cnt = ncaps;

  for( int i = 0; i < self->cnt; ++i )
  {
    diffval_ctor(diffkey_val(self, i));
  }
  return self;
}

/* ------------------------------------------------------------------------- *
 * diffkey_delete
 * ------------------------------------------------------------------------- */

void
diffkey_delete(diffkey_t *self)
{
  if( self != 0 )
  {
    for( int i = 0; i < self->cnt; ++i )
    {
      diffval_dtor(diffkey_val(self, i));
    }
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * diffkey_delete_cb
 * ------------------------------------------------------------------------- */

void
diffkey_delete_cb(void *self)
{
  diffkey_delete(self);
}

/* - - - - - - - - - - - - - - - - - - - *
 * sort operator for process data
 * - - - - - - - - - - - - - - - - - - - */

static int
cmp_app_pid(const void *a1, const void *a2)
{
  const smapsproc_t *p1 = *(const smapsproc_t **)a1;
  const smapsproc_t *p2 = *(const smapsproc_t **)a2;
  int r;
  if( (r = p1->smapsproc_AID - p2->smapsproc_AID) != 0 ) return r;
  if( (r = p1->smapsproc_PID - p2->smapsproc_PID) != 0 ) return r;
  return 0;
}

static void
diff_ins(const diffkey_t *key, const diffval_t *val, int cap,
         int diff_cnt, int diff_max, diffkey_t **diff_tab)
{
  for( int lo = 0, hi = diff_cnt;; )
  {
    if( lo == hi )
    {
      if( diff_cnt == diff_max )
      {
        diff_tab = realloc(diff_tab, (diff_max *= 2) * sizeof *diff_tab);
      }
      memmove(&diff_tab[lo+1], &diff_tab[lo+0],
              (diff_cnt - lo) * sizeof *diff_tab);
      diff_cnt += 1;
      diff_tab[lo] = diffkey_create(key);
      diffval_add(diffkey_val(diff_tab[lo],cap), val);
      break;
    }
    int i = (lo + hi) / 2;
    int r = diffkey_compare(diff_tab[i], key);
    if( r < 0 ) { lo = i + 1; continue; }
    if( r > 0 ) { hi = i + 0; continue; }
    diffval_add(diffkey_val(diff_tab[i],cap), val);
    break;
  }
}

static void
diff_emit_entry(diffkey_t *k, const char *name,
                double rank, const double *data,
                char ***out_row, int *out_cnt, int out_dta,
                const char **appl_str, const char **type_str,
                const char **path_str)
{
  char **out = calloc(out_dta, sizeof *out);
  if( k->appl >= 0 ) xstrfmt(&out[0], "%s", appl_str[k->appl]);
  if( k->inst >= 0 ) xstrfmt(&out[1], "%d", k->inst);
  if( k->type >= 0 ) xstrfmt(&out[2], "%s", type_str[k->type]);
  if( k->path >= 0 ) xstrfmt(&out[3], "%s", path_str[k->path]);
  xstrfmt(&out[4],"%s", name);
  for( int j = 0; j < k->cnt; ++j )
  {
    xstrfmt(&out[5+j], "%g", data[j]);
  }
  xstrfmt(&out[5+k->cnt], "%.1f\n", rank);
  out_row[*out_cnt++] = out;
}

int
smapsfilt_diff(smapsfilt_t *self, const char *path,
               int diff_lev, int html_diff, int trim_cols)
{
  int error = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * difference data handling
   * - - - - - - - - - - - - - - - - - - - */

  int         diff_cnt = 0;
  int         diff_max = 256;
  diffkey_t **diff_tab = calloc(diff_max, sizeof *diff_tab);

  symtab_t *appl_tab = symtab_create();
  symtab_t *type_tab = symtab_create();
  symtab_t *path_tab = symtab_create();

  /* - - - - - - - - - - - - - - - - - - - *
   * initialize symbol tables
   * - - - - - - - - - - - - - - - - - - - */

  symtab_enumerate(type_tab, "code");
  symtab_enumerate(type_tab, "data");
  symtab_enumerate(type_tab, "heap");
  symtab_enumerate(type_tab, "anon");
  symtab_enumerate(type_tab, "stack");

  for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
  {
    smapssnap_t *snap = self->smapsfilt_snaplist.data[i];

    for( int k = 0; k < snap->smapssnap_proclist.size; ++k )
    {
      smapsproc_t *proc = snap->smapssnap_proclist.data[k];
      symtab_enumerate(appl_tab, proc->smapsproc_pid.Name);

      for( int j = 0; j < proc->smapsproc_mapplist.size; ++j )
      {
        smapsmapp_t *mapp = proc->smapsproc_mapplist.data[j];
        symtab_enumerate(path_tab, mapp->smapsmapp_map.path);
      }
    }
  }
  symtab_renum(appl_tab);
  symtab_renum(path_tab);

  /* - - - - - - - - - - - - - - - - - - - *
   * enumerate data, normalize pids to
   * application instances while at it
   * - - - - - - - - - - - - - - - - - - - */

  #define P(x) fprintf(stderr, "%s: %g\n", #x, (double)(x));

  int inst_cnt = 0;

  for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
  {
    smapssnap_t *snap = self->smapsfilt_snaplist.data[i];

// QUARANTINE     P(snap->smapssnap_proclist.size);

    for( int k = 0; k < snap->smapssnap_proclist.size; ++k )
    {
      smapsproc_t *proc = snap->smapssnap_proclist.data[k];
      proc->smapsproc_PID = proc->smapsproc_pid.Pid;
      proc->smapsproc_AID = symtab_enumerate(appl_tab,
                                             proc->smapsproc_pid.Name);
    }

    array_sort(&snap->smapssnap_proclist, cmp_app_pid);

    int aid = -1, pid = -1, cnt = 0;
    for( int k = 0; k < snap->smapssnap_proclist.size; ++k )
    {
      smapsproc_t *proc = snap->smapssnap_proclist.data[k];
      if( aid != proc->smapsproc_AID )
      {
        aid = proc->smapsproc_AID, pid = proc->smapsproc_PID, cnt = 0;
      }
      else if( pid != proc->smapsproc_PID )
      {
        pid = proc->smapsproc_PID, ++cnt;
        if( inst_cnt < cnt ) inst_cnt = cnt;
      }
      proc->smapsproc_PID = cnt;
      for( int j = 0; j < proc->smapsproc_mapplist.size; ++j )
      {
        smapsmapp_t *mapp = proc->smapsproc_mapplist.data[j];
        mapp->smapsmapp_AID = proc->smapsproc_AID;
        mapp->smapsmapp_PID = proc->smapsproc_PID;
        mapp->smapsmapp_TID = symtab_enumerate(type_tab,
                                               mapp->smapsmapp_map.type);
        mapp->smapsmapp_LID = symtab_enumerate(path_tab,
                                               mapp->smapsmapp_map.path);
      }
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * reverse lookup tables
   * - - - - - - - - - - - - - - - - - - - */

  int appl_cnt = appl_tab->symtab_count;
  int type_cnt = type_tab->symtab_count;
  int path_cnt = path_tab->symtab_count;

  const char *appl_str[appl_cnt];
  const char *type_str[type_cnt];
  const char *path_str[path_cnt];

  for( int i = 0; i < appl_cnt; ++i )
  {
    //appl_str[i] = appl_tab->symtab_entry[i].symbol_key;

    symbol_t *s = &appl_tab->symtab_entry[i];
    appl_str[s->symbol_val] = s->symbol_key;
  }
  for( int i = 0; i < type_cnt; ++i )
  {
    //type_str[i] = type_tab->symtab_entry[i].symbol_key;
    symbol_t *s = &type_tab->symtab_entry[i];
    type_str[s->symbol_val] = s->symbol_key;
  }
  for( int i = 0; i < path_cnt; ++i )
  {
    //path_str[i] = path_tab->symtab_entry[i].symbol_key;
    symbol_t *s = &path_tab->symtab_entry[i];
    path_str[s->symbol_val] = s->symbol_key;
  }

// QUARANTINE   P(appl_cnt);
// QUARANTINE   P(inst_cnt);
// QUARANTINE   P(type_cnt);
// QUARANTINE   P(path_cnt);

  /* - - - - - - - - - - - - - - - - - - - *
   * accumulate stats
   * - - - - - - - - - - - - - - - - - - - */

  diffkey_t key;
  diffval_t val;

  key.appl = -1;
  key.inst = -1;
  key.type = -1;
  key.path = -1;
  key.cnt  = self->smapsfilt_snaplist.size;

  for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
  {
    smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
    for( int k = 0; k < snap->smapssnap_proclist.size; ++k )
    {
      smapsproc_t *proc = snap->smapssnap_proclist.data[k];
      for( int j = 0; j < proc->smapsproc_mapplist.size; ++j )
      {
        smapsmapp_t *mapp = proc->smapsproc_mapplist.data[j];

        switch( diff_lev )
        {
        default:
        case 4: key.path = mapp->smapsmapp_LID;
        case 3: key.type = mapp->smapsmapp_TID;
        case 2: key.inst = mapp->smapsmapp_PID;
        case 1: key.appl = mapp->smapsmapp_AID;
        case 0: break;
        }
        val.pri = mapp->smapsmapp_mem.Private_Dirty;
        val.sha = mapp->smapsmapp_mem.Shared_Dirty;
        val.cln = (mapp->smapsmapp_mem.Shared_Clean +
                   mapp->smapsmapp_mem.Private_Clean);
        diff_ins(&key, &val, i, diff_cnt, diff_max, diff_tab);
      }
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * output results
   * - - - - - - - - - - - - - - - - - - - */

  double min_rank = 4;

  FILE *file = 0;

  char ***out_row = calloc(diff_cnt * 3, sizeof *out_row);
  int    out_cnt = 0;
  int    out_dta = 4 + 1 + self->smapsfilt_snaplist.size + 1;

  if( trim_cols > 4 ) trim_cols = 4;

  if( (file = fopen(path, "w")) == 0 )
  {
    perror(path); goto cleanup;
  }

  for( size_t i = 0; i < diff_cnt; ++i )
  {
    diffkey_t *k = diff_tab[i];
    if( diffkey_rank(k, &val) >= min_rank )
    {
      double d[k->cnt];
      if( val.pri >= min_rank )
      {
        for( int j = 0; j < k->cnt; ++j ) {
          d[j] = diffkey_val(k, j)->pri;
        }
	diff_emit_entry(k, "pri", val.pri, d, out_row, &out_cnt, out_dta,
                        appl_str, type_str, path_str);
      }
      if( val.sha >= min_rank )
      {
        for( int j = 0; j < k->cnt; ++j ) {
          d[j] = diffkey_val(k, j)->sha;
        }
	diff_emit_entry(k, "sha", val.sha, d, out_row, &out_cnt, out_dta,
                        appl_str, type_str, path_str);
      }
      if( val.cln >= min_rank )
      {
        for( int j = 0; j < k->cnt; ++j ) {
          d[j] = diffkey_val(k, j)->cln;
        }
	diff_emit_entry(k, "cln", val.cln, d, out_row, &out_cnt, out_dta,
                        appl_str, type_str, path_str);
      }
    }
    diffkey_delete(diff_tab[i]);
  }

  /* diff_emit_table(): */
  {
    if( trim_cols > 0 )
    {
      int N[out_cnt];
      for( int i = 1; i < out_cnt; ++i )
      {
        N[i] = 0;
        for( int k = 0; k <= trim_cols; ++k )
        {
          N[i] = k;
          if( out_row[i-0][k] == 0 ) break;
          if( out_row[i-1][k] == 0 ) break;
          if( strcmp(out_row[i-0][k], out_row[i-1][k]) ) break;
        }
      }
      for( int i = 1; i < out_cnt; ++i )
      {
        for( int k = 0; k < N[i]; ++k )
        {
          if( html_diff )
          {
            free(out_row[i][k]);
            out_row[i][k] = 0;
          }
          else
          {
            out_row[i][k][0] = 0;
          }
        }
      }
    }

    if( html_diff )
    {
      /* - - - - - - - - - - - - - - - - - - - *
       * header
       * - - - - - - - - - - - - - - - - - - - */

      fprintf(file,
              "<html><head><title>SMAPS DIFF</title></head><body>\n"
              "<h1>SMAPS DIFF</h1>\n"
              "<p>\n");

      for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
      {
        smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
        fprintf(file, "CAP%d = %s<br>\n", i+1, snap->smapssnap_source);
      }

      fprintf(file,
              "<table border=1>\n"
              "<tr>\n");

      if( diff_lev >= 1 ) fprintf(file, "<th>Cmd");
      if( diff_lev >= 2 ) fprintf(file, "<th>Pid");
      if( diff_lev >= 3 ) fprintf(file, "<th>Type");
      if( diff_lev >= 4 ) fprintf(file, "<th>Path");
      fprintf(file, "<th>Value");
      for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
      {
        fprintf(file, "<th>CAP%d", i+1);
      }
      fprintf(file, "<th>RANK\n");

      /* - - - - - - - - - - - - - - - - - - - *
       * table data
       * - - - - - - - - - - - - - - - - - - - */

      for( int i = 0; i < out_cnt; ++i )
      {
        fprintf(file, "<tr>\n");
        for( int k = 0; k < out_dta; ++k )
        {
          if( out_row[i][k] != 0 )
          {
            int j;
            int r = (k == 1) || (k >= 5);
            for( j = i+1; j < out_cnt; ++j )
            {
              if( out_row[j][k] ) break;
            }
            if( (j -= i) > 1 )
            {
              fprintf(file, "<td%s valign=top rowspan=%d>%s",
                      r ? " align=right" : "", j, out_row[i][k]);
            }
            else
            {
              fprintf(file, "<td%s>%s",
                      r ? " align=right" : "", out_row[i][k]);
            }
          }
        }
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * trailer
       * - - - - - - - - - - - - - - - - - - - */

      fprintf(file,
              "</table>\n"
              "</body>\n"
              "</html>\n");
    }
    else
    {
      /* - - - - - - - - - - - - - - - - - - - *
       * header
       * - - - - - - - - - - - - - - - - - - - */

      fprintf(file, "generator = %s %s\n", TOOL_NAME, TOOL_VERS);
      for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
      {
        smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
        fprintf(file, "CAP%d = %s\n", i+1, snap->smapssnap_source);
      }
      fprintf(file, "\n");

      if( diff_lev >= 1 ) fprintf(file, "Cmd,");
      if( diff_lev >= 2 ) fprintf(file, "Pid,");
      if( diff_lev >= 3 ) fprintf(file, "Type,");
      if( diff_lev >= 4 ) fprintf(file, "Path,");
      fprintf(file, "Value,");
      for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
      {
        fprintf(file, "CAP%d,", i+1);
      }
      fprintf(file, "RANK\n");

      /* - - - - - - - - - - - - - - - - - - - *
       * table
       * - - - - - - - - - - - - - - - - - - - */

      for( int i = 0; i < out_cnt; ++i )
      {
        for( int n=0, k = 0; k < out_dta; ++k )
        {
          if( out_row[i][k] != 0 )
          {
            fprintf(file, "%s%s", n++ ? "," : "", out_row[i][k]);
          }
        }
      }

      /* - - - - - - - - - - - - - - - - - - - *
       * trailer
       * - - - - - - - - - - - - - - - - - - - */

      fprintf(file, "\n");
    }
  }

  error = 0;

  cleanup:

  symtab_delete(appl_tab);
  symtab_delete(type_tab);
  symtab_delete(path_tab);
  free(diff_tab);

  if( file != 0 ) fclose(file);

  return error;
}

/* ------------------------------------------------------------------------- *
 * smapsfilt_handle_arguments
 * ------------------------------------------------------------------------- */

int parse_level(const char *text)
{
  int level = 2;

  if( !strcmp(text, "sys") )
  {
    level = 0;
  }
  else if( !strcmp(text, "app") )
  {
    level = 1;
  }
  else if( !strcmp(text, "pid") )
  {
    level = 2;
  }
  else if( !strcmp(text, "sec") )
  {
    level = 3;
  }
  else if( !strcmp(text, "obj") )
  {
    level = 4;
  }
  else
  {
    char *e = 0;
    int   n = strtol(text, &e, 0);
    if( e > text && *e == 0 )
    {
      level = n;
    }
  }

  return level;
}

static void
smapsfilt_handle_arguments(smapsfilt_t *self, int ac, char **av)
{
  argvec_t *args = argvec_create(ac, av, app_opt, app_man);
  const char *name = basename(av[0]);

  if (!strcmp(name, "sp_smaps_flatten")) {
	  self->smapsfilt_filtmode = FM_FLATTEN;
  } else if (!strcmp(name, "sp_smaps_normalize")) {
	  self->smapsfilt_filtmode = FM_NORMALIZE;
  } else if (!strcmp(name, "sp_smaps_analyze")) {
	  self->smapsfilt_filtmode = FM_ANALYZE;
  } else if (!strcmp(name, "sp_smaps_appvals")) {
	  self->smapsfilt_filtmode = FM_APPVALS;
  } else if (!strcmp(name, "sp_smaps_diff")) {
	  self->smapsfilt_filtmode = FM_DIFF;
  }
  
  while( !argvec_done(args) )
  {
    int       tag  = 0;
    char     *par  = 0;

    if( !argvec_next(args, &tag, &par) )
    {
      msg_error("(use --help for usage)\n");
      exit(1);
    }

    switch( tag )
    {
    case opt_help:
      argvec_usage(args);
      exit(EXIT_SUCCESS);

    case opt_vers:
      printf("%s\n", TOOL_VERS);
      exit(EXIT_SUCCESS);

    case opt_verbose:
      msg_incverbosity();
      break;

    case opt_quiet:
      msg_decverbosity();
      break;

    case opt_silent:
      msg_setsilent();
      break;

    case opt_input:
    case opt_noswitch:
      str_array_add(&self->smapsfilt_inputs, par);
      break;

    case opt_output:
      cstring_set(&self->smapsfilt_output, par);
      break;

    case opt_filtmode:
      {
        static const struct
        {
          const char *name; int mode;
        } lut[] =
        {
          {"flatten",   FM_FLATTEN },
          {"normalize", FM_NORMALIZE },
          {"analyze",   FM_ANALYZE },
          {"appvals",   FM_APPVALS },
          {"diff",      FM_DIFF },
        };
        for( int i = 0; ; ++i )
        {
          if( i == sizeof lut / sizeof *lut )
          {
            msg_fatal("unknown mode '%s'\n", par);
          }
          if( !strcmp(lut[i].name, par) )
          {
            self->smapsfilt_filtmode = lut[i].mode;
            break;
          }
        }
      }
      break;

    case opt_difflevel:
      self->smapsfilt_difflevel = parse_level(par);
      break;

    case opt_trimlevel:
      self->smapsfilt_trimlevel = parse_level(par);
      break;
    default:
      abort();
    }
  }

  argvec_delete(args);
}

static void
smapsfilt_load_inputs(smapsfilt_t *self)
{
  int error;
  for( int i = 0; i < self->smapsfilt_inputs.size; ++i )
  {
    const char *path = self->smapsfilt_inputs.data[i];

    smapssnap_t *snap = smapssnap_create();
    error = smapssnap_load_cap(snap, path);
    if (error) continue;
    array_add(&self->smapsfilt_snaplist, snap);

// QUARANTINE     smapssnap_save_cap(snap, "out1.cap");
// QUARANTINE     smapssnap_save_csv(snap, "out1.csv");

    smapssnap_create_hierarchy(snap);
    if (snap->smapssnap_format == SNAPFORMAT_OLD) {
      fprintf(stderr, "Warning: %s: oldstyle capture file, not removing threads.\n", path);
    } else {
      smapssnap_collapse_threads(snap);
    }

// QUARANTINE     smapssnap_save_cap(snap, "out2.cap");
// QUARANTINE     smapssnap_save_csv(snap, "out2.csv");
// QUARANTINE     smapssnap_save_html(snap, "out2.html");
  }

}

char *path_slice_extension(char *path)
{
  char *e = path_extension(path);
  if( *e != 0 )
  {
    *e++ = 0;
  }
  return e;
}

char *path_make_output(const char *def,
                       const char *src, const char *ext)
{
  char *res = 0;
  if( def == 0 )
  {
    const char *end = path_extension(src);
    xstrfmt(&res, "%.*s%s", (int)(end - src), src, ext);
  }
  else
  {
    res = strdup(def);
  }
  return res;
}

static void
smapsfilt_write_outputs(smapsfilt_t *self)
{
  switch( self->smapsfilt_filtmode )
  {
  case FM_DIFF:
    if( self->smapsfilt_output == 0 )
    {
      msg_fatal("output path must be specified for diff\n");
    }

    if( self->smapsfilt_snaplist.size < 2 )
    {
      msg_warning("diffing less than two captures is pretty meaningless\n");
    }

    {
      char *work = strdup(self->smapsfilt_output);
      char *ext  = path_slice_extension(work);
      int   html = !strcmp(ext, "html");

      int   level = self->smapsfilt_difflevel;
      int   trim  = self->smapsfilt_trimlevel;

      if( level < 0 )
      {
        level = parse_level(path_slice_extension(work));
      }

      smapsfilt_diff(self, self->smapsfilt_output, level, html, trim);

      free(work);
    }
    break;

  case FM_FLATTEN:
    if( self->smapsfilt_output != 0 && self->smapsfilt_snaplist.size != 1 )
    {
      msg_fatal("forcing output path allowed with one source file only!\n");
    }

    for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
    {
      smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
      char *dest = path_make_output(self->smapsfilt_output,
                                    snap->smapssnap_source,
                                    ".flat");
      smapssnap_save_cap(snap, dest);
      free(dest);
    }
    break;

  case FM_NORMALIZE:
    if( self->smapsfilt_output != 0 && self->smapsfilt_snaplist.size != 1 )
    {
      msg_fatal("forcing output path allowed with one source file only!\n");
    }

    for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
    {
      smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
      char *dest = path_make_output(self->smapsfilt_output,
                                    snap->smapssnap_source,
                                    ".csv");
      smapssnap_save_csv(snap, dest);
      free(dest);
    }
    break;

  case FM_ANALYZE:

    if( self->smapsfilt_output != 0 && self->smapsfilt_snaplist.size != 1 )
    {
      msg_fatal("forcing output path allowed with one source file only!\n");
    }

    for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
    {
      smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
      char *dest = path_make_output(self->smapsfilt_output,
                                    snap->smapssnap_source,
                                    ".html");
      /* SMAPS does not provide any data for kernel threads, so remove them to
       * reduce dummy elements from the analysis report.
       */
      analyze_prune_kthreads(snap);
      analyze_t *az   = analyze_create();
      analyze_enumerate_data(az, snap);
      analyze_accumulate_data(az);
      int error = analyze_emit_main_page(az, snap, dest);
      analyze_delete(az);
      //smapssnap_save_html(snap, dest);
      free(dest);
      assert( error == 0 );
    }
    break;

  case FM_APPVALS:

    if( self->smapsfilt_output != 0 && self->smapsfilt_snaplist.size != 1 )
    {
      msg_fatal("forcing output path allowed with one source file only!\n");
    }

    for( int i = 0; i < self->smapsfilt_snaplist.size; ++i )
    {
      smapssnap_t *snap = self->smapsfilt_snaplist.data[i];
      char *dest = path_make_output(self->smapsfilt_output,
                                    snap->smapssnap_source,
                                    ".apps");

      analyze_t *az   = analyze_create();
      analyze_enumerate_data(az, snap);
      analyze_accumulate_data(az);
      int error = analyze_emit_appvals(az, snap, dest);
      analyze_delete(az);
      free(dest);
      assert( error == 0 );
    }
    break;

  default:
    msg_fatal("unimplemented mode %d\n",self->smapsfilt_filtmode);
    break;
  }
}

/* ========================================================================= *
 * main  --  program entry point
 * ========================================================================= */

int main(int ac, char **av)
{
  smapsfilt_t *app = smapsfilt_create();
  smapsfilt_handle_arguments(app, ac, av);
  smapsfilt_load_inputs(app);
  smapsfilt_write_outputs(app);
  smapsfilt_delete(app);
  return 0;
}

/* - - - - - - - - - - - - - - - - - - - *
 * sp_smaps_snapshot
 *
 * sp_smaps_normalize
 * sp_smaps_flatten
 *
 * sp_smaps_analyze
 * sp_smaps_appvals
 *
 * sp_smaps_diff
 *
 * - - - - - - - - - - - - - - - - - - - */

/* vim: set sw=2 noet */
