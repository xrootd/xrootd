
Name: xrootd-tpc
Version: 0.3.3
Release: 1%{?dist}
Summary: HTTP Third Party Copy plugin for XRootD

Group: System Environment/Daemons
License: BSD
URL: https://github.com/bbockelm/xrootd-tpc
# Generated from:
# git archive v%{version} --prefix=%{name}-%{version}/ | gzip -7 > ~/rpmbuild/SOURCES/%{name}-%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires: xrootd-devel
BuildRequires: xrootd-server-libs
BuildRequires: xrootd-server-devel
BuildRequires: xrootd-private-devel
BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: libcurl-devel

%description
%{summary}

%prep
%setup -q

sed -i 's|".*"|"%{version}"|' src/XrdTpcVersion.hh

%build
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
make VERBOSE=1 %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/libXrdHttpTPC-4.so

%changelog
* Tue Jan 02 2018 Brian Bockelman <bbockelm@cse.unl.edu> - 0.3.3-1
- Remove workaround from bad version of libdavix.

* Fri Dec 29 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 0.3.2-1
- Allow CA directory to be overridden in Xrootd.

* Fri Dec 29 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 0.3.1-1
- Add support for dCache-style TransferHeader.

* Mon Nov 13 2017 Brian Bockelman <bbockelm@cse.unl.edu> - 0.3-1
- Add support for redirections in COPY requests.
- Add RPM packaging.
