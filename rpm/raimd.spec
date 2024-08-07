Name:		raimd
Version:	999.999
Vendor:	        Rai Technology, Inc
Release:	99999%{?dist}
Summary:	Rai market data codecs

License:	ASL 2.0
URL:		https://github.com/raitechnology/%{name}
Source0:	%{name}-%{version}-99999.tar.gz
BuildRoot:	${_tmppath}
Prefix:	        /usr
BuildRequires:  gcc-c++
BuildRequires:  chrpath
BuildRequires:  libdecnumber
BuildRequires:  git-core
BuildRequires:  zlib-devel
Requires:       libdecnumber
Requires:       zlib
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Market data message codecs for Tib and RMDS formats.

%prep
%setup -q


%define _unpackaged_files_terminate_build 0
%define _missing_doc_files_terminate_build 0
%define _missing_build_ids_terminate_build 0
%define _include_gdb_index 1

%build
make build_dir=./usr %{?_smp_mflags} dist_bins
mkdir -p ./usr/share/doc/%{name}
cp -a ./include ./usr/include
cp -a README.md doc/msg_format.md ./usr/share/doc/%{name}/

%install
rm -rf %{buildroot}
mkdir -p  %{buildroot}

# in builddir
cp -a * %{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/usr/bin/*
/usr/lib64/*
/usr/include/*
/usr/share/doc/*

%post
echo "${RPM_INSTALL_PREFIX}/lib64" > /etc/ld.so.conf.d/%{name}.conf
/sbin/ldconfig

%postun
if [ $1 -eq 0 ] ; then
rm -f /etc/ld.so.conf.d/%{name}.conf
fi
/sbin/ldconfig

%changelog
* Sat Jan 01 2000 <support@raitechnology.com>
- Hello world
