Summary:    Cocaine - Native Framework
Name:       cocaine-framework-native
Version:    0.11.1.2
Release:    2%{?dist}

License:    GPLv2+
Group:      System Environment/Libraries
URL:        http://reverbrain.com
Source0:    http://repo.reverbrain.com/sources/cocaine-framework-native/%{name}-%{version}.tar.bz2
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  boost-devel
BuildRequires:  libev-devel
BuildRequires:  msgpack-devel
BuildRequires:  libcocaine-core2-devel
%if %{defined rhel} && 0%{?rhel} < 7
BuildRequires:  cmake28
%else
BuildRequires:  cmake
%endif

%description
A simple framework to ease the development of native Cocaine apps


%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: libcocaine-core2-devel


%description devel
This package contains libraries, header files and developer documentation
needed for developing native Cocaine apps.

%prep
%setup -q

%build
%if %{defined rhel} && 0%{?rhel} < 7
%{cmake28} -DCOCAINE_LIBDIR=%{_libdir} .
%else
%{cmake} -DCOCAINE_LIBDIR=%{_libdir} .
%endif

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc README.md
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/cocaine/framework/*
%{_libdir}/*.so


%changelog
* Thu Jul 25 2013 Arkady L. Shane <ashejn@russianfedora.ru> - 0.10.5.5-1
- initial build
