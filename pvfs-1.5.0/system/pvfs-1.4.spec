#
# Spec file for PVFS version 1.4
#
Summary: A parallel file system for Beowulf workstations and Piles of PCs
Name: pvfs
Distribution: PVFS
Version: 1.4.2
Release: 1
Copyright: GPL
Group: Utilities/File
Source: ftp://ftp.parl.eng.clemson.edu/pub/pvfs/pvfs-1.4.2.tgz
URL: http://ece.clemson.edu/parl/pvfs/index.html
Vendor: Parallel Architecture Research Lab, Clemson University
Packager: Rob Ross <rbross@parl.clemson.edu>
Prefix: /usr
BuildRoot: /var/tmp/pvfs-%{PACKAGE_VERSION}-root

%description
A parallel file system for Beowulf workstations and Piles of PCs.  This
RPM includes the I/O and manager daemons, the application library for
accessing the file system, and some utilities for administration and
movement of files onto the file system.

%changelog

* Sun Aug 22 1999 Rob Ross <rbross@parl.clemson.edu>

- First version of spec file for v1.4, for glibc 2.0

%prep
%setup

%build
./configure --prefix=/usr/pvfs
make
cd ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/lib

%install
cd ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}
make install_root=${RPM_BUILD_ROOT} install

cd ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/docs
make MANDIR=${RPM_BUILD_ROOT}/usr/man
cp ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/docs/user_guide.* \
	${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/
install -d -m755 ${RPM_BUILD_ROOT}/etc/rc.d/init.d
install -d -m755 ${RPM_BUILD_ROOT}/usr/lib
install -d -m755 ${RPM_BUILD_ROOT}/usr/include/pvfs
install -m755 ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/system/iod.rc \
	${RPM_BUILD_ROOT}/etc/rc.d/init.d/iod
install -m755 ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/system/mgr.rc \
	${RPM_BUILD_ROOT}/etc/rc.d/init.d/mgr
install -m755 ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/system/iod.conf \
	${RPM_BUILD_ROOT}/etc/iod.conf
install -m755 ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/system/pvfstab \
	${RPM_BUILD_ROOT}/etc/pvfstab

chmod 755 ${RPM_BUILD_ROOT}/usr/pvfs/lib/libpvfs.so.1.4
install -m755 ${RPM_BUILD_DIR}/pvfs-${RPM_PACKAGE_VERSION}/lib/libminipvfs.a \
	${RPM_BUILD_ROOT}/usr/lib
install -m755 ${RPM_BUILD_ROOT}/usr/pvfs/lib/libpvfs.a \
	${RPM_BUILD_ROOT}/usr/lib/
install -m755 ${RPM_BUILD_ROOT}/usr/pvfs/include/* \
	${RPM_BUILD_ROOT}/usr/include/pvfs/

%files
%defattr(755, root, root)
# config files and rc files
%config /etc/rc.d/init.d/iod
%config /etc/rc.d/init.d/mgr
%config /etc/iod.conf
%config /etc/pvfstab

# examples
/usr/pvfs/bin/twrite
/usr/pvfs/bin/tread

#daemons
/usr/pvfs/bin/iod
/usr/pvfs/bin/mgr

#utilities
/usr/pvfs/bin/pvstat
/usr/pvfs/bin/mkdot
/usr/pvfs/bin/u2p
/usr/pvfs/bin/mkiodtab
/usr/pvfs/bin/enableiod
/usr/pvfs/bin/enablemgr

#libraries
%attr(644, -, -) /usr/lib/libpvfs.a
%attr(644, -, -) /usr/lib/libminipvfs.a
/usr/pvfs/lib/libpvfs.so.1.4

#include files
%attr(644, -, -) /usr/include/pvfs/debug.h
%attr(644, -, -) /usr/include/pvfs/desc.h
%attr(644, -, -) /usr/include/pvfs/meta.h
%attr(644, -, -) /usr/include/pvfs/minmax.h
%attr(644, -, -) /usr/include/pvfs/pvfs.h
%attr(644, -, -) /usr/include/pvfs/req.h
%attr(644, -, -) /usr/include/pvfs/pvfs_config.h
%attr(644, -, -) /usr/include/pvfs/pvfs_proto.h

# man pages - RPM knows these are docs automatically
%attr(644, -, -) /usr/man/man3/pvfs_chmod.3
%attr(644, -, -) /usr/man/man3/pvfs_chown.3
%attr(644, -, -) /usr/man/man3/pvfs_close.3
%attr(644, -, -) /usr/man/man3/pvfs_dup.3
%attr(644, -, -) /usr/man/man3/pvfs_fcntl.3
%attr(644, -, -) /usr/man/man3/pvfs_ioctl.3
%attr(644, -, -) /usr/man/man3/pvfs_lseek.3
%attr(644, -, -) /usr/man/man3/pvfs_mkdir.3
%attr(644, -, -) /usr/man/man3/pvfs_open.3
%attr(644, -, -) /usr/man/man3/pvfs_read.3
%attr(644, -, -) /usr/man/man3/pvfs_rename.3
%attr(644, -, -) /usr/man/man3/pvfs_rmdir.3
%attr(644, -, -) /usr/man/man3/pvfs_stat.3
%attr(644, -, -) /usr/man/man3/pvfs_unlink.3
%attr(644, -, -) /usr/man/man3/pvfs_write.3
%attr(644, -, -) /usr/man/man5/iod.conf.5
%attr(644, -, -) /usr/man/man5/pvfstab.5
%attr(644, -, -) /usr/man/man8/iod.8
%attr(644, -, -) /usr/man/man8/mgr.8

# other docs
%attr(644, -, -) %doc docs/user_guide.html
%attr(644, -, -) %doc docs/figs/blocking-color.jpg
%attr(644, -, -) %doc docs/figs/partition-example1.jpg
%attr(644, -, -) %doc docs/figs/stripe1-color-big.jpg
%attr(644, -, -) %doc docs/figs/partition-defined.jpg
%attr(644, -, -) %doc docs/figs/partition-example2.jpg  

%post

# man page links
ln -s /usr/man/man3/pvfs_chmod.3 /usr/man/man3/pvfs_fchmod.3
ln -s /usr/man/man3/pvfs_chown.3 /usr/man/man3/pvfs_fchown.3
ln -s /usr/man/man3/pvfs_dup.3 /usr/man/man3/pvfs_dup2.3
ln -s /usr/man/man3/pvfs_stat.3 /usr/man/man3/pvfs_fstat.3
ln -s /usr/man/man3/pvfs_stat.3 /usr/man/man3/pvfs_lstat.3

# rc file links for iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc0.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc1.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc2.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc3.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc4.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc5.d/K35iod
ln -s /etc/rc.d/init.d/iod /etc/rc.d/rc6.d/K35iod

# rc file links for mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc0.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc1.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc2.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc3.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc4.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc5.d/K35mgr
ln -s /etc/rc.d/init.d/mgr /etc/rc.d/rc6.d/K35mgr

%postun
rm -f /usr/man/man3/pvfs_fchmod.3
rm -f /usr/man/man3/pvfs_fchown.3
rm -f /usr/man/man3/pvfs_dup2.3
rm -f /usr/man/man3/pvfs_fstat.3
rm -f /usr/man/man3/pvfs_lstat.3

rm -f /etc/rc.d/rc[0-6].d/[SK][0-9][0-9]mgr
rm -f /etc/rc.d/rc[0-6].d/[SK][0-9][0-9]iod

%clean
rm -rf $RPM_BUILD_DIR/pvfs-${RPM_PACKAGE_VERSION}
