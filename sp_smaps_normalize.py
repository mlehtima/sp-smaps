#!/usr/bin/env python

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
# File: sp_smaps_normalize.py
# 
# Author: Simo Piiroinen
# 
# -----------------------------------------------------------------------------
# 
# History:
#
# 28-Feb-2006 Simo Piiroinen
# - /proc/pid/smaps output chaged -> parsing fixed
# - merged earlier COW changes by Ville Medeiros
#
# 08-Sep-2005 Simo Piiroinen
# - added option '--enumerate-pids'
#
# 07-Jul-2005 Simo Piiroinen
# - command line parser added
# - contollable verbosity
# - major cleanup
#
# 16-May-2005 Ville Medeiros
# - Add Cow(Copy on Write) information to output html pages
# - Add values(RSS, Shared, Private,Clean, Cow) to the output html pages
#
# 11-Mar-2005 Simo Piiroinen
# - renamed from 'smaps_pp.py' to 'smaps_to_csv.py'
#
# 03-Feb-2005 Simo Piiroinen
# - first version
# =============================================================================

import sys,os,string
from types import *

# ============================================================================
# Usage And Version Query Support
# ============================================================================

TOOL_NAME = os.path.splitext(os.path.basename(sys.argv[0]))[0]
TOOL_VERS = "0.0.0"
TOOL_HELP = """\
NAME
  <NAME>

SYNOPSIS
  <NAME> [options]

DESCRIPTION

    The tool reads smaps snapshots and writes CSV format data
    usable by sp_smaps_analyze.
    
    The input must be as from 'head -2000 /proc/[1-9]*/smap'.

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
  -f <source path> | --input=<source path>
        Input file to use instead of stdin.
  -o <destination path> | --output=<destination path>
        Output file to use instead of stdout.
  --obsfuscate
        Actual file names are replaced with enumerated stubs.

  --enumerate-pids=<yes|no>
        Pids for each command start from 100.
	
EXAMPLES
  % <NAME> -f smaps-after-boot.cap -o smaps-after-boot.csv

    Converts smaps-after-boot.cap to CSV format, suitable for
    further processing or viewing with spreadsheet program.

COPYRIGHT
  Copyright (C) 2004-2007 Nokia Corporation.

  This is free software.  You may redistribute copies of it under the
  terms of the GNU General Public License v2 included with the software.
  There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
  sp_smaps_snapshot, sp_smaps_analyze
"""
def tool_version():
    print TOOL_VERS
    sys.exit(0)
    
def tool_usage():
    s = TOOL_HELP
    s = s.replace("<NAME>", TOOL_NAME)
    s = s.replace("<VERS>", TOOL_VERS)
    sys.stdout.write(s)
    sys.exit(0)

# ============================================================================
# Message API
# ============================================================================

msg_progname = TOOL_NAME
msg_verbose  = 3

def msg_emit(lev,tag,msg):
    if msg_verbose >= lev:
    	msg = string.split(msg,"\n")
    	msg = map(string.rstrip, msg)
    	while msg and msg[-1] == "":
	    msg.pop()
	    
	pad = "|" + " " * (len(tag)-1)
	
    	for s in msg:
	    s = string.expandtabs(s)
	    print "%s%s" % (tag, s)
	    tag = pad

def msg_fatal(msg):
    msg_emit(1, msg_progname + ": FATAL: ", msg)
    sys.exit(1)
    
def msg_error(msg):
    msg_emit(2, msg_progname + ": ERROR: ", msg)
    
def msg_warning(msg):
    msg_emit(2, msg_progname + ": Warning: ", msg)
    
def msg_silent():
    global msg_verbose
    msg_verbose = 0
    
def msg_more_verbose():
    global msg_verbose
    msg_verbose += 1
    
def msg_less_verbose():
    global msg_verbose
    msg_verbose -= 1
    



SEP=','

OBFUSCATE = 0

obfuscate_path = {}
import random
random.seed()

#----------------------------------------------------------------
def ext(path):
    path = os.path.basename(path)
    e = string.split(path, '.')[1:]
    e = filter(len, e)
    e = filter(lambda x: not('0'<=x[0]<='9'), e)
    if e: e.insert(0,'')
    return string.join(e,'.')

    
#----------------------------------------------------------------
class Map:
    def __init__(self, head,tail,prot,path):
        
        if 01 and OBFUSCATE and path:
	    if not obfuscate_path.has_key(path):
		b,e = "file",ext(path)
		n = len(obfuscate_path)
		obfuscate_path[path] = "%s%d%s" %(b,n,e)
            path = obfuscate_path[path]
        
        self.head = head
        self.tail = tail
        self.prot = prot
        self.path = path
        self.base = "-"
        self.vals = {}
            
        if path:
            self.base = os.path.basename(path)

        if OBFUSCATE:
            self.factor = random.uniform(0.8,1.8)
            
    def obfuscate(self):
        
        f = getattr(self, "factor", None)
        k = ("Shared_Clean","Shared_Dirty","Private_Clean","Private_Dirty","Estimated_Cow")
        if f != None:
            delattr(self, "factor")
            s = 0
	    for n in k:
                v = int(self.vals.get(n,0) * f + 0.5)
                s += v
                self.vals[n] = v
            self.vals["Rss"] = s
            self.vals["Size"] = int(self.vals.get("Size",0) * f + 0.5)
        
    def __repr__(self):
        return "%s:%s" % (self.prot, self.base)

    def empty(self): 
        return not self.vals

    def text(self):
        if self.path and 'x' in self.prot:
            return self.base
        return None
    def data(self):
        if self.path and not 'x' in self.prot:
            return self.base
        return None
    def heap(self):
        if self.path:
            return None
        return "<heap>"

    def get(self, key):
        self.obfuscate()
        return self.vals.get(key,0)
    
    def size(self):     
        return self.get("Size")
    def resident(self):
        return self.get("Rss")
    def clean(self):    
        return self.get("Shared_Clean")+self.get("Private_Clean")
    def shared(self):
        return self.get("Shared_Dirty")
    def private(self):
        return self.get("Private_Dirty")
    def cow(self):
	#return self.get("Estimated_Cow")
	cw = self.get("Shared_Clean") + self.get("Shared_Dirty")
	if cw > 0 and 'w' in self.prot and not 's' in self.prot:
	    return cw
	return 0
    
#----------------------------------------------------------------
class Pid:
    def __init__(self, pid):
        self.pid  = pid
        self.maps = []
        
    def __repr__(self):
        return "Pid:%d" % self.pid
    
    def empty(self): return not self.maps
    
    def appname(self):
        for m in self.maps:
            if 'x' in m.prot and m.path:
                return m.base
        return "<unknown>"
        
#----------------------------------------------------------------
# ==> /proc/1/smaps <==

        
def pid_p(s):
    if s[:3] != '==>':
        return None
    if s[-3:] != '<==':
        return None
    
    s = s[3:-3]
    s = string.split(s, '/')[2]
    try:
        s = int(s)
    except ValueError:
        return None
    
    return Pid(s)
    
pid_labs = ("Pid","Name")
    
#----------------------------------------------------------------
# 00008000-0000f000 r-xp /sbin/init

def map_p(s):
    path = None
    i = s.find("/")
    if i != -1:
	path = s[i:].strip()
	s = s[:i]
    
    s = s.split()
    if len(s) < 2:
	return None
    addr,prot = s[:2]
    i = string.find(addr, '-')
    if i != 8:
        return None
    
    head = long(addr[:i+0], 16)
    tail = long(addr[i+1:], 16)
    
    return Map(head,tail,prot,path)
    
## QUARANTINE     s = string.split(s)
## QUARANTINE     if len(s) < 3:
## QUARANTINE         s.append(None)
## QUARANTINE     if len(s) != 3:
## QUARANTINE         return None
## QUARANTINE     
## QUARANTINE     addr,prot,path = s
## QUARANTINE     
## QUARANTINE     i = string.find(addr, '-')
## QUARANTINE     if i != 8:
## QUARANTINE         return None
## QUARANTINE     
## QUARANTINE     head = long(addr[:i+0], 16)
## QUARANTINE     tail = long(addr[i+1:], 16)
## QUARANTINE     
## QUARANTINE     return Map(head,tail,prot,path)

#map_labs = ("head","tail","prot","Path")
map_labs = ("Prot","Base")

#----------------------------------------------------------------
# Size:                28 kB


def val_p(s):
    s = string.split(s)
    if len(s) != 3:
        return None
    key,val,kb = s
    if key[-1:] != ':':
        return None
    if kb.upper() != "KB":
        return None
    
    val = long(val)
    
    return (key[:-1],val)

val_labs = ("Size","Rss","Clean","Shared","Private","Cow")
#val_labs = ("Size","Rss","ShCl","ShDt","PvCl","PvDt")
#val_keys = ("Size","Rss","Shared_Clean","Shared_Dirty","Private_Clean","Private_Dirty")



#----------------------------------------------------------------

size_warnings_cnt = 0
size_warnings_max = 8

def getopts(options, key, defval=""):
    return options.get(key, defval)

def getoptn(options, key, defval=0):
    if options.has_key(key):
	s = options[key]
	if s in ("y","yes"): return 1
	if s in ("n","no"): return 0
	return int(s)
    return defval

def smaps_normalize(input, output, options):
        
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # parse input
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if input == None:
	file = sys.stdin
	input = "<stdin>"
    else:
	file = open(input)

	
    pids, cpid, cmap = [], None, None
    
    for s in file:
        s = string.rstrip(s)
        if s == "": 
            continue
        
        #print>>sys.stderr, "<<<< %s" % s
        
        t = pid_p(s)
        if t != None:
            cpid = t
            cmap = None
            pids.append(cpid)
            #print>>sys.stderr, "Pid:", cpid
            continue
        
        if cpid:
            t = map_p(s)
            if t != None:
                cmap = t
                cpid.maps.append(cmap)
                #print>>sys.stderr, "MAP:", cmap
                continue
            
            if cmap:
                t = val_p(s)
                if t != None:
                    k,v = t
                    cmap.vals[k] = v
                    #print>>sys.stderr, "VAL:", k,v
                    continue
                
        msg_warning("IGN: %s" % s)
	
    file = None
    
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # tabulate
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    
    labs = pid_labs + map_labs + val_labs

    rows = []
    
    for cpid in pids:
        for cmap in cpid.maps:
            cols = [cpid.pid, cpid.appname()]
            cols.extend((cmap.prot, cmap.base))
            
            sz = cmap.size()
            rs = cmap.resident()
            cl = cmap.clean()
            sh = cmap.shared()
            pv = cmap.private()
	    cw = cmap.cow()

            
            if rs > sz or rs != cl + sh + pv:
		global size_warnings_cnt
		if size_warnings_cnt < size_warnings_max:
		    msg = []
                    msg.append("SIZE MISMATCH")
                    msg.append(repr(cpid))
                    msg.append(repr(cmap))
		    msg.append("Size = %d" % sz)
		    msg.append("RSS  = %d" % rs)
		    msg.append("Pri  = %d" % pv)
		    msg.append("Sha  = %d" % sh)
		    msg.append("Cln  = %d" % pv)
		    msg.append("Cow  = %d" % cw)
		    if rs > sz:
		    	msg.append("PROBLEM: RSS > Size")
		    if rs != cl + sh + pv:
		    	msg.append("PROBLEM: RSS != Pri + Sha + Cln")
		    msg_warning(string.join(msg,"\n"))
		    
		size_warnings_cnt += 1
		if size_warnings_cnt == size_warnings_max:
		    msg_warning("suppressing further SIZE MISMATCH warnings")
		
		
            cols.extend((sz,rs,cl,sh,pv,cw))
            assert len(cols) == len(labs)
            rows.append(cols)


    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # output in csv format
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    if output == None:
	file = sys.stdout
	output = "<stdout>"
    else:
	file = open(output,"w")
    
    if getoptn(options, "enumerate-pids", 0):
	labs = list(labs)
	c_pid  = labs.index("Pid")
	c_name = labs.index("Name")
	names = {}
	for r in rows:
	    p = r[c_pid]
	    n = r[c_name]
	    if not names.has_key(n):
		names[n] = {}
	    if not names[n].has_key(p):
		names[n][p] = str(100 + len(names[n]))
	    r[c_pid] = names[n][p]
	
    print>>file, string.join(map(str, labs),SEP)
    for cols in rows:
        print>>file, string.join(map(str, cols),SEP)
        

    m={}
    for k,v in obfuscate_path.items():
	if m.has_key(v):
	    msg_warning("%s: %s and %s" % (v,m[v],k))
	else:
	    m[v]=k
	    
    file = None
    
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    # done
    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


def main():
    global OBFUSCATE
    
    # - - - - - - - - - - - - - - - - - - - - - - - -
    # defaults
    # - - - - - - - - - - - - - - - - - - - - - - - -
    
    input  = None # stdin
    output = None # stdout

    options = {
    "enumerate-pids" : 0,
    }
    
    # - - - - - - - - - - - - - - - - - - - - - - - -
    # parse command line options
    # - - - - - - - - - - - - - - - - - - - - - - - -
    
    args = sys.argv[1:]
    args.reverse()
    while args:
	arg = args.pop()
	if arg[:2] == "--":
	    if '=' in arg:
		key,val = string.split(arg,"=",1)
	    else:
		key,val = arg, ""
	else:
	    key,val = arg[:2],arg[2:]
	
	if   key in ("-h", "--help"):    tool_usage()
	elif key in ("-V", "--version"): tool_version()
	elif key in ("-f", "--input"):   input  = val or args.pop()
	elif key in ("-o", "--output"):  output = val or args.pop()
	elif key in ("-v", "--verbose"): msg_more_verbose()
	elif key in ("-q", "--quiet"):   msg_less_verbose()
	elif key in ("-s", "--silent"):  msg_silent()
	elif key in ("--obfuscate",): 
	    OBFUSCATE = 1
	elif options.has_key(key[2:]):
	    options[key[2:]] = val
	else:
	    msg_fatal("unknown option: %s\n(use --help for usage)\n" % repr(arg))
    

    smaps_normalize(input, output, options)
    
	    
if __name__ == "__main__":
    main()
    
