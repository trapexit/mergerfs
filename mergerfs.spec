Name:		mergerfs
Version:	__VERSION__
Release:	1%{?dist}
Summary:	A FUSE union filesystem

Group:		Applications/System
License:	ISC
URL:		https://github.com/trapexit/mergerfs

Source:		mergerfs-%{version}.tar.gz

BuildRequires:	gcc-c++
BuildRequires:	libattr-devel
# rpmbuild driven by the Makefile uses git to generate a version number
BuildRequires:	git

Requires:	fuse

%prep
%setup -q

%description
mergerfs is similar to mhddfs, unionfs, and aufs. Like mhddfs in that it too
uses FUSE. Like aufs in that it provides multiple policies for how to handle
behavior.

%build
make %{?_smp_mflags}

%install
make install PREFIX=%{_prefix} DESTDIR=%{buildroot}

%files
%{_bindir}/*
%doc %{_mandir}/*

%changelog
* Mon Jan 25 2016 Antonio SJ Musumeci <trapexit@spawn.link>
- Remove sbin files

* Sat Sep 05 2015 Antonio SJ Musumeci <trapexit@spawn.link>
- Include PREFIX to install

* Mon Dec 29 2014 Joe Lawrence <joe.lawrence@stratus.com>
- Tweak rpmbuild to archive current git HEAD into a tarball, then (re)build in
  the rpmbuild directory -- more complicated but seemingly better suited to
  generate source and debug rpms.

* Fri Jun 20 2014 Joe Lawrence <joe.lawrence@stratus.com>
- Initial rpm spec file.
