Name:       sp-smaps
Summary:    Utilities for collecting whole system SMAPS data
Version:    0.4.2.2
Release:    1
Group:      Development/Tools
License:    GPLv2
URL:        https://github.com/mer-tools/sp-smaps
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  python
BuildRequires:  libsysperf-devel

%description
Utilities for collecting whole system SMAPS data and post-processing the information in it to cross-linked HTML tables

%prep
%setup -q -n %{name}-%{version}

%build
# building is done in during install. Empty build section to avoid rpmlint warning

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%files
%defattr(-,root,root,-)
# >> files
%{_bindir}/sp_smaps_analyze
%{_bindir}/sp_smaps_appvals
%{_bindir}/sp_smaps_diff
%{_bindir}/sp_smaps_filter
%{_bindir}/sp_smaps_flatten
%{_bindir}/sp_smaps_normalize
%{_bindir}/sp_smaps_snapshot
%{_bindir}/sp_smaps_sorted_totals
%{_mandir}/man1/sp_smaps_analyze.1.gz
%{_mandir}/man1/sp_smaps_appvals.1.gz
%{_mandir}/man1/sp_smaps_diff.1.gz
%{_mandir}/man1/sp_smaps_filter.1.gz
%{_mandir}/man1/sp_smaps_flatten.1.gz
%{_mandir}/man1/sp_smaps_normalize.1.gz
%{_mandir}/man1/sp_smaps_snapshot.1.gz
%{_mandir}/man1/sp_smaps_sorted_totals.1.gz
%dir /usr/share/sp-smaps-visualize
/usr/share/sp-smaps-visualize/asc.gif
/usr/share/sp-smaps-visualize/bg.gif
/usr/share/sp-smaps-visualize/desc.gif
/usr/share/sp-smaps-visualize/expander.js
/usr/share/sp-smaps-visualize/jquery.metadata.js
/usr/share/sp-smaps-visualize/jquery.min.js
/usr/share/sp-smaps-visualize/jquery.tablesorter.js
/usr/share/sp-smaps-visualize/tablesorter.css
%doc COPYING README.txt
# << files


