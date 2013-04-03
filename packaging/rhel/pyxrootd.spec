%global _binaries_in_noarch_packages_terminate_build 0

%global __os_install_post    \
    /usr/lib/rpm/redhat/brp-compress \
    %{!?__debug_package:/usr/lib/rpm/redhat/brp-strip %{__strip}} \
    /usr/lib/rpm/redhat/brp-strip-static-archive %{__strip} \
    /usr/lib/rpm/redhat/brp-strip-comment-note %{__strip} %{__objdump} \
    /usr/lib/rpm/redhat/brp-java-repack-jars \
%{nil}

Name:           xrootd-python
Version:        0.1
Release:        1%{?dist}
License:        GPL3
Summary:        Python bindings for XRootD
Group:          Development/Tools
Packager:       Justin Salmon <jsalmon@cern.ch>
URL:            http://github.com/xrootd/python-xrootd
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:       python >= 2.4
BuildRequires:  xrootd-cl-devel python-devel

%description
pyxrootd is a set of python language bindings for xrootd.

%prep
%setup -n %{name}-%{version}

%build
env CFLAGS="$RPM_OPT_FLAGS" python setup.py build

%install
python setup.py install --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
[ "x%{buildroot}" != "x/" ] && rm -rf %{buildroot}

%files -f INSTALLED_FILES
%defattr(-,root,root)

%changelog
* Wed Apr 03 2013 Justin Salmon <jsalmon@cern.ch>
- Initial version