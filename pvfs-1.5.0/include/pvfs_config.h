#ifndef PVFS_CONFIG_H
#define PVFS_CONFIG_H

/*
 * pvfs_config.h copyright (c) 1998 Clemson University, all rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * Contact:  Rob Ross  	rbross@parl.clemson.edu
 *           Walt Ligon walt@parl.clemson.edu
 * 
 */

/* PVFS_CONFIG.H - SYSTEM CONFIGURATION PARAMETERS */

/* __ALPHA__ handles Alpha processor issues (or tries to anyway).
 *
 * NOTE: THIS IS DEFINED IN config.h NOW AUTOMATICALLY FROM configure
 */

/* LARGE_FILE_SUPPORT turns on the (currently buggy) 64bit file support
 * within PVFS.
 */
/* #undef LARGE_FILE_SUPPORT */

/* __USE_SENDFILE__ turns on the use of sendfile() in the I/O daemon.
 * We haven't seen any speedup from this yet, so it's also off by default.
 */
/* #undef __USE_SENDFILE__ */

/* __RANDOM_NEXTSOCK__ turns on randomization in the socket selection
 * routines of nextsock(), defined in shared/sockset.c.  This hasn't
 * seemed to make a difference in any situations, so it's off by
 * default.
 */
/* #undef __RANDOM_NEXTSOCK__ */

/* __RANDOM_BASE__ turns on random selection of base nodes on the
 * manager.  This is VERY useful for groups that are using PVFS to store
 * large numbers of small files.  In the default case this sort of
 * situation will tend to result in I/O node 0 getting a lot of data
 * while the other nodes are mostly empty.
 *
 * On the other hand, it makes it more trouble to know where your data
 * is...
 */
/* #undef __RANDOM_BASE__ */

/* __ALWAYS_CONN__ turns on the O_CONN behavior all the time; sets up
 * connections to all I/O daemons when pvfs_open() is called.  This
 * makes the first read/write calls not include connection time, which
 * makes performance testing a little more straightforward.
 *
 * However, this feature has been found to expose a bug in the Intel
 * Etherexpress Pro fast ethernet card, causing it to hang and spit out
 * nasty messages.  So we've turned it off by default.
 */
#define __ALWAYS_CONN__

/* __FS_H_IS_OK__  controls whether or not /linux/fs.h is included in the PVFS
 * manager and io daemon code.  One thing fs.h does is define NR_OPEN
 * to the kernel's default value, rather than pvfs's relatively small
 * default value of 256.  This controls how many files may be open
 * simultaneously by the file system.
 */
#define __FS_H_IS_OK__

/* PVFSTAB FILE LOCATION */
#define PVFSTAB_PATH "/etc/pvfstab"
/* PVFSTAB ENVIRONMENT VARIABLE FOR OVERRIDING IT */
#define PVFSTAB_ENV "PVFSTAB_FILE"

/* OUR FS MAGIC NUMBER...returned by statfs() */
#define PVFS_SUPER_MAGIC 0x0872

/* CLIENT_SOCKET_BUFFER_SIZE - clients will try to set send and receive
 * buffer sizes to this value
 */
#define CLIENT_SOCKET_BUFFER_SIZE (65535)

/* MANAGER STUFF */
#define MGR_REQ_PORT 3000
#define MAXIODS 512
#define MGR_BACKLOG 64

/* default stripe size */
#define DEFAULT_SSIZE 65536

/* not currently used... */
#define DEFAULT_BSIZE 0

/* inactivity timeout, ms */
#define SOCK_TIME_OUT 300000

/* TCP_CORK - should be easier to get; in /usr/include/linux/socket.h */
#ifndef TCP_CORK
#define TCP_CORK 3
#endif

/* IOD STUFF */
#define IOD_REQ_PORT 7000
#define IOD_BASE_PORT IOD_REQ_PORT
#define IOD_BACKLOG 64

/* this determines how many directories the iod splits files into;
 * 101, 199, 499, 997 are all decent choices.
 */
#define IOD_DIR_HASH_SZ 101

#define IOD_USER "nobody"
#define IOD_GROUP "nobody"
#define IOD_ROOTDIR "/"
#define IOD_DATADIR "/pvfs_data"
#define IOD_LOGDIR "/tmp"

/* IOD_ACCESS_PAGE_SIZE - memory map region size (must be power of 2)
 *   - previously this value was 128K (for smaller machines)
 * IOD_WRITE_BUFFER_SIZE - size of buffer used for writing data
 *   - previously this value was 64K (for smaller machines)
 * IOD_SOCKET_BUFFER_SIZE - size of IOD socket buffers, recv and xmit
 *   - this value used to be 65535 by default, but admins can change it.
 */

#define IOD_ACCESS_PAGE_SIZE (512*1024)
#define IOD_WRITE_BUFFER_SIZE (512*1024)
#define IOD_SOCKET_BUFFER_SIZE (65535)

/* IOCTL DEFINES - COULDN'T FIND A BETTER PLACE TO PUT THEM... */
/* These are just arbitrary #s that linux doesn't seem to use. */
#define GETPART 0x5601
#define SETPART 0x5602
#define GETMETA 0x5603

/* more arbitrary #s; these are flags for pvfs_open() */
/* using lowest 4 bits of highest byte in the integer */
#define PVFSMASK 03700000000
#define O_META   00100000000
#define O_PART   00200000000
#define O_GSTART 00400000000
#define O_GADD   01000000000
#define O_CONN   02000000000


#endif
