%define build_number 1
%define debug_package %{nil}

Name:           monitor
Version:        1.2.0
Release:        1%{?dist}
Summary:        The monitor-hotel-version-%{version}

Group:          Applications/Internet
License:        Kingstone Lisence
URL:            http://www.chuangyuan.net
Source0:        monitor-%{version}.tar.bz2

BuildRequires:  libpcap-devel libnet-devel libstdc++-devel glibc-devel mysql-devel sqlite-devel

Requires:       libpcap libnet libstdc++ mysql-libs glibc sqlite
Conflicts:		monitor-cafe
Vendor:			Kingstone Soft, Inc.  

%description
				monitor is a product of Kingstone Software,Inc. 
				It monitors the network and upload critical logs to the 
				public security department. 

%package hotel
Summary:        The hotel-version of monitor
Group:          Applications/Internet
BuildRoot:      %{_tmppath}/monitor-hotel-%{version}-%{release}-root-%(%{__id_u} -n)

%description hotel
				monitor is a product of Kingstone Software,Inc. 
				It monitors the network and upload critical logs to the 
				public security department. 
				This package is for hotel use
				

%prep 
rm -rf monitor-%{version}
tar -xvf ../../workspace/monitor/release/monitor-%{version}.tar.bz2

%build 
cd monitor-%{version}
%configure --enable-hotel --with-sqlite3
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd monitor-%{version}
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files hotel
%defattr(-,root,root,-)
/etc/init.d/monitord
%{_prefix}/bin/runmonitor
%{_prefix}/bin/monitor
%{_prefix}/libexec/monitor/
%{_docdir}/monitor/README
