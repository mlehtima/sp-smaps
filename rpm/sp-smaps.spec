Name:       sp-smaps
Summary:    Utilities for collecting whole system SMAPS data
Version:    0.4.2.2
Release:    1
License:    GPLv2
URL:        https://github.com/mer-tools/sp-smaps
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  libsysperf-devel

%description
Utilities for collecting whole system SMAPS data and post-processing the information in it to cross-linked HTML tables

%package doc
Summary:   Documentation for %{name}
Requires:  %{name} = %{version}-%{release}

%description doc
Man pages for %{name}.

%prep
%autosetup -n %{name}-%{version}

%build
# building is done in during install. Empty build section to avoid rpmlint warning

%install
%make_install

mkdir -p %{buildroot}%{_docdir}/%{name}-%{version}
install -m0644 README.txt %{buildroot}%{_docdir}/%{name}-%{version}

%files
%defattr(-,root,root,-)
%{_bindir}/sp_smaps_analyze
%{_bindir}/sp_smaps_appvals
%{_bindir}/sp_smaps_diff
%{_bindir}/sp_smaps_filter
%{_bindir}/sp_smaps_flatten
%{_bindir}/sp_smaps_normalize
%{_bindir}/sp_smaps_snapshot
%{_bindir}/sp_smaps_sorted_totals
%dir %{_datadir}/sp-smaps-visualize
%{_datadir}/sp-smaps-visualize/asc.gif
%{_datadir}/sp-smaps-visualize/bg.gif
%{_datadir}/sp-smaps-visualize/desc.gif
%{_datadir}/sp-smaps-visualize/expander.js
%{_datadir}/sp-smaps-visualize/jquery.metadata.js
%{_datadir}/sp-smaps-visualize/jquery.min.js
%{_datadir}/sp-smaps-visualize/jquery.tablesorter.js
%{_datadir}/sp-smaps-visualize/tablesorter.css
%license COPYING

%files doc
%defattr(-,root,root,-)
%{_mandir}/man1/sp_smaps*
%{_docdir}/%{name}-%{version}
