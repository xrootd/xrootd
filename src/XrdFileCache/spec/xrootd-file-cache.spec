
Name: xrootd-file-cache
Version: 0.4
Release: 1
Summary: A caching proxy implementation on top of Xrootd

Group: System Environment/Development
License: BSD
URL: https://github.com/bbockelm/xrootd-file-cache
# Generated from:
# git-archive master | gzip -7 > ~/rpmbuild/SOURCES/xrootd-file-cache.tar.gz
Source0: %{name}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires: xrootd-libs-devel
BuildRequires: xrootd-server-devel

%package devel
Summary: Development headers and libraries for Xrootd caching proxy plugin
Group: System Environment/Development

%description
%{summary}

%description devel
%{summary}

%prep
%setup -q -c -n %{name}-%{version}

%build
%cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .
make VERBOSE=1 %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xrootd
sed -e "s#@LIBDIR@#%{_libdir}#" test/xrootd.cfg > $RPM_BUILD_ROOT%{_sysconfdir}/xrootd/xrootd.sample.file-cache.cfg

# Notice that I don't call ldconfig in post/postun.  This is because xrootd-file-cache
# is really a loadable module, not a shared lib: it's not linked to all the xrootd
# libs necessary to load it outside xrootd.

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
# Note these are not versioned - they are modules, not shlibs
%{_libdir}/libXrdFileCache.so
%{_libdir}/libXrdFileCacheAllowAlways.so
%{_sysconfdir}/xrootd/xrootd.sample.file-cache.cfg

%files devel
%{_includedir}/Decision.hh
%{_bindir}/xrdreadv
%{_bindir}/xrdfragcp

%changelog
* Fri Nov 2 2012 Brian Bockelman <bbockelm@cse.unl.edu> - 0.4-1
- Initial packaging for the caching proxy.

