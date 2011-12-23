Name: sp-smaps
Version: 0.4.2
Release: 1%{?dist}
Summary: /proc/pid/smaps snapshot generator
Group: Development/Tools
License: GPLv2+
URL: http://www.gitorious.org/+maemo-tools-developers/maemo-tools/sp-smaps
Source: %{name}-%{version}.tar.gz
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
%{_mandir}/man1/sp_smaps_filter.1.gz
%{_mandir}/man1/sp_smaps_diff.1.gz
%{_mandir}/man1/sp_smaps_normalize.1.gz
%{_mandir}/man1/sp_smaps_appvals.1.gz
%{_mandir}/man1/sp_smaps_analyze.1.gz
%{_mandir}/man1/sp_smaps_flatten.1.gz
%{_mandir}/man1/sp_smaps_sorted_totals.1.gz
%doc README.txt COPYING

%changelog
* Fri Dec 23 2011 Eero Tamminen <eero.tamminen@nokia.com> 0.4.2
  * Update to v0.4.2 with minor fixes

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
