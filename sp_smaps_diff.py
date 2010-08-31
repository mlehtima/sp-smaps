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
# File: sp_smaps_diff.py
# 
# Author: Simo Piiroinen
# 
# -----------------------------------------------------------------------------
# 
# History:
#
# 12-Sep-2005 Simo Piiroinen
# - added options for version & usage queries
# - added option --level for setting report level
# - added option --min-rank for output filtering
#
# 08-Sep-2005 Simo Piiroinen
# - initial version
# =============================================================================

import sys,os,string,math

TOOL_NAME = "sp_smaps_diff"
TOOL_VERS = "0.0.0"
TOOL_HELP = """\
NAME
  <TOOL_NAME>

SYNOPSIS
  <TOOL_NAME> [options] <smaps-csv-file> ...

DESCRIPTION
  This tool allows making various operations on CSV format data.

OPTIONS
  -h | --help
        This help text
  -V | --version
        Tool version
  -l# | --level=#
        Display results at level:
	    1 = command
	    2 = command, pid
	    3 = command, pid, type
	    4 = command, pid, type, name
  -r# | --min-rank=#
  	Omit results with rank < given number.

EXAMPLES
  % <TOOL_NAME> --level=3 *after_boot*.csv
  
      Shows difference of smaps snapshots at memory usage down
      to level of mapping types (code, data, anon).

COPYRIGHT
  Copyright (C) 2004-2007 Nokia Corporation.

  This is free software.  You may redistribute copies of it under the
  terms of the GNU General Public License v2 included with the software.
  There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
  sp_smaps_snapshot, sp_smaps_normalize, sp_smaps_analyze 
"""


EMIT_SEPARATOR_ROWS = 0

SEP=","

smapkeys = "Name,Pid,Prot,Base".split(",")
smapvals = "Shared,Private".split(",")
#smapvals = "Clean,Shared,Private".split(",")

diffkeys = "Cmd,Pid,Type,Path".split(",")
diffvals = smapvals

def vsum(a,b):
    if a == None:
	a = [0]*len(b)
    return map(sum, zip(a,b))


def combine(rows, klen):
    zero = [0]*len(smapvals)
    m = {}
    for k,v in rows:
	k = k[:klen]
	m[k] = zero
    for k,v in rows:
	k = k[:klen]
	m[k] = map(sum, zip(m[k],v))

    return m
    #rows = m.items()
    #rows.sort()
    #return rows
    

def import_csv_data(path):
    labs = []
    rows = []
    
    text = open(path).readlines()
    text = map(string.rstrip, text)
    text.reverse()
    
    while text and '=' in text[-1]:
	text.pop()
    
    while text and not text[-1]:
	text.pop()

    if text:
	labs = text.pop().split(",")

    if not labs:
	print>>sys.stderr, "%s: empty or missing label row" % path
	return None


    try:
	keys = map(labs.index, smapkeys)
	vals = map(labs.index, smapvals)
    except ValueError:
	print>>sys.stderr,"%s: BROKEN CSV" % path
	sys.exit(1)
    
    while text and text[-1]:
	cols = text.pop().split(",")
	k = map(lambda x:cols[x], keys)
	v = map(lambda x:cols[x], vals)
	v = map(int, v)
	cmd,pid,prot,path = k
	
	if path == "-":
	    prot = "anon"
	    path = "(anon)"
	elif 'x' in prot:
	    prot = "text"
	else:
	    prot = "data"
	    
	k = cmd,pid,prot,path
	rows.append((k,v))

    #rows = combine(rows, len(keys))
    #rows = rows.items()
	
    return rows


def output(rows, keys):
    dummy = [None]*keys
    p = dummy
    c = range(keys)
    r = range(len(diffvals))
    
    for k,v in combine(rows, keys):
	h = []
	for i in c:
	    if p[i] != k[i]:
		if i == 0: h.append("-"*64)
		h.append("  "*i + k[i])
		p = dummy
	h = string.join(h,"\n")
	p = k
	
	if sum(v) == 0: continue
	
	print h
	for i in r:
	    if v[i] == 0: continue
	    print "\t\t\t\t%-7s %d" % (diffvals[i], v[i])
	


def bol_same(a,b):
    i,n = 0,min(len(a),len(b))
    while i < n and a[i] == b[i]:
	i += 1
    return i
def eol_same(a,b):
    i,n = -1,-min(len(a),len(b))
    while i > n and a[i] == b[i]:
	i -= 1
    return -(i+1)
	    
def trimpaths(v):
    v = map(os.path.basename, v)
    i = min(map(lambda x:bol_same(v[0],x), v))
    v = map(lambda x:x[i:], v)
    i = min(map(lambda x:eol_same(v[0],x), v))
    v = map(lambda x:x[:-i], v)
    return v

def vvar(v):
    v = v[:]
    v.sort()
    n = len(v)
    m = (v[(n+0)/2]+v[(n-1)/2])*0.5
    #m = float(sum(v))/len(v)
    s = 0.0
    for i in v:
	s += (m-i)**2.0
    return math.sqrt(s) / n

if __name__ == "__main__":
    RANK = 2.0
    KLEN = 2
    args = []
    temp = sys.argv[1:]
    temp.reverse()
    while len(temp):
	a = temp.pop()
	if a[:1] != '-':
	    args.append(a)
	    continue
	k,v = a[:2],a[2:]

	if k == "--":
	    if '=' in a:
		k,v = a.split('=',1)
	    else:
		k,v = a,''
	
	if   k in ("-h", "--help"):
	    s = TOOL_HELP
	    s = s.replace("<TOOL_NAME>", TOOL_NAME)
	    s = s.replace("<TOOL_VERS>", TOOL_VERS)
	    print s
	    sys.exit(0)
	elif k in ("-V", "--version"):
	    print TOOL_VERS
	    sys.exit(0)
	elif k in ('-l', "--level"):
	    KLEN = int(v)
	elif k in ('-r', "--min-rank"):
	    RANK = float(v)
	else:
	    print>>sys.stderr, "unknown option: %s" % repr(a)
	    sys.exit(1)

    
## QUARANTINE     for s in trimpaths(args): print s
## QUARANTINE     sys.exit(1)
    
    # load data from normalized csv files
    data = []
    for path,name in zip(args, trimpaths(args)):
	rows = import_csv_data(path)
	rows = combine(rows, KLEN)
	data.append((name, rows))
	
	
    # generate global list of mapping keys
    keys = {}
    for path,rows in data:
	for k in rows.keys():
	    keys[k] = None
    keys = keys.keys()
    keys.sort()

    l = []
    c = range(KLEN)
    r = range(len(diffvals))
    for k in keys:
	if k[0] in ("busybox",):
	    continue
	for i in r:
	    s = []
	    v = []
	    for path,rows in data:
		if rows.has_key(k):
		    t = rows[k][i]
		    v.append(t)
		    s.append(str(t))
		else:
		    v.append(0)
		    s.append("n/a")
	    v = vvar(v)
	    if v < RANK: 
		continue
	    #s.append("%.1f" % v)
	    s.append("%.0f" % v)
	    v = list(k) + [diffvals[i]] + s
	    l.append(v)

    h = diffkeys[:KLEN] + ["Value"] + map(lambda x:x[0], data) + ["RANK"]
    print string.join(h, SEP)
    
	    
    d = ["--"]*(KLEN + 1 + len(data) + 1)
    p = d
    for v in l:
	if EMIT_SEPARATOR_ROWS:
	    if v[0] != p[0]:
	    	print "#"+string.join(d,SEP)
	    elif v[1] != p[1]:
	    	print "#"
	
	t = v[:]
	for i in c:
	    if p[i] == v[i]:
		v[i] = ""
	    else:
		p = d
	p = t
	#v = [""]*KLEN + v[KLEN:]
	print string.join(v, SEP)
	    
	
    sys.exit(0)
	
    
    ##########################################33
    
    dummy = [None]*KLEN
    c = range(KLEN)
    r = range(len(diffvals))
    
    p = dummy
    for k in keys:
	if k[0] in ("busybox",):
	    continue
	
## QUARANTINE 	d = ["","","",""]
## QUARANTINE 	for path,rows in data: d.append("%7.7s"%path)
## QUARANTINE 	d = string.join(d,SEP)
	
	h = []
	for i in c:
	    if p[i] != k[i]:
		if i == 0: 
		    h.append("-"*64)
		    #h.append(d)
		h.append("  "*i + k[i])
		p=dummy
	h = string.join(h,"\n")
	p = k
	#print h
	
	for i in r:
	    d = ["","","",diffvals[i]]
	    ne,nz = None,None
	    v = []
	    for path,rows in data:
		if rows.has_key(k):
		    t = rows[k][i]
		    v.append(t)
		    if t: nz=1
		    d.append("%7d" % t)
		else:
		    v.append(0)
		    d.append("%7s"%"n/a")
		    #d.append("%7s"%"--")
		    nz=1
		if v[0] != v[-1]: ne=1

	    d.append("%7.0f" % vvar(v))
	    d = string.join(d, SEP)
	    
	    #if ne or len(v)==1: 
	    if ne or (nz and len(v)==1):
		if h:
		    print h
		    h=None
		print d
    
	
## QUARANTINE 	for k,v in combine(rows, 2):
## QUARANTINE 	rows = combine(rows, 3)
## QUARANTINE 	for k,v in rows:
## QUARANTINE 	    print k,v
    sys.exit(0)


