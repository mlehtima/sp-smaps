Name: sp-smaps
Version: 0.4.0
Release: 1%{?dist}
Summary: /proc/pid/smaps snapshot generator
Group: Development/Tools
License: GPLv2+
URL: http://www.gitorious.org/+maemo-tools-developers/maemo-tools/sp-smaps
Source: %{name}_%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build
BuildRequires: libsysperf-devel, python

%description
 Contains /proc/pid/smaps snapshot and data visualization utilities.

%prep
%setup -q -n %{name}

%build
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

#
# Measurement package
#
%package -n %{name}-measure
Summary: /proc/pid/smaps snapshot generator
Group: Development/Tools

%description -n %{name}-measure
 Contains the `sp_smaps_snapshot' utility for snapshotting system state, that
 collects /proc/pid/smaps files for all processes. The snapshot file can be
 later analyzed for example with the tools from the sp-smaps-visualize package.

%files -n %{name}-measure
%defattr(-,root,root,-)
%{_bindir}/sp_smaps_snapshot
%{_mandir}/man1/sp_smaps_snapshot.1.gz
%doc README.txt COPYING

#
# Visualization package
#
%package -n %{name}-visualize
Summary: /proc/pid/smaps snapshot analyzer
Group: Development/Tools

%description -n %{name}-visualize
 Utilities for analyzing /proc/pid/smaps snapshots.

%files -n %{name}-visualize
%defattr(-,root,root,-)
%{_bindir}/sp_smaps_filter
%{_bindir}/sp_smaps_diff
%{_bindir}/sp_smaps_normalize
%{_bindir}/sp_smaps_appvals
%{_bindir}/sp_smaps_analyze
%{_bindir}/sp_smaps_flatten
%{_bindir}/sp_smaps_sorted_totals
%{_datadir}/%{name}-visualize/
%{_datadir}/%{name}-visualize/tablesorter.css
%{_datadir}/%{name}-visualize/desc.gif
%{_datadir}/%{name}-visualize/jquery.tablesorter.js
%{_datadir}/%{name}-visualize/expander.js
%{_datadir}/%{name}-visualize/asc.gif
%{_datadir}/%{name}-visualize/jquery.min.js
%{_datadir}/%{name}-visualize/bg.gif
%{_datadir}/%{name}-visualize/jquery.metadata.js
%{_mandir}/man1/sp_smaps_filter.1.gz
%{_mandir}/man1/sp_smaps_diff.1.gz
%{_mandir}/man1/sp_smaps_normalize.1.gz
%{_mandir}/man1/sp_smaps_appvals.1.gz
%{_mandir}/man1/sp_smaps_analyze.1.gz
%{_mandir}/man1/sp_smaps_flatten.1.gz
%{_mandir}/man1/sp_smaps_sorted_totals.1.gz
%doc README.txt COPYING

%changelog
* Tue Jun 07 2011 Eero Tamminen <eero.tamminen@nokia.com> 0.4.0
  * sp_smaps_snapshot changes:
    * remove parameter -f, allow only to read /proc/*/smaps.
    * give a warning for empty /proc/pid/smaps files.
  * sp_smaps_analyze HTML report updates:
    * Include local copies of all javascript files.
    * Remove the Referenced columns from non-summary tables.
    * Remove kernel threads from the report (as the smaps snapshot does not
      contain any real data for them).
    * Allow dynamic expand and collapse of the process hierarchy.
    * Truncate long mapping, process and library names.
    * Remove `RSS / Class' columns from the Object Values table.
    * Remove one-page mappings from the `Object Values' table.
    * Remove all-zero entries from the `Application Values' table. Also give a
      warning when this happens as it most likely indicates that the smaps
      data capture is incomplete.
    * Add new SMAPS based columns `Anonymous' and `Locked'.
    * The XREF tables are now sortable on-the-fly; initially by PSS.
    * Remove the dubious `COW' estimate.
    * Add new columns `Size / Class' for the `Application Values' table.
    * Add new columns `Resident Peak' and `Size Peak' to application summary
      tables.

* Mon Feb 07 2011 Eero Tamminen <eero.tamminen@nokia.com> 0.3.0

  * New post processing tool `sp_smaps_sorted_totals', that shows sorted
    totals of given mapping for all proecsses.
  * Several tweaks to `sp_smaps_analyze' HTML output:
    * Add tooltips for the columns in the Object Values table. The private
      clean, private dirty and PSS columns are based on summation, while the
      other columns show the maximum values.
    * Tables are now sorted according to the PSS columns. The 'Application
      Values' and 'Object Values' tables can be dynamically sorted by any
      column.
    * Report private and shared separately for clean memory.
    * The "Application XREF" and "Mapping XREF" tables can have hundreds of
      rows, so the table headers are repeated for improved readability.
    * Minimal table of contents added to top of main HTML.

* Mon Sep 06 2010 Eero Tamminen <eero.tamminen@nokia.com> 0.2.4

  * Remove nested functions in sp_smaps_filter.c, fixes executable stack and
    some crashes seen when running under ARM qemu.
  * Fix crash when trying to process non-existent input files.

* Wed Sep 01 2010 Eero Tamminen <eero.tamminen@nokia.com> 0.2.3

  * Add manual pages for all binaries in the sp-smaps-visualize package.
    Fixes: NB#187454

* Thu Aug 20 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.2.2

  * Added support of the new smaps fields - pss, swap, referenced. Fixes: NB#124861

* Fri Jun 26 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.2.1

  * Ignore & give a warning for smaps entries that do not match "==>
    /proc/[0-9]+/smaps <==", instead of dying in assert. This fixes
    cases where we have "==> /proc/self/smaps <==". Fixes: NB#124672
  * Dont try to remove threads from old style capture files.

* Mon May 11 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.2.0

  * Switch to sp_smaps_filter written in C. Fixes: NB#103757

* Mon Jul 28 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.8

  * Fixed incorrectly named reference to another manual page in
    sp_smaps_filter and a spelling issue in sp_smaps_analyze
    documentation. Fixes: NB#86493

* Wed Jun 18 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.7

  * Removed an obsolete script from the package.
  * sp_smaps_snapshot manual page cleanup.

* Tue Jun 03 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.6

  * Running sp_smaps_snapshot as a root from serial console caused a
    reboot due to automatic usage of realtime scheduling and elevated
    priority.  This behavior is now optional (-r switch) rather than
    default. Fixes: NB#86016

* Wed Nov 07 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.1.5

  * sp_smaps_analyze.py: Licencing information inconsistencies fixed.
    Fixes: NB#73435
  * sp_smaps_snapshot.c: Avoid calling basename with /proc/PID/cmdline
    strings that might confuse it - Fixes: NB#73438

* Wed Aug 29 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.1.4

  * Not all changed files were commited while making the previous
    release. Fixing that now. Fixes: NB#59897

* Wed Aug 29 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.1.3

  * Checked/fixed licensing information and documentation - Fixes: NB#59897

* Thu Jan 18 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.1.2

  * new postprocessor sp_smaps_filter
  * old python filters retained for compatibility

* Mon Apr 10 2006 Eero Tamminen <eero.tamminen@nokia.com> 0.1.1

  * Code cleanup & help fixes
  * Source package name change to reflect actual contents

* Fri Apr 07 2006 Eero Tamminen <eero.tamminen@nokia.com> 0.1.0
  * The sp_smaps_snapshot tool now interleaves smaps data with data from
    /proc/pid/status and /proc/pid/cmdline. This allows better handling
    of launchers and multimode binaries like busybox.
  * Use of 'head /proc/[1-9]*/smaps > foo.cap' discouraged as it misses
    the extra information provided by sp_smaps_snapshot. The format is
    still tolerated for handling old capture files.
  * New versions of smaps kernels use [heap] and [stack] type filenames
    for smaps output. These are utilized to obtain more fine grained
    type information.
  * The postprocessing scripts are now implemented as one multimode
    python script. This allows users to skip normalization step as
    all analysis tools can grok raw smaps data.
  * The sp_smaps_analyze now writes out also process hierarchy tree to
    make it easier to find processes of interest. Duplicate label rows
    added to long application and library listings.
  * New tool sp_smaps_appvals can be used to obtain essential memory
    figures from all applications in a capture file.
  * The sp_smaps_diff tool can output either csv or html format.

* Tue Feb 28 2006 Eero Tamminen <eero.tamminen@nokia.com> 0.0.6

  * sync to /proc/pid/smaps output changes
  * merged earlier COW changes by Ville Medeiros

* Mon Sep 12 2005 Eero Tamminen <eero.tamminen@nokia.com> 0.0.5

  * sp_smaps_diff:
    - typo fix in --level handling
    - added --min-rank option for output filtering

* Mon Sep 12 2005 Eero Tamminen <eero.tamminen@nokia.com> 0.0.4

  * sp_smaps_diff: shows differences between normalized smaps snapshots

* Fri Sep 09 2005 Eero Tamminen <eero.tamminen@nokia.com> 0.0.3

  * sp_smaps_snapshot now implemented in C

* Thu Jul 07 2005 Eero Tamminen <eero.tamminen@nokia.com> 0.0.2

  * Fixed: sp_smaps_analyze output directory

* Thu Jul 07 2005 Eero Tamminen <eero.tamminen@nokia.com> 0.0.1

  * Initial release.
