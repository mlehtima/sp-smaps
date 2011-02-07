# This file is part of sp-smaps.
#
# Copyright (C) 2004-2007 by Nokia Corporation
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
# File: Makefile
#
# Author: Simo Piiroinen
#
# -----------------------------------------------------------------------------
#
# History:
#
# 25-Feb-2009 Simo Piiroinen
# - fixed changelog source list
#
# 18-Jan-2007 Simo Piiroinen
# - sp_smaps_filter added to package
#
# 07-Apr-2006 Simo Piiroinen
# - added symlinking for multimode postprocessor
#
# 12-Sep-2005 Simo Piiroinen
# - added sp_smaps_diff
#
# 09-Sep-2005 Simo Piiroinen
# - added C version of sp_smaps_snapshot
#
# 07-Jul-2005 Simo Piiroinen
# - copied from sp-memtransfer
# =============================================================================

# -----------------------------------------------------------------------------
# Directory Configuration
# -----------------------------------------------------------------------------

NAME   ?= sp-smaps-visualize
ROOT   ?= /tmp/$(NAME)-testing
PREFIX ?= /usr
BIN    ?= $(PREFIX)/bin
MAN1   ?= $(PREFIX)/share/man/man1

# -----------------------------------------------------------------------------
# Common Compiler Options
# -----------------------------------------------------------------------------

CFLAGS += -Wall
CFLAGS += -std=c99
CFLAGS += -D_GNU_SOURCE
CFLAGS += -D_THREAD_SAFE
CFLAGS += -Werror

BUILD  ?= final

ifeq ($(BUILD),final)
CFLAGS  += -Os
LDFLAGS += -s
endif

ifeq ($(BUILD),debug)
CFLAGS  += -O0
CFLAGS  += -fno-inline
CFLAGS  += -g
LDFLAGS += -g
endif

ifeq ($(BUILD),gprof)
CFLAGS  += -O0
CFLAGS  += -g -pg
LDFLAGS += -g -pg
endif

# -----------------------------------------------------------------------------
# Measurement Package Files
# -----------------------------------------------------------------------------

BIN_MEASURE += sp_smaps_snapshot

MAN_MEASURE += $(patsubst %,%.1.gz,$(BIN_MEASURE))
ALL_MEASURE += $(BIN_MEASURE) $(MAN_MEASURE)

# -----------------------------------------------------------------------------
# Normalization Package Files
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Visualization Package Files
# -----------------------------------------------------------------------------

## QUARANTINE BIN_VISUALIZE += sp_smaps_normalize
## QUARANTINE BIN_VISUALIZE += sp_smaps_analyze
## QUARANTINE BIN_VISUALIZE += sp_smaps_diff

BIN_VISUALIZE += sp_smaps_filter

LNK_VISUALIZE += sp_smaps_analyze
LNK_VISUALIZE += sp_smaps_flatten
LNK_VISUALIZE += sp_smaps_normalize
LNK_VISUALIZE += sp_smaps_appvals
LNK_VISUALIZE += sp_smaps_diff

MAN_VISUALIZE += $(patsubst %,%.1.gz,$(BIN_VISUALIZE))
MAN_VISUALIZE += sp_smaps_sorted_totals.1.gz

ALL_VISUALIZE += $(BIN_VISUALIZE) $(MAN_VISUALIZE) $(LNK_VISUALIZE)

# -----------------------------------------------------------------------------
# Targets From All Packages
# -----------------------------------------------------------------------------

ALL_TARGETS += $(ALL_MEASURE) $(ALL_NORMALIZE) $(ALL_VISUALIZE)

# -----------------------------------------------------------------------------
# Top Level Targets
# -----------------------------------------------------------------------------

.PHONY: build install clean distclean mostlyclean tags

build:: $(ALL_TARGETS)

install:: install-measure install-visualize

mostlyclean::
	$(RM) *.o *~

clean:: mostlyclean
	$(RM) $(ALL_TARGETS)

distclean:: clean
	$(RM) tags

tags::
	ctags *.c *.h

.PHONY: tree stats changelog

tree:: install
	tree $(ROOT)

changelog:
	sp_gen_changelog >$@ *.c *.py Makefile

# -----------------------------------------------------------------------------
# Installation Macros & Rules
# -----------------------------------------------------------------------------

INSTALL_DIR = $(if $1, install -m755 -d $2/)
INSTALL_MAN = $(if $1, install -m644 $1 $2/)
INSTALL_BIN = $(if $1, install -m755 $1 $2/)
INSTALL_HDR = $(if $1, install -m644 $1 $2/)

install-%-man::
	$(call INSTALL_DIR,$^,$(ROOT)$(MAN1))
	$(call INSTALL_MAN,$^,$(ROOT)$(MAN1))

install-%-bin::
	$(call INSTALL_DIR,$^,$(ROOT)$(BIN))
	$(call INSTALL_BIN,$^,$(ROOT)$(BIN))

install-%-hdr::
	$(call INSTALL_DIR,$^,$(ROOT)$(HDR))
	$(call INSTALL_HDR,$^,$(ROOT)$(HDR))

# -----------------------------------------------------------------------------
# Compilation Macros & Rules
# -----------------------------------------------------------------------------

# C -> asm
%.q	: %.c
	$(CC) -S -o $@ $(CFLAGS) $<

# PY -> installable script
%	: %.py
	sp_fix_tool_vers -f$< -o$@
	@chmod a-w $@

# generating MAN pages
%.1.gz : %.1
	gzip -9 -c <$< >$@
%.1    : %
	sp_gen_manfile -c ./$< -o $@

# updating static libraries
lib%.a :
	$(AR) ru $@ $^

# dynamic libraries
%.so : %.o
	$(CC) -shared -o $@ $(LDFLAGS) $^ $(LD_LIBS)

# -----------------------------------------------------------------------------
# Measurement Package Installation
# -----------------------------------------------------------------------------

install-measure:: $(addprefix install-measure-,bin man)

install-measure-bin:: $(BIN_MEASURE)
install-measure-man:: $(MAN_MEASURE)

# -----------------------------------------------------------------------------
# Visualization Package Installation
# -----------------------------------------------------------------------------

install-visualize:: $(addprefix install-visualize-,bin lnk man)

install-visualize-bin:: $(BIN_VISUALIZE) sp_smaps_sorted_totals
install-visualize-lnk:: $(addprefix $(ROOT)$(BIN)/,$(LNK_VISUALIZE))
install-visualize-man:: $(MAN_VISUALIZE)
	$(call INSTALL_DIR,$^,$(ROOT)$(MAN1))
	$(call INSTALL_MAN,$^,$(ROOT)$(MAN1))
	ln -fs sp_smaps_filter.1.gz $(ROOT)$(MAN1)/sp_smaps_analyze.1.gz
	ln -fs sp_smaps_filter.1.gz $(ROOT)$(MAN1)/sp_smaps_appvals.1.gz
	ln -fs sp_smaps_filter.1.gz $(ROOT)$(MAN1)/sp_smaps_diff.1.gz
	ln -fs sp_smaps_filter.1.gz $(ROOT)$(MAN1)/sp_smaps_flatten.1.gz
	ln -fs sp_smaps_filter.1.gz $(ROOT)$(MAN1)/sp_smaps_normalize.1.gz

# -----------------------------------------------------------------------------
# Target specific Rules
# -----------------------------------------------------------------------------

sp_smaps_snapshot : LDLIBS += -lsysperf
sp_smaps_snapshot : sp_smaps_snapshot.o

$(addprefix $(ROOT)$(BIN)/,$(LNK_VISUALIZE)): sp_smaps_filter
	ln -fs $< $@

$(LNK_VISUALIZE): sp_smaps_filter
	ln -fs $< $@

# -----------------------------------------------------------------------------
# Dependency Scanning
# -----------------------------------------------------------------------------

ifneq ($(MAKECMDGOALS),depend)
include .depend
endif

depend:
	gcc -MM *.c > .depend

# -----------------------------------------------------------------------------
# EOF
# -----------------------------------------------------------------------------

sp_smaps_filter : LDLIBS += -lsysperf -lm
sp_smaps_filter : sp_smaps_filter.o symtab.o
