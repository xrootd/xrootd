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
BuildArch:      noarch
Requires:       python >= 2.4
BuildRequires:  xrootd-cl-devel

%description
pyxrootd is a set of python language bindings for xrootd.

%prep
%setup

%install
python setup.py install
