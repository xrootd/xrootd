
Name: xrootd-macaroons
Version: 0.3.0
Release: 1%{?dist}
Summary: Macaroons support for XRootD

Group: System Environment/Daemons
License: LGPL
URL: https://github.com/bbockelm/xrootd-macaroons
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
BuildRequires: libuuid-devel
BuildRequires: libmacaroons-devel
BuildRequires: openssl-devel
BuildRequires: json-c-devel

%description
%{summary}

%prep
%setup -q

%build
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
%make_build

%install
%make_install

%files
%defattr(-,root,root,-)
%{_libdir}/libXrdMacaroons-4.so

%changelog
* Mon Jul 09 2018 Brian Bockelman <bbockelm@cse.unl.edu> - 0.3.0-1
- Add the concept of a max duration.
- Check the location matches the current location.
- Record ID usage in the logs.

* Mon Jul 09 2018 Brian Bockelman <bbockelm@cse.unl.edu> - 0.2.0-1
- Record authz name within the macaroon; set it on request.
- Allow multiple logging levels.
- Allow chaining of authorization libraries.

* Fri Jun 15 2018 Brian Bockelman <bbockelm@cse.unl.edu> - 0.1.0-1
- Initial macaroons build.

