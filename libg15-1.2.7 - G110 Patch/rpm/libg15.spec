# libg15 rpm spec file
#
%define prefix	/usr
Summary: library to control logitech G15 keyboards
Name: libg15
Version: 1.2.7
Release: 1
License: GPL
Group: Applications/System
Source: http://prdownloads.sourceforge.net/g15tools/libg15-1.2.7.tar.bz2
URL: http://sourceforge.net/projects/g15tools
Distribution: Linux
Vendor: NONE
Packager: Mike Lampard <mlampard@users.sourceforge.net>
Buildroot: /var/tmp/libg15-%{PACKAGE_VERSION}-root
Requires: libusb
BuildRequires: libusb-devel
Provides: libg15

%description
libg15 controls the G15 keyboard, providing applications access
to the keyboard's LCD display, and the additional keys available
on this keyboard.

%package devel
Summary: libg15 controls the G15 keyboard and LCD.
Group: System Environment/Libraries
Requires: libusb
Requires: libg15
Provides: libg15-devel

%description devel
libg15 controls the G15 keyboard, providing applications access
to the keyboard's LCD display, and the additional keys available
on this keyboard.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files 
%defattr(-, root, root)

%doc AUTHORS COPYING NEWS README
%{prefix}/lib/lib*.la
%{prefix}/lib/libg15.so*

%files devel
%defattr(-, root, root)

%doc AUTHORS COPYING NEWS README
%{prefix}/lib/lib*.a
%{prefix}/include/*
