#! /usr/bin/env python

# This file is part of sp-smaps
#
# Copyright (C) 2004-2007 Nokia Corporation.
#
# Contact: Eero Tamminen <eero.tamminen@nokia.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA

# =============================================================================
# File: sp_smaps_analyze.py
#
# Author: Simo Piiroinen
#
# -----------------------------------------------------------------------------
#
# History:
#
# 25-Apr-2006 Simo Piiroinen
# - sp_smaps_diff column VAR -> back to RANK
# - sp_smaps_diff no longer filters vertical redundancy from csv output
#   by default (new option --filter-output=yes/no)
#
# 10-Apr-2006 Simo Piiroinen
# - code cleanup
# - help fixes
#
# 07-Apr-2006 Simo Piiroinen
# - help fixes
# - is a multi mode executable now and implements:
#   * sp_smaps_flatten:   cap -> cap  thread remover for arm captures
#   * sp_smaps_normalize: cap -> csv  normalization
#   * sp_smaps_analyze:   cap -> html analysis page generator
#   * sp_smaps_appvals:   cap -> csv  application data filter
#   * sp_smaps_diff:      cap -> html smaps capture diff (also csv output)
# - rewritten from scratch
# =============================================================================

import sys,os,string,math

PROGNAME = os.path.splitext(os.path.basename(sys.argv[0]))[0]

TOOL_NAME = PROGNAME
TOOL_VERS = "0.1.3"

NO_PATH = "-"
NO_NAME = "unknown"
ANON = "[anon]"

COPYRIGHT = """\
COPYRIGHT
  Copyright (C) 2004-2007 Nokia Corporation.

  This is free software.  You may redistribute copies of it under the
  terms of the GNU General Public License v2 included with the software.
  There is NO WARRANTY, to the extent permitted by law.
"""

# =============================================================================
FLATTEN_HELP = """\
NAME
  <TOOL_NAME>  --  remove threads from sp_smaps_snapshot data

SYNOPSIS
  <TOOL_NAME> [options] <smaps-capture> ...

DESCRIPTION
  The tool reads raw capture files generated with sp_smaps_snapshot and
  tries to detect and remove duplicate entries due to multiple threads.

  Note that this tool can also process snapshots that were not taken
  from /proc/*/smaps entries.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -v | --verbose
        Enable diagnostic messages
  -q | --quiet
        Disable warning messages
  -s | --silent
        Disable all messages
  -o<path> --output=<path>
        Explicitly set output path for first smaps capture.
        Default: *.csv -> *.flat
EXAMPLES
   %% <TOOL_NAME> smaps-*.cap

    Removes thread redundancy from all smaps-*.cap files, output is
    written to smaps*.flat.

%s

SEE ALSO
  sp_smaps_snapshot (1)
""" % COPYRIGHT

# =============================================================================
ANALYZE_HELP = """\
NAME
  <TOOL_NAME>  --  sp_smaps_snapshot data to browsable html system info

SYNOPSIS
  <TOOL_NAME> [options] <smaps-capture> ...

DESCRIPTION
  The tool reads raw (*.cap) or normalized (*.csv) sp_smaps_snapshot
  capture data and produces detailed view to system / process / object
  memory usage in html format.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -v | --verbose
        Enable diagnostic messages
  -q | --quiet
        Disable warning messages
  -s | --silent
        Disable all messages
  -o<path> --output=<path>
        Explicitly set output path for first smaps capture.
        Default: *.csv -> *.html.
EXAMPLES
  %% <TOOL_NAME> smaps-*.cap

    Converts all smaps data files in current directory for browsing.

%s

SEE ALSO
  sp_smaps_snapshot (1), sp_smaps_normalize (1)
""" % COPYRIGHT

# =============================================================================
APPVALS_HELP = """\
NAME
  <TOOL_NAME>  --  sp_smaps_snapshot data to process summary info

SYNOPSIS
  <TOOL_NAME> [options] <smaps-capture> ...

DESCRIPTION
    The tool reads raw (*.cap) or normalized (*.csv) sp_smaps_snapshot
    capture data and produces summary table for memory usage of all
    applications in the capture file.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -v | --verbose
        Enable diagnostic messages
  -q | --quiet
        Disable warning messages
  -s | --silent
        Disable all messages
  -o<path> --output=<path>
        Explicitly set output path for first smaps capture.
        Default: *.ext -> *.apps
EXAMPLES
  %% <TOOL_NAME> smaps-*.cap

    Generates application memory use summaries from all smaps snapshots.

%s

SEE ALSO
  sp_smaps_snapshot (1), sp_smaps_normalize (1)
""" % COPYRIGHT

# =============================================================================
NORMALIZE_HELP = """\
NAME
  <TOOL_NAME>  --  sp_smaps_snapshot data to csv converter

SYNOPSIS
  <TOOL_NAME> [options] <smaps-capture> ...

DESCRIPTION
  The tool reads smaps snapshots and writes CSV format data that
  can be imported to spreadsheet programs or sp_smaps_analyze
  and friends - slightly faster if multiple passes over the
  data must be done.

  The capture files should be taken with sp_smaps_snapshot,
  though the tool tolerates to some extent old style capture
  files obtained with: 'head -2000 /proc/[1-9]*/smap'.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -v | --verbose
        Enable diagnostic messages
  -q | --quiet
        Disable warning messages
  -s | --silent
        Disable all messages
  -o <destination path> | --output=<destination path>
        Output file to use instead of stdout.
EXAMPLES
  %% <TOOL_NAME> smaps-after-boot.cap -o smaps-after-boot.csv

    Converts smaps-after-boot.cap to CSV format, suitable for
    further processing or viewing with spreadsheet program.

%s

SEE ALSO
  sp_smaps_snapshot (1), sp_smaps_analyze (1)
""" % COPYRIGHT

# =============================================================================
APPDIFF_HELP = """\
NAME
  <TOOL_NAME>  --  sp_smaps_snapshot data diff tool

SYNOPSIS
  <TOOL_NAME> [options] <smaps-capture> ...

DESCRIPTION
  The tool reads N smaps caputre files generated with sp_smaps_snapshot and
  provides differences in application memory usage between the capture files.

  The verbosity of generated diff data can be controlled by
  diff level and minimum difference settings.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -v | --verbose
        Enable diagnostic messages
  -q | --quiet
        Disable warning messages
  -s | --silent
        Disable all messages
  -l# | --level=#
        Display results at level:
            0 = capture
            1 = command
            2 = command, pid
            3 = command, pid, type
            4 = command, pid, type, path
  -r# | --min-rank=#
        Omit results with rank < given number.

  -o<path> | --output=<path>
        Write to <path> instead of stdout.

  -e<bool> | --filtered-output=<bool>
        Filter vertical redundancy from output, makes tables
        more human readable. Default: html=yes, csv=no.

EXAMPLES
  %% <TOOL_NAME> --level=3 *after_boot*.csv

      Shows difference of smaps snapshots at memory usage down
      to level of mapping types (code, data, anon).

  %% <TOOL_NAME> *after_boot*.cap -o boot_diff.sys.csv

      Writes diff in csv format using level 0.

  %% <TOOL_NAME> *after_boot*.cap -o boot_diff.pid.html

      Writes diff in html using level 2.

NOTES
  Setting destination path will also implicitly set the level if
  path has some of the following components:
      .sys -> 0
      .app -> 1
      .pid -> 2
      .sec -> 3
      .obj -> 4

%s

SEE ALSO
  sp_smaps_snapshot (1), sp_smaps_normalize (1), sp_smaps_analyze (1)
""" % COPYRIGHT

# ===========================================================================
# Diagnostics API
# ===========================================================================

verbosity = 3 # 0=silent, 1=fatal, 2=error, 3=warning, 4=debug

def verbose():
    global verbosity
    verbosity += 1

def quiet():
    global verbosity
    verbosity -= 1

def silent():
    global verbosity
    verbosity = 0

def message(msg):
    t = PROGNAME
    l = " " * len(t)
    for s in msg.split("\n"):
        print>>sys.stderr, "%s: %s" % (t,s)
        t = l

def fatal(msg):
    if verbosity >= 1: message("FATAL: %s" % msg)
    sys.exit(1)

def error(msg):
    if verbosity >= 2: message("ERROR: %s" % msg)

def warning(msg):
    if verbosity >= 3: message("Warning: %s" % msg)

def debug(msg):
    if verbosity >= 4: message("debug: %s" % msg)

def tos(s):
    if s == 0 or s == "0":
        return "-"
    return str(s).strip()

# ===========================================================================
# Utilities
# ===========================================================================

def sliceQ(n,k):
    if n and k:
        p = (n+k-1)/k
        q = (n+p-1)/p
        return q
    return None

def relpath(dest, srce):
    # "ding/dong/foo.txt" to "ding/hong/bar.html"
    # -> ../hong/bar.html"

    d = dest.split("/")
    s = srce.split("/")
    s.pop() # remove file
    while s and d and s[0] == d[0]:
        del s[0]
        del d[0]
    return string.join([".."]*len(s) + d, "/")

def tocsv(s):
    return str(s).replace(",","_")

def autoconv(s):
    try:
        return long(s)
    except ValueError:
        return s.strip()

def dotable(data, top=1, lft=1, topmap={}, lftmap={}, slice=None):
    ROWS = len(data)
    COLS = len(data[0])
    html = []
    _ = lambda x:html.append(x)

    Zero = "\1"

    p = lambda y,x:x<COLS and y<ROWS and data[y][x] == None

    for c in range(lft):
        r = top
        while r < ROWS:
            i = r + 1
            while i < ROWS and data[r][c] == data[i][c]:
                data[i][c] = None
                i += 1
            r = i

    _("<table border=1>")

    if p(0,0):
        data[0][0] = ""

    if slice == None:
        slice = ROWS
    else:
        slice += top
    beg = len(html)
    end = len(html)

    for r in range(0, ROWS):
        if r == top:
            end = len(html)
        if r != 0 and r % slice == 0:
            #print>>sys.stderr, "SLICE @ %d" % r
            html.extend(html[beg:end])
        _("<tr>")
        for c in range(COLS):
            if data[r][c] in (None,Zero):
                continue
            w,h = 1,1
            while p(r+h,c): h += 1
            while p(r,c+w): w += 1
            for i in range(h):
                for k in range(1,w):
                    if not p(r+i,c+k):
                        w = min(w,k)
                        break
            if r < top:
                t = data[r][c]
                t = topmap.get(t,t)
                bg = ""
                if c or t: bg = " bgcolor=#ffffaa"
                _("<th%s colspan=%d rowspan=%d>%s"%(bg,w,h,t))
            elif c < lft:
                t = data[r][c]
                t = lftmap.get(t,t)
                _("<th bgcolor=#ddffff colspan=%d rowspan=%d align=left>%s"%(w,h,t))
            else:
                bg = "#ffffff"
                if (r-top)/3&1: bg = "#f0f0f0"
                t = tos(data[r][c])
                _("<td bgcolor=%s colspan=%d rowspan=%d align=right>%s"%(bg,w,h,t))
            while h > 1:
                h -= 1
                data[r+h][c] = Zero
            while w > 1:
                w -= 1
                data[r][c+w] = Zero
    _("</table>")
    return string.join(html,"\n")

# ===========================================================================
# Data Fields
# ===========================================================================

mem_rows_def = ("total","code","data","heap","anon","stack")
mem_rows = []
mem_cols = ("pri","sha","cln","rss","size","cow")

cap_uniq = (
"name",
"vmsize",
"vmlck",
"vmrss",
"vmdata",
"vmstk",
"vmexe",
"vmlib",
"vmpte",
)

cap_process_keys = (
("name",    "Name",          NO_NAME),
("pid",     "Pid",           0),
("ppid",    "PPid",          0),
("threads", "Threads",       1),
)

cap_memory_keys = (
("vmsize",  "VmSize",        0),
("vmlck",   "VmLck",         0),
("vmrss",   "VmRSS",         0),
("vmdata",  "VmData",        0),
("vmstk",   "VmStk",         0),
("vmexe",   "VmExe",         0),
("vmlib",   "VmLib",         0),
("vmpte",   "VmPTE",         0),
)

map_file_keys = (
("head",    None,            0),
("tail",    None,            0),
("prot",    None,            0),
("offs",    None,            0),
("node",    None,            0),
("flag",    None,            0),
("path",    None,            0),
)

map_size_keys = (
("size",    "Size",          0),
("rss",     "Rss",           0),
("shacln",  "Shared_Clean",  0),
("shadty",  "Shared_Dirty",  0),
("pricln",  "Private_Clean", 0),
("pridty",  "Private_Dirty", 0),
)

map_derived_keys = (
("pri",    None,             0),
("sha",    None,             0),
("cln",    None,             0),
("cow",    None,             0),
)

cap_keys = cap_process_keys + cap_memory_keys

map_keys = cap_process_keys + map_file_keys + map_size_keys + map_derived_keys

cap_lut = {}
for k,v,d in cap_keys:
    if v != None:
        cap_lut[v] = k

map_lut = {}
for k,v,d in map_keys:
    if v != None:
        map_lut[v] = k

# ===========================================================================
# CAPTURE FILES as from sp_smaps_snapshot
# ===========================================================================

class CapData:
    def __init__(self, tag):
        for k,v,d in cap_keys:
            setattr(self, k, d)

        self.tag = tag
        self.data = []
        self.sub = []
        self.par = None
        self.use = 1

    def set(self, key, val):
        if key == "Name":
            while val[:1] == "-":
                val = val[1:]
            if not val:
                val = NO_NAME

        if cap_lut.has_key(key):
            setattr(self, cap_lut[key], autoconv(val))
        else:
            warning("CapData: unknown key: %s=%s" % (key,val))

    def key(self):
        return map(lambda x:getattr(self, x), cap_uniq)

    def emit(self, file):
        print>>file, "==>%s<==" % self.tag

        for k,v,d in cap_keys:
            print>>file, "#%s: %s" % (v, str(getattr(self, k)))

        for s in self.data:
            print>>file, "%s" % s
        print>>file, ""

def cap_load(path=NO_PATH):
    caps = []
    cap = None

    if path == NO_PATH:
        path = "stdin"
        file = sys.stdin
    elif not os.path.isfile(path):
        fatal("%s: no such file" % path)
    else:
        file = open(path)

    debug("%s: reading ..." % path)

    for s in file:
        s = s.strip()
        if s == "":
            continue

        if s[:3] == "==>" and s[-3:] == "<==":
            cap = CapData(s[3:-3].strip())
            caps.append(cap)
            continue

        if not cap:
            warning("Ignoring: %s" % s)
            continue

        if s[0] == "#":
            if not ':' in s:
                warning("Ignoring: %s" % s)
                continue
            k,v = s[1:].split(":",1)
            cap.set(k.strip(), v.strip())
            continue

        cap.data.append(s)

    caps.sort(lambda a,b:cmp(a.pid, b.pid))

    return caps

def cap_save(caps, path=NO_PATH):
    if path == NO_PATH:
        path = "stdout"
        file = sys.stdout
    else:
        file = open(path,"w")

    debug("%s: writing ..." % path)

    for cap in caps:
        cap.emit(file)

def cap_tree(cap, lev=0):
    print>>sys.stderr, "%5d %*s%s" % (cap.pid, lev*4, "", cap.name)
    for sub in cap.sub:
        cap_tree(sub, lev+1)

def cap_flat_recursive(cap):
    # -- depth first --
    for sub in cap.sub:
        cap_flat_recursive(sub)

    # -- flatten --
    key = cap.key()
    new = []
    for sub in cap.sub:
        if cap.name == sub.name and key == sub.key():
            debug("%s: merge %d -> %d" % (cap.name, sub.pid, cap.pid))
            sub.use = None
            new.extend(sub.sub)
            cap.threads += sub.threads
        else:
            new.append(sub)

    # -- reparent --
    for sub in new:
        sub.par  = cap
        sub.ppid = cap.pid

    cap.sub = new

def cap_flat(caps):
    pids = {}
    done = []
    mpid = 0

    for cap in caps:
        if cap.pid == 0:
            v = cap.tag.split("/")
            try:
                cap.pid = int(cap.tag.split("/")[2])
            except ValueError:
                cap.pid = 100000+mpid
            mpid += 1

    if mpid == len(caps):
        warning("oldstyle capture file, not flattening ...")
        return caps

    for cap in caps:
        if pids.has_key(cap.pid):
            error("duplicate PID %d" % cap.pid)
        else:
            pids[cap.pid] = cap
            done.append(cap)

    for cap in done:
        if cap.pid != cap.ppid and pids.has_key(cap.ppid):
            cap.par = pids[cap.ppid]
            cap.par.sub.append(cap)
        else:
            if cap.pid != 1:
                warning("PID %d -> nonexisting PPID %d" % (cap.pid, cap.ppid))
            cap.ppid = 0

    for cap in done:
        if not cap.par:
            cap_flat_recursive(cap)

## QUARANTINE     for cap in done:
## QUARANTINE   if not cap.par:
## QUARANTINE       cap_tree(cap)

    return filter(lambda x:x.use, done)

# ===========================================================================
# MAP entries
# ===========================================================================

class MapData:
    def __init__(self, var=None, cap=None):

        for k,v,d in map_keys:
            setattr(self, k, d)

        if cap != None:
            for k,v,d in map_keys:
                if hasattr(cap, k):
                    setattr(self, k, getattr(cap, k))

        if var != None:
            head,tail,prot,offs,node,flag,path = var

            self.head = head
            self.tail = tail
            self.prot = prot
            self.offs = offs
            self.node = node
            self.flag = flag
            self.path = path

    def set(self, key, val):
        if map_lut.has_key(key):
            setattr(self, map_lut[key], autoconv(val))

    def scan(self):
        self.file = os.path.basename(self.path)

        if self.path[:1] == "[" and self.path[-1:] == "]":
            self.type = self.path[1:-1]
        elif 'x' in self.prot:
            self.type = "code"
        else:
            self.type = "data"

        self.cln = self.shacln + self.pricln
        self.sha = self.shadty
        self.pri = self.pridty
        self.cow = 0

        self.name = str(self.name)  # fix bug with numeric command names
        while self.name[:1] == "-":
            self.name = self.name[1:]

def maps_from_caps(caps):
    oldstyle = 0
    maps = []
    curr = None
    for cap in caps:
        todo = []
        for s in cap.data:
            if s[8:9] == "-":
                v = s.split(None, 5)

                if len(v) == 2:
                    v += ["0","00:00","0",ANON]
                    oldstyle = 1
                elif len(v) == 3:
                    v = v[:2] + ["0","00:00","0"] + v[-1:]
                    oldstyle = 1

                if len(v) < 5:
                    warning("Ignoring: %s" % s)
                    continue
                if len(v) < 6:
                    v.append(ANON)

                addr,prot,offs,node,flag,path = v
                head,tail = addr.split("-",1)

                head = long(head, 16)
                tail = long(tail, 16)
                offs = long(offs, 16)
                flag = long(flag)

                var = (head,tail,prot,offs,node,flag,path)
                curr = MapData(var=var, cap=cap)
                todo.append(curr)
                continue

            if not curr or ':' not in s:
                warning("Ignoring: %s" % s)
                continue

            k,v = s.split(":",1)
            curr.set(k.strip(), v.split()[0])

        if not cap.name or cap.name == NO_NAME:
            for curr in todo:
                if not 'x' in curr.prot: continue
                if not '/' in curr.path: continue
                cap.name = os.path.basename(curr.path)
                warning("fixed: %d name -> %s" % (cap.pid, cap.name))
                for curr in todo: curr.name = cap.name
                break
        maps.extend(todo)

    for curr in maps:
        curr.scan()

    if oldstyle:
        warning("old style smaps data encountered, added bogus data...")

    return maps

def maps_save_csv(maps, path=NO_PATH):
    if path == NO_PATH:
        path = "stdout"
        file = sys.stdout
    else:
        file = open(path,"w")

    debug("%s: writing ..." % path)

    print >>file, "generator=%s %s" % (TOOL_NAME, TOOL_VERS)
    print >>file, ""

    labs = map(lambda x:x[0], map_keys)
    print >>file, string.join(map(tocsv, labs), ",")

    for m in maps:
        r = map(lambda x: getattr(m, x), labs)
        print >>file, string.join(map(tocsv, r), ",")
    print >>file, ""

def maps_load_csv(path=NO_PATH):
    maps = []
    if path == NO_PATH:
        path = "stdin"
        file = sys.stdin
    else:
        file = open(path)

    debug("%s: reading ..." % path)

    head, labs, rows = [],[],[]

    srce = file.readlines()
    srce = map(string.rstrip, srce)
    srce.reverse()

    while srce:
        s = srce.pop()
        if s == "":
            if not head:
                warning("%s: empty header?" % path)
            break
        if not '=' in s:
            warning("%s: malformed header?" % path)
            srce.append(s)
            break
        k,v = s.split("=",1)
        head.append((k.strip(), v.strip()))

    if srce and srce[-1]:
        s = srce.pop()
        labs = s.split(",")

    if not labs:
        warning("%s: no labels?" % path)

    cols = map(lambda x:x[0], map_keys)
    cols = map(lambda x:labs.index(x), cols)

    while srce:
        s = srce.pop()
        if s == "":
            break
        r = s.split(",")
        assert len(r) == len(labs)
        r = map(lambda x:r[x], cols)
        rows.append(r)

    if not rows:
        warning("%s: no table?" % path)

    cols = map(lambda x:x[0], map_keys)

    for r in rows:
        m = MapData()
        for k,v in zip(cols, r):
            setattr(m, k, autoconv(v))
        m.scan()
        maps.append(m)
    return maps

def accumulate(a,b):
    a.pri  += b.pri
    a.sha  += b.sha
    a.cln  += b.cln
    a.rss  += b.rss
    a.size += b.size
    a.cow  += b.cow

def maximum(a,b):
    a.pri  += b.pri
    a.sha  = max(a.sha,  b.sha)
    a.cln  = max(a.cln,  b.cln)
    a.rss  = max(a.rss,  b.rss)
    a.size = max(a.size, b.size)
    a.cow  = max(a.cow,  b.cow)

def maximum2(a,b):
    a.pri  = max(a.pri,  b.pri)
    a.sha  = max(a.sha,  b.sha)
    a.cln  = max(a.cln,  b.cln)
    a.rss  = max(a.rss,  b.rss)
    a.size = max(a.size, b.size)
    a.cow  = max(a.cow,  b.cow)

class MemRow:
    def __init__(self):
        self.pri  = 0
        self.sha  = 0
        self.cln  = 0
        self.rss  = 0
        self.size = 0
        self.cow  = 0

    def vector(self):
        return [self.pri,self.sha,self.cln,self.rss,self.size,self.cow]

    def nonzero(self):
        return max(self.vector()) != 0

class MemStat:
    def __init__(self):
        self.row = []
        for k in mem_rows:
            self.row.append(MemRow())

    def getrow(self, lab):
        r = mem_rows.index(lab)
        return self.row[r]

    def getcol(self, lab):
        return map(lambda x:getattr(x,lab), self.row)

    def update_total(self):
        self.row[0] = t = MemRow()
        for r in self.row:
            accumulate(t,r)

    def total(self):
        return self.row[0]

    def add_app(self, app):
        accumulate(self.getrow(app.type), app)

    def add_obj(self, obj):
        maximum(self.getrow(obj.type), obj)

    def table(self):
        tab = [
        ["","Dirty",None,"Clean","Resident","Size","COW"],
        [None,"Private","Shared",None,None,None,None],
        ]
        for t in mem_rows:
            r = self.getrow(t)
            #if not r.nonzero():        continue
            tab.append([t] + r.vector())
        return dotable(tab,2,1)

def maps_to_data(maps):

    # -- scan all map types used in data --
    v,h = [],{}
    for t in mem_rows_def:
        if not h.has_key(t):
            h[t] = None
            v.append(t)
    for t in map(lambda x:x.type, maps):
        if not h.has_key(t):
            h[t] = None
            v.append(t)
    mem_rows[:] = v

    # -- enumerate unique applications and objects --
    appid, objid = {}, {}
    for m in maps:
        if not appid.has_key(m.pid):  appid[m.pid]  = "app%03d" % len(appid)
        if not objid.has_key(m.path): objid[m.path] = "obj%03d" % len(objid)

    # -- gather mapping data for applications and objects --
    apps, objs = [],[]
    for key,tag in appid.items():
        sel = filter(lambda x:x.pid == key, maps)
        tab = MemStat()
        for m in sel:
            tab.add_app(m)
        tab.update_total()
        apps.append((key,tag,sel,tab))

    for key,tag in objid.items():
        sel = filter(lambda x:x.path == key, maps)
        tab = MemStat()
        for m in sel:
            tab.add_obj(m)
        tab.update_total()
        objs.append((key,tag,sel,tab))

    # -- sort applications and objects according to private memory usage --
    apps.sort(lambda a,b:cmp(b[-1].total().vector(),a[-1].total().vector()))
    objs.sort(lambda a,b:cmp(b[-1].total().vector(),a[-1].total().vector()))

    # -- scan max process --
    appmax = MemStat()
    for key,tag,sel,tab in apps:
        for t in mem_rows:
            a = appmax.getrow(t)
            b = tab.getrow(t)
            maximum2(a,b)

    # -- scan system upper bound --
    sysmax = MemStat()
    for key,tag,sel,tab in apps:
        for t in mem_rows:
            a = sysmax.getrow(t)
            b = tab.getrow(t)
            accumulate(a,b)
    sysmax.update_total()

    # -- scan system estimate --
    sysest = MemStat()
    for key,tag,sel,tab in objs:
        for t in mem_rows:
            a = sysest.getrow(t)
            b = tab.getrow(t)
            accumulate(a,b)
    for t in mem_rows:
        # recalculate rss from estimate -> more useful this way
        a = sysest.getrow(t)
        a.rss = a.pri + a.sha + a.cln
    sysest.update_total()

    return apps,appid,objs,objid,sysest,sysmax,appmax

def data_to_appvals(path, apps,appid,objs,objid,sysest,sysmax,appmax):
    rows = []

    aval = ["name","pid","ppid","threads"]
    xval = ["pri","sha","cln","rss","size","cow"]
    yval = mem_rows[1:]
    rows.append(aval+xval+yval)

    # -- application values --
    for key,tag,sel,tab in apps:
        app = sel[0]
        row = []
        for k in aval:
            row.append(getattr(app, k))
        tmp = tab.total()
        for k in xval:
            row.append(getattr(tmp, k))
        for tmp in yval:
            row.append(tab.getrow(tmp).rss)
        assert len(row) == len(rows[0])
        rows.append(row)

    debug("%s: writing ..." % path)
    file = open(path,"w")
    print>>file, "generator = %s %s" % (TOOL_NAME, TOOL_VERS)
    print>>file, ""
    for row in rows:
        row = map(tocsv, row)
        print>>file, string.join(row, ",")
    print>>file, ""

class Dummy:
    def __init__(self):
        pass
    def emit(self, f):
        f('<li><a href="%s">%s (%d)</a>' % (self.url,self.app.name, self.app.pid))
        if self.sub:
            f("<ul>")
            for c in self.sub: c.emit(f)
            f("</ul>")

def data_to_html(navpath, apps,appid,objs,objid,sysest,sysmax,appmax):

    # -- make sure we have datadir --
    datadir = os.path.splitext(navpath)[0] + ".dir"
    if not os.path.isdir(datadir):
        os.makedirs(datadir)

    # =======================================================================
    # MAIN NAVIGATOR
    # =======================================================================

    navdata = []
    _ = lambda x:navdata.append(x)

    _("<html><head><title>SMAPS DATA</title></head><body>")
    _("<h1>System Estimates</h1>")

    # -- system estimate --
    _("<h2>System: Memory Use Estimate</h2>")
    _(sysest.table())
    _("<p>Private and Size are accurate, the rest are minimums.")

    _("<h2>System: Memory Use App Totals</h2>")
    _(sysmax.table())
    _("<p>Private is accurate, the rest are maximums.")

    # -- maximum process --
    _("<h2>System: Memory Use App Maximums</h2>")
    _(appmax.table())
    _("<p>No process has values larger than the ones listed above.")

    # -- process tree --
    _("<h1>Process Hierarchy</h1>")
    pids = []
    pidm = {}
    for key,tag,sel,tab in apps:
        d = Dummy()
        d.app = sel[0]
        d.sub = []
        d.par = None
        d.url = "%s/%s.html" % (datadir,appid[d.app.pid])
        d.url = relpath(d.url, navpath)
        pids.append(d)
        pidm[d.app.pid] = d
    for d in pids:
        if pidm.has_key(d.app.ppid):
            p = pidm[d.app.ppid]
            d.par = p
            p.sub.append(d)
    pids = filter(lambda x:not x.par, pids)
    if pids:
        _("<ul>")
        for d in pids:d.emit(_)
        _("</ul>")
    pids,pidm = None,None

    # -- application values --
    _("<h1>Application Values</h1>")
    xxx = mem_rows[1:]
    yyy = [None]*len(xxx)
    zzz = ["RSS / Class"]+yyy[1:]
    dta = [
    ["Application","RSS / Status",None,None,"Virtual<br>Memory",None,"RSS<br>COW<br>Est."]+zzz,
    [None,"Dirty",None,"Clean",None,None,None]+map(lambda x:x.capitalize(),xxx),
    [None,"Private","Shared",None,"RSS","Size",None]+yyy,
    ]
    for key,tag,sel,tab in apps:
        app = sel[0]
        url = "%s/%s.html" % (datadir, appid[key])
        url = relpath(url, navpath)
        url = '<a href="%s">%s %d</a>' % (url, app.name,app.pid)
        row = [url]
        t = tab.total()
        row += [t.pri, t.sha, t.cln, t.rss, t.size, t.cow]
        for t in xxx:
            r = tab.getrow(t)
            row.append(r.rss)
        dta.append(row)
    _(dotable(dta,3,1,slice=sliceQ(len(dta),20)))

    # -- object values --
    _("<h1>Object Values</h1>")
    xxx = mem_rows[1:]
    yyy = [None]*len(xxx)
    zzz = ["RSS / Class"]+yyy[1:]
    dta = [
    ["Object","RSS / Status",None,None,"Virtual<br>Memory",None,"RSS<br>COW<br>Est."]+zzz,
    [None,"Dirty",None,"Clean",None,None,None]+map(lambda x:x.capitalize(),xxx),
    [None,"Private","Shared",None,"RSS","Size",None]+yyy,
    ]
    for key,tag,sel,tab in objs:
        obj = sel[0]
        url = "%s/%s.html" % (datadir, objid[key])
        url = relpath(url, navpath)
        url = '<a href="%s">%s</a>' % (url, obj.file)
        row = [url]
        t = tab.total()
        row += [t.pri, t.sha, t.cln, t.rss, t.size, t.cow]
        for t in xxx:
            r = tab.getrow(t)
            row.append(r.rss)
        dta.append(row)
    _(dotable(dta,3,1,slice=sliceQ(len(dta),20)))
    #_(dotable(dta,3,1,slice=20))

    # -- write out html --
    _("</body></html>")
    #print>>sys.stderr, "\rwriting: %s ...\33[K" % navpath,
    debug("writing: %s ..." % navpath)
    print>>open(navpath,"w"),string.join(navdata,"\n")

    # =======================================================================
    # APPLICATION PAGES
    # =======================================================================

    # -- xref sort operatot --
    _1 = lambda a,b: cmp(a.file.lower(), b.file.lower())
    _2 = lambda a,b: cmp(a.path.lower(), b.path.lower())
    _3 = lambda a,b: cmp(a.type, b.type)
    __ = lambda a,b:_1(a,b) or _2(a,b) or _3(a,b)

    for key,tag,sel,mem in apps:
        app = sel[0]
        tag  = "Application: %s (%d)" % (app.name, app.pid)
        path = "%s/%s.html" % (datadir, appid[app.pid])
        html = []
        _    = lambda x:html.append(x)

        _("<html><head><title>%s</title></head><body>" % tag)
        _("<h1>%s</h1>" % tag)
        _(mem.table())

        _("<h1>Mapping XREF</h1>")
        tab = [
        ["Object","Type","Prot","Size","Rss","Dirty",None,"Clean",None],
        [None,None,None,None,None,"Private","Shared","Private","Shared"]
        ]
        sel.sort(__)
        for m in sel:
            url = "%s/%s.html" % (datadir, objid[m.path])
            url = relpath(url, path)
            url = '<a href="%s">%s</a>' % (url, os.path.basename(m.path))
            row = [url, m.type, m.prot, m.size,m.rss]
            row += [m.pridty,m.shadty, m.pricln, m.shacln]
            tab.append(row)
        _(dotable(tab,2,1))
        _("</body></html>")
        #print>>sys.stderr, "\rwriting: %s ...\33[K" % path,
        debug("writing: %s ..." % path)
        print>>open(path,"w"),string.join(html,"\n")

    # =======================================================================
    # OBJECT PAGES
    # =======================================================================

    # -- xref sort operatot --
    _1 = lambda a,b: cmp(a.name.lower(), b.name.lower())
    _2 = lambda a,b: cmp(a.pid, b.pid)
    _3 = lambda a,b: cmp(a.type, b.type)
    #_4 = lambda a,b: cmp(a.prot, b.prot)
    _4 = lambda a,b: cmp('w' in b.prot, 'w' in a.prot)
    __ = lambda a,b:_1(a,b) or _2(a,b) or _3(a,b) or _4(a,b)

    for key,tag,sel,mem in objs:
        obj = sel[0]
        tag  = "Object: %s" % (obj.path)
        path = "%s/%s.html" % (datadir, objid[obj.path])
        html = []
        _    = lambda x:html.append(x)

        _("<html><head><title>%s</title></head><body>" % tag)
        _("<h1>%s</h1>" % tag)
        _(mem.table())
        _("<h1>Application XREF</h1>")
        tab = [
        ["Object","Type","Prot","Size","Rss","Dirty",None,"Clean",None],
        [None,None,None,None,None,"Private","Shared","Private","Shared"]
        ]
        sel.sort(__)
        for m in sel:
            url = "%s/%s.html" % (datadir, appid[m.pid])
            url = relpath(url, path)
            tag = "%s (%d)" % (m.name, m.pid)
            url = '<a href="%s">%s</a>' % (url, tag)
            row = [url, m.type, m.prot, m.size,m.rss]
            row += [m.pridty,m.shadty, m.pricln, m.shacln]
            tab.append(row)
        _(dotable(tab,2,1))
        _("</body></html>")
        #print>>sys.stderr, "\rwriting: %s ...\33[K" % path,
        debug("writing: %s ..." % path)
        print>>open(path,"w"),string.join(html,"\n")

    # =======================================================================
    # END HTML GEN
    # =======================================================================

    #print>>sys.stderr,"\rdone\33[K"
    debug("done")

def import_capture(srce):
    ext = os.path.splitext(srce)[1]
    if not ext in (".cap",".flat"):
        warning("%s: assuming RAW capture format data" % srce)
    caps = cap_load(srce)
    caps = cap_flat(caps)
    return caps

def import_normalized(srce):
    ext = os.path.splitext(srce)[1]
    if not ext in (".cap",".flat",".csv"):
        warning("%s: assuming CSV capture format data" % srce)
        ext = ".csv"

    if ext in (".cap", ".flat"):
        caps = import_capture(srce)
        maps = maps_from_caps(caps)
    elif ext == ".csv":
        maps = maps_load_csv(srce)
    else:
        fatal("don't know how to analyze %s" % repr(srce))
        assert 0
    return maps

def import_processed(srce):
    maps = import_normalized(srce)
    apps,appid,objs,objid,sysest,sysmax,appmax = maps_to_data(maps)
    return (apps,appid,objs,objid,sysest,sysmax,appmax)

def median(v):
    v = v[:]
    v.sort()
    n = len(v)
    return (v[(n-1)/2] + v[(n-0)/2]) * 0.5

def calc_rank(v):
    s,m = 0,median(v)
    for i in v:
        s += (i-m)**2
    #return math.sqrt(s)/len(v)
    return math.sqrt(s/len(v))

def uniqlist(v):
    h = {}
    for i in v: h[i] = None
    v = h.keys()
    v.sort()
    return v

class DiffBlk:
    def __init__(self, tid, key, maps, parent=None):
        self.tid = tid
        self.key = key
        self.maps = maps
        self.subs = {}
        self.zero()
        if parent != None:
            parent.subs[key] = self

    def get(self, v):
        if self.tid == "NUL":
            return "n/a"
        return getattr(self, v)

    def emit(self, lev=0):
        t = " "*(lev*4)
        r = "%6d %6d %6d" % (self.pri, self.sha, self.cln)
        l = "%s%s(%s)" % (t, self.tid, self.key)
        m = ("  . " * 40)[:64][len(l):]
        print l,m,r
        #print "%-64s %s" % (l,r)
        if not self.subs:
            for m in self.maps:
                print "%s  MAP -> %s %s" % (t, m.type, m.file)
        for s in self.subs.values():
            s.emit(lev+1)

    def zero(self):
        self.pri  = 0
        self.sha  = 0
        self.cln  = 0
        self.rss  = 0
        self.size = 0
        self.cow  = 0

    def scan(self):
        self.zero()
        for m in self.maps:
            accumulate(self, m)
        for s in self.subs.values():
            s.scan()

    def getsub(self, key):
        if not self.subs.has_key(key):
            # return temporary dummy entry
            return DiffBlk("NUL",key,[])
        return self.subs[key]

def gendiff(csv,row,blks,key,rank,lev,end):
    row = row[:]
    for i in range(lev, len(row)): row[i] = "xxx"
    row[lev] = key

    if lev >= end:
        for k in ("pri","sha","cln"):
            p = map(lambda x:getattr(x,k), blks)
            v = calc_rank(p)
            p = map(lambda x:x.get(k), blks)
            if v >= rank:
                i = 5
                row[i] = k
                i += 1
                for t in p:
                    row[i] = t
                    i += 1
                row[i] = "%.2f"% v
                csv.append(row[:])
                #print row
    else:
        keys = map(lambda x:x.subs.keys(), blks)
        keys = reduce(lambda a,b:a+b, keys, [])
        keys = uniqlist(keys)

        for key in keys:
            temp = map(lambda x:x.getsub(key), blks)
            gendiff(csv,row,temp,key,rank,lev+1,end)

# =============================================================================
# DIFF
# =============================================================================

def bool(s):
    try:
        return int(s)
    except ValueError:
        pass
    s = s.lower()
    if s in ("yes","y", "true","t"):
        return 1
    return 0

def appdiff(args):
    help = APPDIFF_HELP
    srce = []
    args.reverse()
    level = None
    dest  = None
    mode  = None
    dest  = None
    rank  = 4
    easy  = None # default to nonfiltered output

    while args:
        a = args.pop()
        if a[:1] != '-':
            srce.append(a)
            continue
        if a[:2] == '--':
            if '=' in a:
                k,v = a.split("=",1)
            else:
                k,v = a, None
        else:
            k,v = a[:2],a[2:]

        if k in ("-h", "--help"):
            help = help.replace("<TOOL_NAME>", TOOL_NAME)
            help = help.replace("<TOOL_VERS>", TOOL_VERS)
            sys.stdout.write(help)
            sys.exit(0)
        elif k in ("-V", "--version"):
            print TOOL_VERS
            sys.exit(0)
        elif k in ("-v", "--verbose"):
            verbose()
        elif k in ("-q", "--quiet"):
            quiet()
        elif k in ("-s", "--silent"):
            silent()
        elif k in ("-l", "--level"):
            level = int(v or args.pop())
        elif k in ("-r", "--min-rank"):
            rank = float(v or args.pop())
        elif k in ("-m", "--mode"):
            mode = ".%s" % (v or args.pop())
        elif k in ("-o", "--output"):
            dest = v or args.pop()
        elif k in ("-e", "--filtered-output"):
            easy = bool(v or args.pop())
        else:
            fatal("Unknown option: %s" % repr(a))

    if dest != None:
        if mode == None:
            mode = os.path.splitext(dest)[1]

    if mode == None:
        mode = ".csv"

    if level == None and dest != None:
        t = os.path.basename(dest)
        t = t.split(".")
        while not level and t:
            e = t.pop()
            if   e == "sys": level = 0
            elif e == "app": level = 1
            elif e == "pid": level = 2
            elif e == "sec": level = 3
            elif e == "obj": level = 4

    if rank == None:
        rank = 8

    if level == None:
        level = 3 # upto map type: code, data, ...

    if mode not in (".csv",".html"):
        fatal("unknown export mode: %s" % repr(mode))

    if easy == None:
        if mode == ".csv":
            easy = 0
        else:
            easy = 1

    level = min(4, max(0, level))
    #print>>sys.stderr, repr((dest,mode,level,rank))

    # -- import capture data --
    data = []
    for path in srce:
        maps = import_normalized(path)
        #blck = DiffBlk("CAP", path, maps)
        blck = DiffBlk("CAP", "cap", maps)
        data.append(blck)

    # -- scanning capture data --
    for blck in data:
        # scan applications
        for name in uniqlist(map(lambda x:x.name, blck.maps)):
            maps = filter(lambda x:x.name==name, blck.maps)
            appl = DiffBlk("APP", name, maps, blck)

            # enumerate pids
            base,pids = 1,{}
            for pid in uniqlist(map(lambda x:x.pid, appl.maps)):
                if not pids.has_key(pid):
                    pids[pid] = base + len(pids)
            for m in appl.maps:
                m.pid = pids[m.pid]

            # scan application instances
            for pid in uniqlist(map(lambda x:x.pid, appl.maps)):
                maps = filter(lambda x:x.pid==pid, appl.maps)
                pid  = DiffBlk("PID", pid, maps, appl)

                # scan maps by type
                for type in uniqlist(map(lambda x:x.type, pid.maps)):
                    maps = filter(lambda x:x.type==type, pid.maps)
                    sec = DiffBlk("SEC", type, maps, pid)

                    # scan maps by object path
                    for path in uniqlist(map(lambda x:x.path, sec.maps)):
                        maps = filter(lambda x:x.path==path, sec.maps)
                        obj   = DiffBlk("OBJ", path, maps, sec)

        # recursively calculate memory values
        blck.scan()
        #blck.emit()

    # -- generate smaps diff table --
    v = map(lambda x:"CAP%d"%(x+1), range(len(srce)))
    v = ["Sys","Cmd","Pid","Type","Path","Value"] + v + ["RANK"]
    csv = [v]
    gendiff(csv,csv[-1][:], data, "sys", rank, 0, level)

## QUARANTINE     N=len(csv[0])
## QUARANTINE     for i in csv:
## QUARANTINE   print string.join(map(str, i))
## QUARANTINE   assert len(i) == N

    # -- make it easier to read: remove vertical redundancy --
    if easy:
        csv.reverse()
        cols = range(5)
        prev = csv.pop()
        done = [prev]
        if csv:
            prev = csv.pop()
            done.append(prev)
        while csv:
            curr = csv.pop()
            temp = curr[:]
            for c in cols:
                if prev[c] == temp[c]:
                    temp[c] = ""
                else:
                    if c < 2:
                        done.append(map(lambda x:"", curr))
                    break
            done.append(temp)
            prev = curr
        csv = done

    # -- rip off exess columns --
    csv = map(lambda x:x[1:1+level]+x[5:], csv)

    if dest == None:
        file = sys.stdout
    else:
        file = open(dest, "w")
    debug("%s: writing ..." % (dest or "stdout"+mode))

    if mode == ".csv":
        # -- generate csv output --
        print>>file, "generator=%s %s" % (TOOL_NAME, TOOL_VERS)
        for i in range(len(srce)):
            print>>file, "CAP%d=%s" % (i+1, srce[i])
        print>>file,""
        for i in csv:
            print>>file, string.join(map(str, i),",")
        print>>file,""

    elif mode == ".html":
        # -- generate html output --
        print>>file, "<html><head><title>SMAPS DIFF</title></head><body>"
        print>>file, "<h1>SMAPS DIFF</h1>"
        print>>file, "<p>"
        for i in range(len(srce)):
            print>>file, "CAP%d = %s<br>" % (i+1, srce[i])
        csv = map(lambda y:map(lambda x:str(x) or None,y),csv)
        print>>file, dotable(csv,1,level)
        print>>file, "</body></html>"
    else:
        assert 0

    file = None
    # -- done --

# =============================================================================
# FLATTEN
# =============================================================================

def flatten(args):
    help = FLATTEN_HELP
    srce = []
    dest = None
    args.reverse()
    while args:
        a = args.pop()
        if a[:1] != '-':
            srce.append(a)
            continue
        if a[:2] == '--':
            if '=' in a:
                k,v = a.split("=",1)
            else:
                k,v = a,None
        else:
            k,v = a[:2],a[2:]

        if k in ("-h", "--help"):
            help = help.replace("<TOOL_NAME>", TOOL_NAME)
            help = help.replace("<TOOL_VERS>", TOOL_VERS)
            sys.stdout.write(help)
            sys.exit(0)
        elif k in ("-V", "--version"):
            print TOOL_VERS
            sys.exit(0)
        elif k in ("-v", "--verbose"):
            verbose()
        elif k in ("-q", "--quiet"):
            quiet()
        elif k in ("-s", "--silent"):
            silent()

        elif k in ("-o", "--output"):
            dest = v or args.pop()
        elif k in ("-f", "--input"):
            srce.append(v or args.pop())
        else:
            fatal("Unknown option: %s" % repr(a))

    for srce in srce:
        if not dest:
            dest = os.path.splitext(srce)[0] + ".flat"
        assert srce != dest
        caps = import_capture(srce)
        cap_save(caps, dest)
        dest = None

# =============================================================================
# NORMALIZE
# =============================================================================
def normalize(args):
    help = NORMALIZE_HELP
    srce = []
    dest = None
    args.reverse()
    while args:
        a = args.pop()
        if a[:1] != '-':
            srce.append(a)
            continue
        if a[:2] == '--':
            if '=' in a:
                k,v = a.split("=",1)
            else:
                k,v = a,None
        else:
            k,v = a[:2],a[2:]

        if k in ("-h", "--help"):
            help = help.replace("<TOOL_NAME>", TOOL_NAME)
            help = help.replace("<TOOL_VERS>", TOOL_VERS)
            sys.stdout.write(help)
            sys.exit(0)
        elif k in ("-V", "--version"):
            print TOOL_VERS
            sys.exit(0)
        elif k in ("-v", "--verbose"):
            verbose()
        elif k in ("-q", "--quiet"):
            quiet()
        elif k in ("-s", "--silent"):
            silent()

        elif k in ("-o", "--output"):
            dest = v or args.pop()
        elif k in ("-f", "--input"):
            srce.append(v or args.pop())
        else:
            fatal("Unknown option: %s" % repr(a))

    for srce in srce:
        if not dest:
            dest = os.path.splitext(srce)[0] + ".csv"
        assert srce != dest
        maps = import_normalized(srce)
        maps_save_csv(maps, dest)
        dest = None

# =============================================================================
# ANALYZE
# =============================================================================
def analyze(args):
    help = ANALYZE_HELP
    srce = []
    dest = None

    args.reverse()
    while args:
        a = args.pop()
        if a[:1] != '-':
            srce.append(a)
            continue
        if a[:2] == '--':
            if '=' in a:
                k,v = a.split("=",1)
            else:
                k,v = a,None
        else:
            k,v = a[:2],a[2:]

        if k in ("-h", "--help"):
            help = help.replace("<TOOL_NAME>", TOOL_NAME)
            help = help.replace("<TOOL_VERS>", TOOL_VERS)
            sys.stdout.write(help)
            sys.exit(0)
        elif k in ("-V", "--version"):
            print TOOL_VERS
            sys.exit(0)
        elif k in ("-v", "--verbose"):
            verbose()
        elif k in ("-q", "--quiet"):
            quiet()
        elif k in ("-s", "--silent"):
            silent()

        elif k in ("-o", "--output"):
            dest = v or args.pop()
        elif k in ("-f", "--input"):
            srce.append(v or args.pop())
        else:
            fatal("Unknown option: %s" % repr(a))

    for srce in srce:
        if not dest:
            dest = os.path.splitext(srce)[0] + ".html"
        assert srce != dest
        apps,appid,objs,objid,sysest,sysmax,appmax = import_processed(srce)
        data_to_html(dest, apps,appid,objs,objid,sysest,sysmax,appmax)
        dest = None

# =============================================================================
# APPVALS
# =============================================================================

def appvals(args):

    help = APPVALS_HELP
    srce = []
    dest = None

    args.reverse()
    while args:
        a = args.pop()
        if a[:1] != '-':
            srce.append(a)
            continue
        if a[:2] == '--':
            if '=' in a:
                k,v = a.split("=",1)
            else:
                k,v = a,None
        else:
            k,v = a[:2],a[2:]

        if k in ("-h", "--help"):
            help = help.replace("<TOOL_NAME>", TOOL_NAME)
            help = help.replace("<TOOL_VERS>", TOOL_VERS)
            sys.stdout.write(help)
            sys.exit(0)
        elif k in ("-V", "--version"):
            print TOOL_VERS
            sys.exit(0)
        elif k in ("-v", "--verbose"):
            verbose()
        elif k in ("-q", "--quiet"):
            quiet()
        elif k in ("-s", "--silent"):
            silent()

        elif k in ("-o", "--output"):
            dest = v or args.pop()
        elif k in ("-f", "--input"):
            srce.append(v or args.pop())
        else:
            fatal("Unknown option: %s" % repr(a))

    for srce in srce:
        if not dest:
            dest = os.path.splitext(srce)[0] + ".apps"
        assert srce != dest
        apps,appid,objs,objid,sysest,sysmax,appmax = import_processed(srce)
        data_to_appvals(dest, apps,appid,objs,objid,sysest,sysmax,appmax)
        dest = None

# =============================================================================
# MULTIMODE MAIN ENTRY
# =============================================================================

if __name__ == "__main__":

    args = sys.argv[1:]

    if   "analyze"   in PROGNAME: analyze(args)
    elif "normalize" in PROGNAME: normalize(args)
    elif "flatten"   in PROGNAME: flatten(args)
    elif "appvals"   in PROGNAME: appvals(args)
    elif "diff"      in PROGNAME: appdiff(args)
    else:
        print>>sys.stderr, "Unknown operation mode: %s" % PROGNAME
        assert 0
    sys.exit(0)
