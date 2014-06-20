Name:		mergerfs
Version:	%{pkg_version}
Release:	1%{?dist}
Summary:	A FUSE union filesystem

Group:		Applications/System
License:	MIT
URL:		https://github.com/trapexit/mergerfs

BuildRequires:	gcc-c++
BuildRequires:	libattr-devel
BuildRequires:	fuse-devel
# rpmbuild driven by the Makefile uses git to generate a version number
BuildRequires:	git
# pandoc pulls in ~60 packages (87M installed) from EPEL :(
BuildRequires:	pandoc

Requires:	fuse-libs

%description
mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too
uses FUSE. Like aufs in that it provides multiple policies for how to handle
behavior.

%build
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%{_bindir}/*
%doc %{_mandir}/*

%changelog
* Fri Jun 20 2014 Joe Lawrence <joe.lawrence@stratus.com>
- Initial rpm spec file.
