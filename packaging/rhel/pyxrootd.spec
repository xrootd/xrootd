%if 0%{?rhel} && 0%{?rhel} <= 5
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(1))")}
%endif

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
Requires:       xrootd-cl >= 3.3.2
BuildRequires:  xrootd-cl-devel python-devel

%description
pyxrootd is a set of python language bindings for xrootd.

%prep
%setup -n %{name}-%{version}

%build
env CFLAGS="$RPM_OPT_FLAGS" %{__python} setup.py build

%install
%{__python} setup.py install --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
[ "x%{buildroot}" != "x/" ] && rm -rf %{buildroot}

%files -f INSTALLED_FILES
%defattr(-,root,root)

%changelog
* Fri Apr 26 2013 Justin Salmon <jsalmon@cern.ch>
- Add requirement for xrootd-cl 3.3.2
* Fri Apr 26 2013 Justin Salmon <jsalmon@cern.ch>
- Install to correct place in RHEL5
* Wed Apr 03 2013 Justin Salmon <jsalmon@cern.ch>
- Initial version