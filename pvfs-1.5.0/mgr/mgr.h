/*
 * mgr.h copyright (c) 1999 Clemson University, all rights reserved.
 *
 * Written by Rob Ross.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Contact:  Rob Ross    rbross@parl.clemson.edu
 */

#ifndef MGR_H
#define MGR_H

/* INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <desc.h> /* has iod_info structure */
#include <meta.h> /* has pvfs_filestat structure */
#include <sockio.h>
#include <sockset.h>
#include <fslist.h>
#include <flist.h>
#include <iodtab.h>
#include <req.h>
#include <minmax.h>
#include <pvfs_config.h>

/* PROTOTYPES */
int do_access(int, mreq_p, void *, mack_p);
int do_chmod(int, mreq_p, void *, mack_p);
int do_chown(int, mreq_p, void *, mack_p);
int do_close(int, mreq_p, void *, mack_p);
int do_stat(int, mreq_p, void *, mack_p);
int do_mount(int, mreq_p, void *, mack_p);
int do_open(int, mreq_p, void *, mack_p);
int do_umount(int, mreq_p, void *, mack_p);
int do_unlink(int, mreq_p, void *, mack_p);
int do_umount(int, mreq_p, void *, mack_p);
int do_fstat(int, mreq_p, void *, mack_p);
int do_rename(int, mreq_p, void *, mack_p);
int do_shutdown(int, mreq_p, void *, mack_p);
int do_iod_info(int, mreq_p, void *, mack_p);
int do_mkdir(int, mreq_p, void *, mack_p);
int do_rmdir(int, mreq_p, void *, mack_p);
int do_fchown(int, mreq_p, void *, mack_p);
int do_fchmod(int, mreq_p, void *, mack_p);
int do_truncate(int, mreq_p, void *, mack_p);
int do_utime(int, mreq_p, void *, mack_p);
int do_statfs(int, mreq_p, void *, mack_p);
int do_getdents(int, mreq_p, void *, mack_p);
int send_error(int, int, int, mack_p);
fsinfo_p quick_mount(char *, int, int, mack_p);
int stop_iods(void *);
int stop_idle_iods(void *);
void *restart(int sig_nr);
void *mgr_shutdown(int sig_nr);
static void *do_signal(int sig_nr);

/* METADATA FN PROTOTYPES */
int md_chmod(mreq_p request);
int md_chown(mreq_p request);
int md_close(mreq_p req, char *fname);
int md_mount(mreq_p request);
int md_open(char *name, mreq_p request, fmeta_p metar_p);
int md_rmdir(mreq_p req_p);
int md_stat(char *name, mreq_p request, fmeta_p metar_p);
int md_unlink(mreq_p request, fmeta_p metar_p);
int check_pvfs(const char *pathname);
int get_root_dir(char *fname, char *rootbuf);
int put_dmeta(char *fname, dmeta_p dir);

#endif
