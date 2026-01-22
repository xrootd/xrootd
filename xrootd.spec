%bcond_with    asan
%bcond_with    ceph
%bcond_with    clang
%bcond_with    docs
%bcond_with    git

%bcond_without tests
%bcond_without xrdec

Name:		xrootd
Epoch:		1
Release:	1%{?dist}%{?with_clang:.clang}%{?with_asan:.asan}
Summary:	Extended ROOT File Server
Group:		System Environment/Daemons
License:	LGPL-3.0-or-later AND BSD-2-Clause AND BSD-3-Clause AND curl AND MIT AND Zlib
URL:		https://xrootd.org

%if !%{with git}
Version:	5.9.1
Source0:	https://xrootd.web.cern.ch/download/v%{version}/%{name}-%{version}.tar.gz
%else
%define git_version %(tar xzf %{_sourcedir}/%{name}.tar.gz -O xrootd/VERSION)
%define src_version %(sed -e "s/%%(describe)/v5.9-rc%(date +%%Y%%m%%d)/" <<< "%git_version")
%define rpm_version %(sed -e 's/v//; s/-rc/~rc/; s/-g/+git/; s/-/.post/; s/-/./' <<< "%src_version")
Version:	%rpm_version
Source0:	%{name}.tar.gz
%endif

%undefine __cmake_in_source_build

BuildRequires:	cmake
BuildRequires:	gcc-c++
BuildRequires:	gdb
BuildRequires:	which
BuildRequires:	make
BuildRequires:	pkgconfig
BuildRequires:	fuse-devel
BuildRequires:	krb5-devel
BuildRequires:	libcurl-devel
BuildRequires:	libxml2-devel
BuildRequires:	libzip-devel
BuildRequires:	ncurses-devel
BuildRequires:	openssl-devel
BuildRequires:	perl-generators
BuildRequires:	readline-devel
BuildRequires:	zlib-devel
BuildRequires:	selinux-policy-devel
BuildRequires:	systemd-rpm-macros
BuildRequires:	systemd-devel
BuildRequires:	python3-devel
BuildRequires:	python3-pip
BuildRequires:	python3-setuptools
BuildRequires:	python3-wheel
BuildRequires:	json-c-devel
BuildRequires:	libmacaroons-devel
BuildRequires:	libuuid-devel
BuildRequires:	voms-devel
BuildRequires:	scitokens-cpp-devel
BuildRequires:	davix-devel
BuildRequires:  libxcrypt-devel

%if %{with asan}
BuildRequires:	libasan
%endif

%if %{with ceph}
BuildRequires:	librados-devel
BuildRequires:	libradosstriper-devel
%endif

%if %{with clang}
BuildRequires:	clang
%endif

%if %{with docs}
BuildRequires:	doxygen
BuildRequires:	graphviz
BuildRequires:	python3-sphinx
%endif

%if %{with tests}
BuildRequires:	attr
BuildRequires:	coreutils
BuildRequires:	curl
BuildRequires:	davix
BuildRequires:	gtest-devel
BuildRequires:	krb5-server
BuildRequires:	krb5-workstation
BuildRequires:	openssl
BuildRequires:	procps-ng
BuildRequires:	sqlite
%endif

%if %{with xrdec}
BuildRequires:	isa-l-devel
%endif

Requires:	%{name}-client%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-server%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-selinux = %{epoch}:%{version}-%{release}

%description
The Extended root file server consists of a file server called xrootd
and a cluster management server called cmsd.

The xrootd server was developed for the root analysis framework to
serve root files. However, the server is agnostic to file types and
provides POSIX-like access to any type of file.

The cmsd server is the next generation version of the olbd server,
originally developed to cluster and load balance Objectivity/DB AMS
database servers. It provides enhanced capability along with lower
latency and increased throughput.

%package server
Summary:	XRootD server daemons
Group:		System Environment/Daemons
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-server-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	expect
Requires:	logrotate
%{?sysusers_requires_compat}
%{?systemd_requires}

%description server
This package contains the XRootD servers without the SELinux support.
Unless you are installing on a system without SELinux also install the
xrootd-selinux package.

%package selinux
Summary:	SELinux policy modules for the XRootD servers
Group:		System Environment/Base
BuildArch:	noarch
Requires:	selinux-policy
Requires(post):		policycoreutils
Requires(postun):	policycoreutils

%description selinux
This package contains SELinux policy modules for the xrootd-server package.

%package libs
Summary:	Libraries used by XRootD servers and clients
Group:		System Environment/Libraries

%description libs
This package contains libraries used by the XRootD servers and clients.

%package devel
Summary:	Development files for XRootD
Group:		Development/Libraries
Provides:	%{name}-libs-devel = %{epoch}:%{version}-%{release}
Provides:	%{name}-libs-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Obsoletes:	%{name}-libs-devel < %{epoch}:%{version}-%{release}

%description devel
This package contains header files and development libraries for XRootD
development.

%package client-libs
Summary:	Libraries used by XRootD clients
Group:		System Environment/Libraries
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description client-libs
This package contains libraries used by XRootD clients.

%package client-devel
Summary:	Development files for XRootD clients
Group:		Development/Libraries
Provides:	%{name}-cl-devel = %{epoch}:%{version}-%{release}
Provides:	%{name}-cl-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}
Obsoletes:	%{name}-cl-devel < %{epoch}:%{version}-%{release}

%description client-devel
This package contains header files and development libraries for XRootD
client development.

%package server-libs
Summary:	Libraries used by XRootD servers
Group:		System Environment/Libraries
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description server-libs
This package contains libraries used by XRootD servers.

%package server-devel
Summary:	Development files for XRootD servers
Group:		Development/Libraries
Requires:	%{name}-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-server-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description server-devel
This package contains header files and development libraries for XRootD
server development.

%package private-devel
Summary:	Private XRootD headers
Group:		Development/Libraries
Requires:	%{name}-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-server-devel%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description private-devel
This package contains some private XRootD headers. Backward and forward
compatibility between versions is not guaranteed for these headers.

%package client
Summary:	XRootD command line client tools
Group:		Applications/Internet
Provides:	%{name}-cl = %{epoch}:%{version}-%{release}
Provides:	%{name}-cl%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}
Obsoletes:	%{name}-cl < %{epoch}:%{version}-%{release}

%description client
This package contains the command line tools used to communicate with
XRootD servers.

%package fuse
Summary:	XRootD FUSE tool
Group:		Applications/Internet
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	fuse

%description fuse
This package contains the FUSE (file system in user space) XRootD mount
tool.

%package voms
Summary:	VOMS attribute extractor plugin for XRootD
Group:		System Environment/Libraries
Provides:	vomsxrd = %{epoch}:%{version}-%{release}
Provides:	%{name}-voms-plugin = %{epoch}:%{version}-%{release}
Provides:	xrdhttpvoms = %{epoch}:%{version}-%{release}
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Obsoletes:	%{name}-voms-plugin < 1:0.6.0-3
Obsoletes:	xrdhttpvoms < 0.2.5-9
Obsoletes:	vomsxrd < 1:0.6.0-4

%description voms
The VOMS attribute extractor plugin for XRootD.

%package scitokens
Summary:	SciTokens authorization support for XRootD
Group:		System Environment/Libraries
License:	Apache-2.0 AND BSD-2-Clause AND BSD-3-Clause
Requires:	%{name}-server%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description scitokens
This ACC (authorization) plugin for the XRootD framework utilizes the
SciTokens library to validate and extract authorization claims from a
SciToken passed during a transfer. Configured appropriately, this
allows the XRootD server admin to delegate authorization decisions for
a subset of the namespace to an external issuer.

%package -n xrdcl-http
Summary:	HTTP client plugin for XRootD
Group:		System Environment/Libraries
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description -n xrdcl-http
xrdcl-http is an XRootD client plugin which allows XRootD to interact
with HTTP repositories.

%if %{with ceph}
%package ceph
Summary:	XRootD plugin for interfacing with the Ceph storage platform
Group:		System Environment/Libraries
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description ceph
The xrootd-ceph is an OSS layer plugin for the XRootD server for
interfacing with the Ceph storage platform.
%endif

%package -n python%{python3_pkgversion}-%{name}
Summary:	Python 3 bindings for XRootD
Group:		System Environment/Libraries
%py_provides	python%{python3_pkgversion}-%{name}
Requires:	%{name}-libs%{?_isa} = %{epoch}:%{version}-%{release}
Requires:	%{name}-client-libs%{?_isa} = %{epoch}:%{version}-%{release}

%description -n python%{python3_pkgversion}-%{name}
This package contains Python 3 bindings for XRootD.

%package doc
Summary:	Developer documentation for the XRootD libraries
Group:		Documentation
BuildArch:	noarch

%description doc
This package contains the API documentation of the XRootD libraries.

%prep

%if %{with git}
%autosetup -n %{name}
%else
%autosetup -p1
%endif

%build

%if %{with clang}
export CC=clang
export CXX=clang++
%endif

%cmake \
    -DFORCE_ENABLED:BOOL=TRUE \
    -DENABLE_ASAN:BOOL=%{with asan} \
    -DENABLE_CEPH:BOOL=%{with ceph} \
    -DENABLE_FUSE:BOOL=TRUE \
    -DENABLE_KRB5:BOOL=TRUE \
    -DENABLE_MACAROONS:BOOL=TRUE \
    -DENABLE_READLINE:BOOL=TRUE \
    -DENABLE_SCITOKENS:BOOL=TRUE \
    -DENABLE_TESTS:BOOL=%{with tests} \
    -DENABLE_VOMS:BOOL=TRUE \
    -DENABLE_XRDCL:BOOL=TRUE \
    -DENABLE_XRDCLHTTP:BOOL=TRUE \
    -DENABLE_XRDEC:BOOL=%{with xrdec} \
    -DENABLE_XRDCLHTTP:BOOL=TRUE \
    -DXRDCL_ONLY:BOOL=FALSE \
    -DXRDCL_LIB_ONLY:BOOL=FALSE \
    -DENABLE_PYTHON:BOOL=TRUE \
    -DINSTALL_PYTHON_BINDINGS:BOOL=FALSE \
    -DXRD_PYTHON_REQ_VERSION=%{python3_version}

%cmake3_build

make -C config -f /usr/share/selinux/devel/Makefile

%if %{with docs}
doxygen Doxyfile
%endif

%if %{with tests}
%check
%ctest3
%endif

%install

%cmake3_install

# Remove test binaries and libraries
%if %{with tests}
	rm -f %{buildroot}%{_bindir}/test-runner
	rm -f %{buildroot}%{_bindir}/xrdshmap
	rm -f %{buildroot}%{_libdir}/libXrd*Tests*
	rm -f %{buildroot}%{_libdir}/libXrdClTestMonitor*.so
%endif

%if %{with ceph}
	rm -f %{buildroot}%{_libdir}/libXrdCephPosix.so
%endif

rm -f %{buildroot}%{python3_sitearch}/xrootd-*.*-info/direct_url.json
rm -f %{buildroot}%{python3_sitearch}/xrootd-*.*-info/RECORD
[ -r %{buildroot}%{python3_sitearch}/xrootd-*.*-info/INSTALLER ] && \
	sed s/pip/rpm/ -i %{buildroot}%{python3_sitearch}/xrootd-*.*-info/INSTALLER

%{__python3} -m pip install \
	--no-deps --ignore-installed --disable-pip-version-check --verbose \
	--prefix %{buildroot}%{_prefix} %{_vpath_builddir}/python

%if %{with docs}
LD_LIBRARY_PATH=%{buildroot}%{_libdir} \
PYTHONPATH=%{buildroot}%{python3_sitearch} \
PYTHONDONTWRITEBYTECODE=1 \
make -C python/docs html SPHINXBUILD=sphinx-build-3
%endif

# Service unit files
mkdir -p %{buildroot}%{_unitdir}
install -m 644 systemd/xrootd@.service %{buildroot}%{_unitdir}
install -m 644 systemd/xrootd@.socket %{buildroot}%{_unitdir}
install -m 644 systemd/xrdhttp@.socket %{buildroot}%{_unitdir}
install -m 644 systemd/cmsd@.service %{buildroot}%{_unitdir}
install -m 644 systemd/frm_xfrd@.service %{buildroot}%{_unitdir}
install -m 644 systemd/frm_purged@.service %{buildroot}%{_unitdir}

mkdir -p %{buildroot}%{_sysusersdir}
install -m 644 systemd/%{name}-sysusers.conf %{buildroot}%{_sysusersdir}/%{name}.conf

# Server config
mkdir -p %{buildroot}%{_sysconfdir}/%{name}
install -m 644 -p config/%{name}-clustered.cfg \
	%{buildroot}%{_sysconfdir}/%{name}/%{name}-clustered.cfg
install -m 644 -p config/%{name}-standalone.cfg \
	%{buildroot}%{_sysconfdir}/%{name}/%{name}-standalone.cfg
install -m 644 -p config/%{name}-filecache-clustered.cfg \
	%{buildroot}%{_sysconfdir}/%{name}/%{name}-filecache-clustered.cfg
install -m 644 -p config/%{name}-filecache-standalone.cfg \
	%{buildroot}%{_sysconfdir}/%{name}/%{name}-filecache-standalone.cfg
sed 's!/usr/lib64/!!' config/%{name}-http.cfg > \
	%{buildroot}%{_sysconfdir}/%{name}/%{name}-http.cfg

# Client config
mkdir -p %{buildroot}%{_sysconfdir}/%{name}/client.plugins.d
install -m 644 -p config/client.conf \
	%{buildroot}%{_sysconfdir}/%{name}/client.conf
sed 's!/usr/lib/!!' config/client-plugin.conf.example > \
	%{buildroot}%{_sysconfdir}/%{name}/client.plugins.d/client-plugin.conf.example
sed -e 's!/usr/lib64/!!' -e 's!-5!!' config/recorder.conf > \
	%{buildroot}%{_sysconfdir}/%{name}/client.plugins.d/recorder.conf
sed 's!/usr/lib64/!!' config/http.client.conf.example > \
	%{buildroot}%{_sysconfdir}/%{name}/client.plugins.d/xrdcl-http-plugin.conf

chmod 644 %{buildroot}%{_datadir}/%{name}/utils/XrdCmsNotify.pm

sed 's!/usr/bin/env perl!/usr/bin/perl!' -i \
	%{buildroot}%{_datadir}/%{name}/utils/netchk \
	%{buildroot}%{_datadir}/%{name}/utils/XrdCmsNotify.pm \
	%{buildroot}%{_datadir}/%{name}/utils/XrdOlbMonPerf

sed 's!/usr/bin/env bash!/bin/bash!' -i %{buildroot}%{_bindir}/xrootd-config

mkdir -p %{buildroot}%{_sysconfdir}/%{name}/config.d

mkdir -p %{buildroot}%{_localstatedir}/log/%{name}
mkdir -p %{buildroot}%{_localstatedir}/spool/%{name}

mkdir -p %{buildroot}%{_sysconfdir}/logrotate.d
install -m 644 -p config/%{name}.logrotate \
	%{buildroot}%{_sysconfdir}/logrotate.d/%{name}

mkdir -p %{buildroot}%{_datadir}/selinux/packages/%{name}
install -m 644 -p config/%{name}.pp \
	%{buildroot}%{_datadir}/selinux/packages/%{name}

%if %{with docs}
	mkdir -p %{buildroot}%{_pkgdocdir}
	cp -pr doxydoc/html %{buildroot}%{_pkgdocdir}

	cp -pr python/docs/build/html %{buildroot}%{_pkgdocdir}/python
	rm %{buildroot}%{_pkgdocdir}/python/.buildinfo
%endif

%ldconfig_scriptlets libs

%ldconfig_scriptlets client-libs

%ldconfig_scriptlets server-libs

%pre server
%sysusers_create_compat %(tar -z -x -f %{SOURCE0} --no-anchored xrootd-sysusers.conf -O > /tmp/xrootd-sysusers.conf && echo /tmp/xrootd-sysusers.conf)

%post server

if [ $1 -eq 1 ] ; then
	systemctl daemon-reload >/dev/null 2>&1 || :
fi

%preun server
if [ $1 -eq 0 ] ; then
	for DAEMON in xrootd cmsd frm_purged frm_xfrd; do
		for INSTANCE in `systemctl | grep $DAEMON@ | awk '{print $1;}'`; do
			systemctl --no-reload disable $INSTANCE > /dev/null 2>&1 || :
			systemctl stop $INSTANCE > /dev/null 2>&1 || :
		done
	done
fi

%postun server
if [ $1 -ge 1 ] ; then
	systemctl daemon-reload >/dev/null 2>&1 || :
	for DAEMON in xrootd cmsd frm_purged frm_xfrd; do
		for INSTANCE in `systemctl | grep $DAEMON@ | awk '{print $1;}'`; do
			systemctl try-restart $INSTANCE >/dev/null 2>&1 || :
		done
	done
fi

%post selinux
semodule -i %{_datadir}/selinux/packages/%{name}/%{name}.pp >/dev/null 2>&1 || :

%postun selinux
if [ $1 -eq 0 ] ; then
	semodule -r %{name} >/dev/null 2>&1 || :
fi

%files
# Empty

%files server
%{_bindir}/cconfig
%{_bindir}/cmsd
%{_bindir}/frm_admin
%{_bindir}/frm_purged
%{_bindir}/frm_xfragent
%{_bindir}/frm_xfrd
%{_bindir}/mpxstats
%{_bindir}/wait41
%{_bindir}/xrdacctest
%{_bindir}/xrdpfc_print
%{_bindir}/xrdpwdadmin
%{_bindir}/xrdsssadmin
%{_bindir}/xrootd
%{_mandir}/man8/cmsd.8*
%{_mandir}/man8/frm_admin.8*
%{_mandir}/man8/frm_purged.8*
%{_mandir}/man8/frm_xfragent.8*
%{_mandir}/man8/frm_xfrd.8*
%{_mandir}/man8/mpxstats.8*
%{_mandir}/man8/xrdpfc_print.8*
%{_mandir}/man8/xrdpwdadmin.8*
%{_mandir}/man8/xrdsssadmin.8*
%{_mandir}/man8/xrootd.8*
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/utils
%{_unitdir}/*
%{_sysusersdir}/%{name}.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}
%dir %{_sysconfdir}/%{name}/config.d
%attr(-,xrootd,xrootd) %config(noreplace) %{_sysconfdir}/%{name}/*.cfg
%attr(-,xrootd,xrootd) %{_localstatedir}/log/%{name}
%attr(-,xrootd,xrootd) %{_localstatedir}/spool/%{name}
%ghost %attr(-,xrootd,xrootd) %{_rundir}/%{name}

%files selinux
%{_datadir}/selinux/packages/%{name}/%{name}.pp

%files libs
%{_libdir}/libXrdAppUtils.so.*
%{_libdir}/libXrdCrypto.so.*
%{_libdir}/libXrdCryptoLite.so.*
%{_libdir}/libXrdUtils.so.*
%{_libdir}/libXrdXml.so.*
# Plugins
%{_libdir}/libXrdCksCalczcrc32-5.so
%{_libdir}/libXrdCryptossl-5.so
%{_libdir}/libXrdSec-5.so
%{_libdir}/libXrdSecProt-5.so
%{_libdir}/libXrdSecgsi-5.so
%{_libdir}/libXrdSecgsiAUTHZVO-5.so
%{_libdir}/libXrdSecgsiGMAPDN-5.so
%{_libdir}/libXrdSeckrb5-5.so
%{_libdir}/libXrdSecpwd-5.so
%{_libdir}/libXrdSecsss-5.so
%{_libdir}/libXrdSecunix-5.so
%{_libdir}/libXrdSecztn-5.so
%license COPYING* LICENSE

%files devel
%{_bindir}/xrootd-config
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/XProtocol
%{_includedir}/%{name}/Xrd
%{_includedir}/%{name}/XrdCks
%{_includedir}/%{name}/XrdNet
%{_includedir}/%{name}/XrdOuc
%{_includedir}/%{name}/XrdSec
%{_includedir}/%{name}/XrdSys
%{_includedir}/%{name}/XrdXml
%{_includedir}/%{name}/XrdVersion.hh
%{_libdir}/libXrdAppUtils.so
%{_libdir}/libXrdCrypto.so
%{_libdir}/libXrdCryptoLite.so
%{_libdir}/libXrdUtils.so
%{_libdir}/libXrdXml.so
%{_libdir}/cmake/XRootD
%dir %{_datadir}/%{name}

%files client-libs
%{_libdir}/libXrdCl.so.*
%if %{with xrdec}
%{_libdir}/libXrdEc.so.*
%endif
%{_libdir}/libXrdFfs.so.*
%{_libdir}/libXrdPosix.so.*
%{_libdir}/libXrdPosixPreload.so.*
# This lib may be used for LD_PRELOAD so the .so link needs to be included
%{_libdir}/libXrdPosixPreload.so
%{_libdir}/libXrdSsiLib.so.*
%{_libdir}/libXrdSsiShMap.so.*
# Plugins
%{_libdir}/libXrdClCurl-5.so
%{_libdir}/libXrdClS3-5.so
%{_libdir}/libXrdClProxyPlugin-5.so
%{_libdir}/libXrdClRecorder-5.so
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/client.conf
%dir %{_sysconfdir}/%{name}/client.plugins.d
%config(noreplace) %{_sysconfdir}/%{name}/client.plugins.d/client-plugin.conf.example
%config(noreplace) %{_sysconfdir}/%{name}/client.plugins.d/recorder.conf

%files client-devel
%{_includedir}/%{name}/XrdCl
%{_includedir}/%{name}/XrdClCurl
%{_includedir}/%{name}/XrdPosix
%{_libdir}/libXrdCl.so
%if %{with xrdec}
%{_libdir}/libXrdEc.so
%endif
%{_libdir}/libXrdFfs.so
%{_libdir}/libXrdPosix.so

%files server-libs
%{_libdir}/libXrdHttpUtils.so.*
%{_libdir}/libXrdServer.so.*
# Plugins
%{_libdir}/libXrdBlacklistDecision-5.so
%{_libdir}/libXrdBwm-5.so
%{_libdir}/libXrdCmsRedirectLocal-5.so
%{_libdir}/libXrdFileCache-5.so
%{_libdir}/libXrdHttp-5.so
%{_libdir}/libXrdHttpTPC-5.so
%{_libdir}/libXrdHttpCors-5.so
%{_libdir}/libXrdMacaroons-5.so
%{_libdir}/libXrdN2No2p-5.so
%{_libdir}/libXrdOfsPrepGPI-5.so
%{_libdir}/libXrdOssArc-5.so
%{_libdir}/libXrdOssCsi-5.so
%{_libdir}/libXrdOssSIgpfsT-5.so
%{_libdir}/libXrdOssStats-5.so
%{_libdir}/libXrdPfc-5.so
%{_libdir}/libXrdPfcPurgeQuota-5.so
%{_libdir}/libXrdPss-5.so
%{_libdir}/libXrdSsi-5.so
%{_libdir}/libXrdSsiLog-5.so
%{_libdir}/libXrdThrottle-5.so
%{_libdir}/libXrdXrootd-5.so

%files server-devel
%{_includedir}/%{name}/XrdAcc
%{_includedir}/%{name}/XrdCms
%{_includedir}/%{name}/XrdHttp
%{_includedir}/%{name}/XrdOfs
%{_includedir}/%{name}/XrdOss
%{_includedir}/%{name}/XrdPfc
%{_includedir}/%{name}/XrdSfs
%{_includedir}/%{name}/XrdXrootd
%{_libdir}/libXrdHttpUtils.so
%{_libdir}/libXrdServer.so

%files private-devel
%{_includedir}/%{name}/private
%{_libdir}/libXrdSsiLib.so
%{_libdir}/libXrdSsiShMap.so

%files client
%{_bindir}/xrdadler32
%{_bindir}/xrdcks
%{_bindir}/xrdcopy
%{_bindir}/xrdcp
%{_bindir}/xrdcrc32c
%{_bindir}/xrdfs
%{_bindir}/xrdgsiproxy
%{_bindir}/xrdgsitest
%{_bindir}/xrdmapc
%{_bindir}/xrdpinls
%{_bindir}/xrdreplay
%{_mandir}/man1/xrdadler32.1*
%{_mandir}/man1/xrdcopy.1*
%{_mandir}/man1/xrdcp.1*
%{_mandir}/man1/xrdfs.1*
%{_mandir}/man1/xrdgsiproxy.1*
%{_mandir}/man1/xrdgsitest.1*
%{_mandir}/man1/xrdmapc.1*

%files fuse
%{_bindir}/xrootdfs
%{_mandir}/man1/xrootdfs.1*

%files voms
%{_libdir}/libXrdVoms-5.so
%{_libdir}/libXrdHttpVOMS-5.so
%{_libdir}/libXrdSecgsiVOMS-5.so
%doc %{_mandir}/man1/libXrdVoms.1*
%doc %{_mandir}/man1/libXrdSecgsiVOMS.1*

%files scitokens
%{_libdir}/libXrdAccSciTokens-5.so
%doc src/XrdSciTokens/README.md

%files -n xrdcl-http
%{_libdir}/libXrdClHttp-5.so
%config(noreplace) %{_sysconfdir}/%{name}/client.plugins.d/xrdcl-http-plugin.conf

%if %{with ceph}
%files ceph
%{_libdir}/libXrdCeph-5.so
%{_libdir}/libXrdCephXattr-5.so
%{_libdir}/libXrdCephPosix.so.*
%endif

%files -n python%{python3_pkgversion}-%{name}
%{python3_sitearch}/xrootd-*.*-info
%{python3_sitearch}/pyxrootd
%{python3_sitearch}/XRootD

%if %{with docs}
%files doc
%doc %{_pkgdocdir}
%endif

%changelog

* Mon Nov 17 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.9.1-1
- XRootD 5.9.1

* Sat Oct 04 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.9.0-1
- XRootD 5.9.0

* Thu Jul 10 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.8.4-1
- XRootD 5.8.4

* Thu Jun 05 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.8.3-1
- XRootD 5.8.3

* Thu May 08 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.8.2-1
- XRootD 5.8.2

* Mon Apr 14 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.8.1-1
- XRootD 5.8.1

* Fri Mar 21 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.8.0-1
- XRootD 5.8.0

* Tue Jan 28 2025 Guilherme Amadio <amadio@cern.ch> - 1:5.7.3-1
- XRootD 5.7.3

* Wed Nov 27 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.7.2-1
- XRootD 5.7.2

* Mon Sep 02 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.7.1-1
- XRootD 5.7.1

* Thu Jun 27 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.7.0-1
- XRootD 5.7.0

* Fri Mar 08 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.6.9-1
- XRootD 5.6.9

* Fri Feb 23 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.6.8-1
- XRootD 5.6.8

* Tue Feb 06 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.6.7-1
- XRootD 5.6.7

* Thu Jan 25 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.6.6-1
- XRootD 5.6.6

* Mon Jan 22 2024 Guilherme Amadio <amadio@cern.ch> - 1:5.6.5-1
- XRootD 5.6.5

* Fri Dec 08 2023 Guilherme Amadio <amadio@cern.ch> - 1:5.6.4-1
- Use isa-l library from the system
- Extract version from tarball when building git snapshots
- XRootD 5.6.4

* Fri Oct 27 2023 Guilherme Amadio <amadio@cern.ch> - 1:5.6.3-2
- XRootD 5.6.3

* Mon Sep 18 2023 Guilherme Amadio <amadio@cern.ch> - 1:5.6.2-2
- Add patch with fix for id parsing in XrdAccAuthFile (#2088)

* Fri Sep 15 2023 Guilherme Amadio <amadio@cern.ch> - 1:5.6.2-1
- Link XRootD 4 with openssl1.1 when using --with openssl11
- XRootD 5.6.2

* Fri Aug 11 2023 Guilherme Amadio <amadio@cern.ch> - 1:5.6.1-1
- Modernize spec file to add more optional features and select
  default build options automatically for each supported OS.
- Use latest official release tarball by default.
- Enable snapshot builds from git.

* Thu Oct 15 2020 Michal Simon <michal.simon@cern.ch> - 5.0.2-1
- Introduce xrootd-scitokens package

* Wed May 27 2020 Michal Simon <michal.simon@cern.ch> - 4.12.2-1
- Remove xrootd-voms-devel

* Fri Apr 17 2020 Michal Simon <michal.simon@cern.ch> - 4.12.0-1
- Introduce xrootd-voms and xrootd-voms-devel packages

* Mon Sep 02 2019 Michal Simon <michal.simon@cern.ch> - 4.10.1-1
- Move xrdmapc to client package

* Fri Aug 30 2019 Michal Simon <michal.simon@cern.ch> - 5.0.0
- Remove XRootD 3.x.x compat package

* Wed Apr 17 2019 Michal Simon <michal.simon@cern.ch> - 4.10.0-1
- Create add xrdcl-http package

* Tue Jan 08 2019 Edgar Fajardo <emfajard@ucsd.edu>
- Create config dir /etc/xrootd/config.d

* Tue May 08 2018 Michal Simon <michal.simon@cern.ch>
- Make python3 sub-package optional

* Fri Nov 10 2017 Michal Simon <michal.simon@cern.ch> - 1:4.8.0-1
- Add python3 sub-package
- Rename python sub-package

* Tue Dec 13 2016 Gerardo Ganis <gerardo.ganis@cern.ch>
- Add xrdgsitest to xrootd-client-devel

* Mon Mar 16 2015 Lukasz Janyst <ljanyst@cern.ch>
- create the python package

* Wed Mar 11 2015 Lukasz Janyst <ljanyst@cern.ch>
- create the xrootd-ceph package

* Thu Oct 30 2014 Lukasz Janyst <ljanyst@cern.ch>
- update for 4.1 and introduce 3.3.6 compat packages

* Thu Aug 28 2014 Lukasz Janyst <ljanyst@cern.ch>
- add support for systemd

* Wed Aug 27 2014 Lukasz Janyst <ljanyst@cern.ch>
- use generic selinux policy build mechanisms

* Tue Apr 01 2014 Lukasz Janyst <ljanyst@cern.ch>
- correct the license field (LGPLv3+)
- rename to xrootd4
- add 'conflicts' statements
- remove 'provides' and 'obsoletes'

* Mon Mar 31 2014 Lukasz Janyst <ljanyst@cern.ch>
- Add selinux policy

* Fri Jan 24 2014 Lukasz Janyst <ljanyst@cern.ch>
- Import XrdHttp

* Fri Jun 7 2013 Lukasz Janyst <ljanyst@cern.ch>
- adopt the EPEL RPM layout by Mattias Ellert

* Tue Apr 2 2013 Lukasz Janyst <ljanyst@cern.ch>
- remove perl

* Thu Nov 1 2012 Justin Salmon <jsalmon@cern.ch>
- add tests package

* Fri Oct 21 2011 Lukasz Janyst <ljanyst@cern.ch> 3.1.0-1
- bump the version to 3.1.0

* Mon Apr 11 2011 Lukasz Janyst <ljanyst@cern.ch> 3.0.3-1
- the first RPM release - version 3.0.3
- the detailed release notes are available at:
  https://xrootd.org/download/ReleaseNotes.html
