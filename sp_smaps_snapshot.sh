#!/bin/sh

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
# File: sp_smaps_snapshot.sh
# 
# Author: Simo Piiroinen
# 
# -----------------------------------------------------------------------------
# 
# History:
# 
# 07-Jul-2005 Simo Piiroinen
# - initial version
# =============================================================================

TOOL_NAME="sp_smaps_snapshot"
TOOL_VERS="0.0.0"

function show_version()
{
  echo "$TOOL_VERS"
  exit 0
}
function show_usage()
{
cat <<EOF
NAME
  $TOOL_NAME $TOOL_VERS - snapshot of /proc/pid/smaps files

SYNOPSIS
  $TOOL_NAME [options]

DESCRIPTION
  Dumps /proc/pid/smaps output for all processes with separators
  recognized by sp_smaps_normalize.

OPTIONS
  
  -h | --help
        This help text
  -V | --version
        Tool version
  -o <destination path> | --output=<destination path>
        Output file to use instead of stdout.
 
EXAMPLES
  $TOOL_NAME -o smaps-after-boot.cap

COPYRIGHT
  Copyright (C) 2004-2007 Nokia Corporation.

  This is free software.  You may redistribute copies of it under the
  terms of the GNU General Public License v2 included with the software.
  There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
  sp_smaps_normalize, sp_smaps_analyze
EOF
  exit 0
}


output=""

while [ "$#" -gt 0 ]; do
  arg=$1;shift
  case $arg in
    -h|--help)
      show_usage
      ;;
    -V|--version)
      show_version
      ;;
    -o)
      output=$1;shift
      ;;
    *)
      echo >&2 "Unknown option \"$arg\""
      echo >&2 "(use --help for usage)"
      exit 1
      ;;
  esac
done

if [ ! -f /proc/self/smaps ]; then
  echo >&2 "$TOOL_NAME: FATAL: system does not have smaps"
  exit 1
fi

if [ -z "$output" ]; then
  head -2000 /proc/[1-9]*/smaps
else
  head -2000 /proc/[1-9]*/smaps > "$output"
fi
