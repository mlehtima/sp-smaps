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
make install ROOT=%{buildroot}

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
%defattr(-,root,root,-)
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
%defattr(-,root,root,-)
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
