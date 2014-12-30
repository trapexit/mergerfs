Name:		mergerfs
Version:	__VERSION__
Release:	1%{?dist}
Summary:	A FUSE union filesystem

Group:		Applications/System
License:	MIT
URL:		https://github.com/trapexit/mergerfs
Source:		mergerfs-%{version}.tar.gz

BuildRequires:	gcc-c++
BuildRequires:	libattr-devel
BuildRequires:	fuse-devel
# rpmbuild driven by the Makefile uses git to generate a version number
BuildRequires:	git
# pandoc pulls in ~60 packages (87M installed) from EPEL :(
BuildRequires:	pandoc

Requires:	fuse-libs

%prep
%setup -q

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
* Mon Dec 29 2014 Joe Lawrence <joe.lawrence@stratus.com>
- Tweak rpmbuild to archive current git HEAD into a tarball, then (re)build in
  the rpmbuild directory -- more complicated but seemingly better suited to
  generate source and debug rpms.

* Fri Jun 20 2014 Joe Lawrence <joe.lawrence@stratus.com>
- Initial rpm spec file.
