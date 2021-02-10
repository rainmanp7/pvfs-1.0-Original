/* Hey Emacs! This file is -*- nroff -*- source.
 * 
 * Copyright (c) 1996 Clemson University, All Rights Reserved.
 * 2000 Argonne National Laboratory.
 * Originally Written by Rob Ross.
 *
 * Parallel Architecture Research Laboratory, Clemson University
 * Mathematics and Computer Science Division, Argonne National Laboratory
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
 **Code** This Protected Resource Provided By = rainmanp7
 * Contact:  
 * Rob Ross			    rross@mcs.anl.gov
 * Matt Cettei			mcettei@parl.eng.clemson.edu
 * Chris Brown			muslimsoap@gmail.com
 * Walt Ligon			walt@parl.clemson.edu
 * Robert Latham		robl@mcs.anl.gov
 * Neill Miller 		neillm@mcs.anl.gov
 * Frank Shorter		fshorte@parl.clemson.edu
 * Harish Ramachandran	rharish@parl.clemson.edu
 * Dale Whitchurch		dalew@parl.clemson.edu
 * Mike Speth			mspeth@parl.clemson.edu
 * Brad Settlemyer		bradles@CLEMSON.EDU
 */

/* 
 * This code could potentially use 3 different methods for memory
 * transfers on read and write depending on which option is defined.
 * PVFS_BUFFER_DYNAMIC - dynamically allocate a buffer for each request
 * PVFS_BUFFER_STATIC - use a single statically allocated buffer
 * PVFS_BUFFER_MAPPED - use the kernel patch to map the user's buffer
 *   into kernel space, avoiding the copy and the allocate altogether.
 */

/* These calls are intended to handle communications between 
 *	the kernel low level vfs calls and the user level pvfsd daemon.
 */

#include <config.h>

#define __NO_VERSION__
#include <pvfs_kernel_config.h>

#include <linux/poll.h>

#ifdef HAVE_LINUX_IOBUF_H
#include <linux/iobuf.h>
#endif

#include <ll_pvfs.h>
#include <pvfs_bufmap.h>
#include <pvfs_linux.h>
#include <pvfsdev.h>

/* 2.4 */
#ifdef HAVE_LINUX_TQUEUE_H
#include <linux/tqueue.h>
#endif
#include <linux/slab.h>
#ifdef HAVE_LINUX_DEVFS_FS_KERNEL_H
#include <linux/devfs_fs_kernel.h>
#endif
#ifdef HAVE_LINUX_VMALLOC_H
#include <linux/vmalloc.h>
#endif

/* these operation types are purely for debugging purposes! */
enum {
	TEST_UPWAIT = 667,
	TEST_UPWAIT_COMM = 668,
	TEST_CHEAT_DEC = 669, 
	TEST_RD_XFER = 670, 
	TEST_WR_XFER = 672
};

/* and a maximum sequence number for kernel communications */
enum {
	PVFSDEV_MAX_SEQ = 8000
};


extern int pvfs_buffer;
extern int pvfs_maxsz;




/********************************************************************
 * file operations:
 */

/* visible IO operations on the device */
static int pvfsdev_read(struct file *my_file, char *buf, size_t
	buf_sz, loff_t *offset);
static int pvfsdev_write(struct file *my_file, const char *buf, 
	size_t buf_sz, loff_t *offset);
unsigned int pvfsdev_poll(struct file *my_file, struct 
	poll_table_struct *wait);
int pvfsdev_ioctl (struct inode * inode, struct file * filp, 
	unsigned int cmd, unsigned long arg);
int pvfsdev_open(struct inode * inode, struct file * filp);
int pvfsdev_release(struct inode * inode, struct file * filp);

#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
/* 2.4 */
static struct file_operations pvfsdev_fops =
{
	owner:   THIS_MODULE,
	read:    pvfsdev_read,
	write:   pvfsdev_write,
	poll:    pvfsdev_poll,
	ioctl:   pvfsdev_ioctl,
	open:    pvfsdev_open,
	release: pvfsdev_release,
};
#else
/* 2.2 kernel has 15 entries here */
static struct file_operations pvfsdev_fops =
{
	NULL,                 /* lseek */
   pvfsdev_read,         /* read */
   pvfsdev_write, 		 /* write */
   NULL, 			       /* readdir */
   pvfsdev_poll,         /* poll */
   pvfsdev_ioctl,        /* ioctl */
   NULL,                 /* mmap */
   pvfsdev_open,         /* open */
   NULL,                 
   pvfsdev_release,      /* release */
   NULL,                 /* fsync */
   NULL,                 /* fasync */
   NULL,                 /* check_media_change */
   NULL,                 /* revalidate */
   NULL                  /* lock */
};
#endif

/*********************************************************************
 * various structures used in the support code:
 */

/* buffer and size for memory operations */
struct pvfsdev_mem_reg {
	void* ptr;
	pvfs_size_t size;
};

/* this is the internal representation of the upcalls as needed to
 * be placed on a queue
 */
struct pvfsdev_upq_entry {
	struct pvfs_upcall pending_up;
	struct list_head req_list;
};

/* this is the internal representation of the downcalls as needed to
 * be placed on a queue
 */
struct pvfsdev_downq_entry {
	struct pvfs_downcall pending_down;
	struct list_head req_list;
};

/* sequence number entries in the seq_pending queue */
struct pvfsdev_seq_entry {
	pvfs_seq_t seq;
	struct list_head seq_list;
	int invalid;
	int kernel_read; /* flag indicating that data is going to kernel space */

	/* not the most logical place to store this memory transfer
	 * buffer information, but it will keep me from having to
	 * manipulate an extra queue...
	 */

	/* this is the kiobuf for reads and writes to user areas */
	struct kiobuf *iobuf;
	/* this variable is used to determine what phase of the write
	 * operation we are in... may be phased out later.
	 */
	int step;
	/* this is the pointer and offset for reads and writes to kernel
	 * areas
	 */
	struct pvfsdev_mem_reg kernelbuf;
};




/*********************************************************************
 * internal support functions:
 */

static int queue_len(struct list_head *my_list);
static int down_dequeue(struct list_head *my_list, 
	pvfs_seq_t my_seq, struct pvfs_downcall* out_downcall); 
static int seq_is_pending(struct list_head *my_list, pvfs_seq_t
	my_seq, int* invalid);
static int pvfsdev_down_enqueue(struct pvfs_downcall *in_downcall);
static int remove_seq_pending(struct list_head *my_list, pvfs_seq_t my_seq);
static int invalidate_seq_pending(struct list_head *my_list, pvfs_seq_t 
	my_seq);
static pvfs_seq_t inc_locked_seq(void);
static int up_dequeue(struct list_head *my_list, 
	pvfs_seq_t my_seq, struct pvfs_upcall* out_upcall);
static struct pvfsdev_seq_entry* match_seq_entry(struct list_head *my_list, 
	pvfs_seq_t my_seq);
static int is_fake_useraddr(void);




/**********************************************************************
 * various device internal queues and other device wide structures
 * that require locking before use. 
 */

/* this will be the queue of pending messages to pvfsd */
LIST_HEAD(up_pending);

/* this is the queue of upcall sequence numbers to be matched 
 * with the corresponding downcall in pvfsdev_enqueue.
 */
LIST_HEAD(seq_pending);

/* this is the list of pending downcalls */
LIST_HEAD(down_pending);

/* this the sequence number assigned to upcalls */
static pvfs_seq_t current_seq = 1;

/* this variable keeps up with the number of processes which have
 * opened the device at any given time.  Mostly for sanity checking
 * so that pvfs ops aren't attempted with no pvfsd.
 */
static int nr_daemons;

/* this is a buffer which will be used in transfers between the
 * application and pvfsd if static copies are used (the "static" buffer
 * option)
 */
static void *static_buf = NULL;

/* two semaphores:
 * static_buf_sem - protects our static buffer
 * seq_sem - protects sequence list
 */
#ifdef HAVE_DECLARE_MUTEX
static DECLARE_MUTEX(static_buf_sem);
static DECLARE_MUTEX(seq_sem);
#else
static struct semaphore static_buf_sem = MUTEX;
static struct semaphore seq_sem = MUTEX;
#endif

/* keeps up with the amount of memory allocated for dynamic 
 * and mapped buffers 
 */
static unsigned long pvfsdev_buffer_size = 0;

/* these locks are used to protect the data defined above */
static spinlock_t pvfsdev_up_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t pvfsdev_down_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t pvfsdev_current_seq_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t pvfsdev_nr_daemons_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t pvfsdev_buffer_pool_lock = SPIN_LOCK_UNLOCKED;

/* three device wait queues:
 * pvfsdev_waitq - used for polling
 * pvfsdev_seq_wait - used for sleeping while waiting on a
 *   sequence-matched downcall
 * pvfsdev_buffer_pool_wait - used to sleep when dynamic buffer has
 *   gotten too large
 */
#ifdef HAVE_DECLARE_WAIT_QUEUE_HEAD
static DECLARE_WAIT_QUEUE_HEAD(pvfsdev_waitq);
static DECLARE_WAIT_QUEUE_HEAD(pvfsdev_seq_wait);
static DECLARE_WAIT_QUEUE_HEAD(pvfsdev_buffer_pool_wait);
#else
static struct wait_queue *pvfsdev_waitq = NULL;
static struct wait_queue *pvfsdev_seq_wait = NULL;
static struct wait_queue *pvfsdev_buffer_pool_wait = NULL;
#endif

#ifdef HAVE_DEVFS_SYMS
/* device handle for the character device */
static devfs_handle_t devfs_handle = NULL;
#endif

/**********************************************************************
 * module specific code :
 */

/* pvfsdev_init()
 *
 * Called at module load time.
 */
int pvfsdev_init(void)
{
	/* do I need to do more stuff here?  how 'bout checking the minor
	 * number? 
	 */

#ifdef HAVE_DEVFS_SYMS
	if (devfs_register_chrdev(PVFSD_MAJOR, "pvfsd", &pvfsdev_fops))
#else
	if (register_chrdev(PVFSD_MAJOR, "pvfsd", &pvfsdev_fops))
#endif
	{
		PERROR("pvfsdev : unable to get major %d\n", PVFSD_MAJOR);
		return -ENODEV;
	}

#ifdef HAVE_DEVFS_SYMS
	devfs_handle = devfs_register(NULL, "pvfsd", DEVFS_FL_DEFAULT,
	PVFSD_MAJOR, 0, (S_IFCHR | S_IRUSR | S_IWUSR), &pvfsdev_fops, NULL);
#endif

	/* initialize the sequence counter */
	spin_lock(&pvfsdev_current_seq_lock);
	current_seq = 0;
	spin_unlock(&pvfsdev_current_seq_lock);
	spin_lock(&pvfsdev_nr_daemons_lock);
	nr_daemons = 0;
	spin_unlock(&pvfsdev_nr_daemons_lock);

	if (pvfs_buffer == PVFS_BUFFER_STATIC) {
		down(&static_buf_sem);
		static_buf = vmalloc(pvfs_maxsz);
		up(&static_buf_sem);
		if (static_buf == NULL) return -ENOMEM;
	}

	PDEBUG(D_PSDEV, "pvfsdev: device module loaded (major=%d)\n", PVFSD_MAJOR);
	return 0;
}

/* pvfsdev_cleanup()
 *
 * Called at module unload.
 */
void pvfsdev_cleanup(void)
{
	struct pvfsdev_upq_entry *up_check;
	struct pvfsdev_downq_entry *down_check;
	struct pvfsdev_seq_entry *seq_check;
	int rm_seqs = 0;
	int inv_seqs = 0;

	/* we need to free up the all of the pending lists at this point */

	spin_lock(&pvfsdev_up_lock);
	while(up_pending.next != &up_pending){
		up_check = list_entry(up_pending.next, struct pvfsdev_upq_entry, 
			req_list);
		list_del(up_pending.next);
		kfree(up_check);
	}
	spin_unlock(&pvfsdev_up_lock);

	spin_lock(&pvfsdev_down_lock);
	while(down_pending.next != &down_pending){
		down_check = list_entry(down_pending.next, struct 
			pvfsdev_downq_entry, req_list);
		list_del(down_pending.next);
		kfree(down_check);
	}
	spin_unlock(&pvfsdev_down_lock);

	down(&seq_sem);
	while(seq_pending.next != &seq_pending){
		seq_check = list_entry(seq_pending.next, struct 
			pvfsdev_seq_entry, seq_list);
		rm_seqs++;
		if(seq_check->invalid){
			inv_seqs++;
		}
		list_del(seq_pending.next);
		kfree(seq_check);
	}
	up(&seq_sem);

	if(rm_seqs){
		PERROR("pvfsdev: %d upcall sequences removed, %d of which had
		become invalid.\n", rm_seqs, inv_seqs);
	}

	if (pvfs_buffer == PVFS_BUFFER_STATIC) {
		/* get rid of the static buffer we had laying around */
		down(&static_buf_sem);
		if (static_buf != NULL) {
			vfree(static_buf);
			static_buf = NULL;
		}
		up(&static_buf_sem);
	}

#ifdef HAVE_DEVFS_SYMS
	devfs_unregister(devfs_handle);
	devfs_unregister_chrdev(PVFSD_MAJOR, "pvfsd");
#else
	unregister_chrdev(PVFSD_MAJOR, "pvfsd");
#endif
	PDEBUG(D_PSDEV, "pvfsdev: device module unloaded\n");
	return;
}


/***************************************************************
 * file operation code:
 */

/* pvfsdev_open()
 *
 * Called when a process opens the PVFS device
 *
 * Returns 0 on success, -errno on failure.
 */
int pvfsdev_open(struct inode *inode, struct file *filp)
{

	PDEBUG(D_PSDEV, "pvfsdev: device opened.\n");
	
	/* we only allow one device to exist on the system */
	if ((MINOR(inode->i_rdev)) != 0) {
		return(-ENODEV);
	}

	MOD_INC_USE_COUNT; /* moved here in 2.4 code */

	spin_lock(&pvfsdev_nr_daemons_lock);
	/* we really only allow one daemon to use this device at a time */
	if (nr_daemons != 0) {
		spin_unlock(&pvfsdev_nr_daemons_lock);
		MOD_DEC_USE_COUNT; /* moved here in 2.4 code */
		return(-EBUSY);
	}
	nr_daemons++;
	spin_unlock(&pvfsdev_nr_daemons_lock);

	return(0);
}

/* pvfsdev_release()
 *
 * Called when process closes the device
 */
int pvfsdev_release(struct inode * inode, struct file * filp)
{

	PDEBUG(D_PSDEV, "pvfsdev: device released.\n");

	spin_lock(&pvfsdev_nr_daemons_lock);
	nr_daemons--;
	spin_unlock(&pvfsdev_nr_daemons_lock);
	
	MOD_DEC_USE_COUNT;

	return(0);
}


/* pvfsdev_write()
 *
 * Called when the PVFS daemon writes into the PVFS device file.
 *
 * Returns 0 on success, -errno on failure.
 */
static int pvfsdev_write(struct file *my_file, const char *buf, 
	size_t buf_sz, loff_t *offset){

	struct pvfs_downcall *in_req = NULL;
	int req_sz = sizeof(struct pvfs_downcall);
	int ret = 0;
	int invalid = 0;
	struct pvfsdev_seq_entry *my_seq = NULL;

	PDEBUG(D_DOWNCALL, "pvfsdev: somebody is writing to me\n");
	if(buf_sz != req_sz){
		return -EINVAL;
	}
	
	in_req = kmalloc(req_sz, GFP_KERNEL);
	if(!in_req){
		return -ENOMEM;
	}

	
	ret = copy_from_user(in_req, buf, buf_sz);
	if(ret!=0){
		kfree(in_req);
		return -ENOMEM;
	}

	/* verify the magic */
	if(in_req->magic != PVFS_DOWNCALL_MAGIC){
		kfree(in_req);
		return -EINVAL;
	}

	if(in_req->seq == -1){
		/* this is an independent downcall, do not match it to
		 * an outstanding upcall sequence. */
		/* do something else instead- not defined yet. */
		kfree(in_req);
		return -ENOSYS;
	}

	PDEBUG(D_TIMING, "pvfsdev: writing downcall number %d.\n",
		in_req->seq);
	/* now we should check to see if we are indeed waiting on
	 * this sequence number... */
	if(!seq_is_pending(&seq_pending, in_req->seq, &invalid)){
		/* Uh-oh... something is amok because we don't have the
		 * sequence number for this downcall queued up... bail
		 * out! */
		kfree(in_req);
		PDEBUG(D_TIMING, "pvfsdev: can't match this sequence number.\n");
		return -EINVAL;
	}

	/* at this point we need to be sure this write is still valid
	 * with respect to its comm sequence.  If not just eat it and
	 * return as normal- the user app on the the other side was
	 * probably terminated. */
	if(invalid){
		kfree(in_req);
		PDEBUG(D_TIMING, "pvfsdev: recieved write for invalid seq.\n");
		if(remove_seq_pending(&seq_pending, in_req->seq) < 0){
			PDEBUG(D_TIMING, "pvfsdev: remove of seq_entry failed... \n");
		}
		return req_sz;

	}
	 
	/* now everything should be Ok for submitting this downcall */

	/* now we can do something with it */
	/* use the switch here to catch special test cases as well as
	 * normal requests that require memory transfers. */

	switch(in_req->type){
		case WRITE_OP:
			/* this is the first downcall from the pvfsd associated with a
			 * write.  this is where the memory transfer will occur.
			 */
			PDEBUG(D_SPECIAL, "pvfsdev: performing memory xfer (write).\n");

			/* the step parameter determines which phase of the write
			 * operation we are in...
			 */
			down(&seq_sem);
			if ((my_seq = match_seq_entry(&seq_pending, in_req->seq)) == NULL) {
				kfree(in_req);
				up(&seq_sem);
				PDEBUG(D_SPECIAL, "pvfsdev: couldn't find iobuf!\n");
				return -EINVAL;
			}
			if (!my_seq->step) {
				my_seq->step = 1;
								
				PDEBUG(D_SPECIAL, "pvfsdev: preparing to copy %ld bytes.\n", (long)in_req->xfer.size);
				if (my_seq->kernel_read) {
					ret = copy_to_user(in_req->xfer.ptr, my_seq->kernelbuf.ptr, 
					in_req->xfer.size);
				}
/* only include this option in the code if the patch is in place */
#ifdef HAVE_PVFS_BUFMAP
				else if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
						ret = pvfs_copy_from_userbuf(in_req->xfer.ptr,
						my_seq->iobuf, in_req->xfer.size);
				}
#endif
				else {
					ret = copy_to_user(in_req->xfer.ptr, my_seq->iobuf,
						in_req->xfer.size);
				}

				up(&seq_sem);
				if(ret != 0){
					PDEBUG(D_SPECIAL, "pvfsdev: pvfs memory transfer failure.\n");
					kfree(in_req);
					return -ENOMEM;
				}
		
				/* go ahead and return here because we don't want to 
				 * enqueue this downcall- wait for the next one :) */
				kfree(in_req);
				return req_sz;
			}
			
			up(&seq_sem);
			break;
		/* these are the calls that require memory transfers (application reads). */
		case TEST_RD_XFER:
		case READ_OP:

			PDEBUG(D_SPECIAL, "pvfsdev: performing memory xfer (read).\n");
			down(&seq_sem);
			if((my_seq = match_seq_entry(&seq_pending, in_req->seq)) == NULL){
				kfree(in_req);
				PDEBUG(D_SPECIAL, "pvfsdev: couldn't find iobuf!\n");
				up(&seq_sem);
				return -EINVAL;
			}
			PDEBUG(D_SPECIAL, "pvfsdev: preparing to copy %ld bytes.\n",
			(long)in_req->xfer.size);
			if (my_seq->kernel_read) {
				ret = copy_from_user(my_seq->kernelbuf.ptr, in_req->xfer.ptr, 
				in_req->xfer.size);
			}
/* only include this option in the code if the patch is in place */
#ifdef HAVE_PVFS_BUFMAP
			else if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
				ret = pvfs_copy_to_userbuf(my_seq->iobuf, in_req->xfer.ptr, 
				in_req->xfer.size);
			}
#endif
			else {
				ret = copy_from_user(my_seq->iobuf, in_req->xfer.ptr,
				in_req->xfer.size);
			}
			up(&seq_sem);
			if (ret != 0) {
				PDEBUG(D_SPECIAL,
				"pvfsdev: copy_from_user/pvfs_copy_to_userbuf failure.\n");
				kfree(in_req);
				return -ENOMEM;
			}
			break;
		case GETDENTS_OP:
			PDEBUG(D_SPECIAL, "pvfsdev: performing memory xfer (getdents).\n");
			down(&seq_sem);
			if((my_seq = match_seq_entry(&seq_pending, in_req->seq)) 
				== NULL){
				kfree(in_req);
				up(&seq_sem);
				PDEBUG(D_SPECIAL, "pvfsdev: couldn't find kernelbuf!\n");
				return -EINVAL;
			}

			ret = copy_from_user(my_seq->kernelbuf.ptr, in_req->xfer.ptr, 
				in_req->xfer.size);
			up(&seq_sem);
			if(ret != 0){
				PDEBUG(D_SPECIAL, "pvfsdev: copy_from_user to pvfs_userbuf failure.\n");
				kfree(in_req);
				return -ENOMEM;
			}
			break;

		default:
			/* no memory transfer required for this type of request. */
			break; 
	}
	
	ret = pvfsdev_down_enqueue(in_req);
	if(ret < 0){
		kfree(in_req);
		return ret;
	}
	kfree(in_req);
	
	return req_sz;
}


/* pvfsdev_read()
 *
 * Called when pvfsd reads from the PVFS device.
 *
 * Returns 0 on success, -errno on failure.
 */
static int pvfsdev_read(struct file *my_file, char *buf, size_t buf_sz,
loff_t *offset)
{

	struct pvfsdev_upq_entry *out_entry;

	PDEBUG(D_UPCALL, "pvfsdev: someone is reading from me.\n");
	
	if (buf_sz != sizeof(struct pvfs_upcall)) {
		return -EINVAL;
	}

	spin_lock(&pvfsdev_up_lock);
	if (up_pending.prev != &up_pending) {
		out_entry = list_entry(up_pending.prev, struct pvfsdev_upq_entry, req_list);
		PDEBUG(D_TIMING, "pvfsdev: reading upcall number %d.\n",
			out_entry->pending_up.seq);
		copy_to_user(buf, &(out_entry->pending_up), sizeof(struct pvfs_upcall));
		list_del(up_pending.prev);
		kfree(out_entry);
		spin_unlock(&pvfsdev_up_lock);
		return(sizeof(struct pvfs_upcall));
	}
	else {
		/* nothing in the queue */
		spin_unlock(&pvfsdev_up_lock);
		return 0;
	}

}

/* poll (was select in 2.0 kernel) */
unsigned int pvfsdev_poll(struct file *my_file, struct 
	poll_table_struct *wait){

	/* do I need to increment the module count here? */
	/* nah, that would probably be bad if process selecting on device
	 * prevents it from unloading... */

	unsigned int mask = POLLOUT | POLLWRNORM;
	poll_wait(my_file, &pvfsdev_waitq, wait);
	if(queue_len(&up_pending) > 0){
		mask |= POLLIN | POLLRDNORM;
	}
		
	return mask;
}

/* ioctl is mostly used for debugging right now. */
int pvfsdev_ioctl (struct inode * inode, struct file * filp, 
	unsigned int cmd, unsigned long arg){

	struct pvfs_upcall *loop_up = NULL;
	struct pvfs_downcall *test_req = NULL;
	int ret = 0;
	
	struct ioctl_mem_reg {
		void* ptr;
		pvfs_size_t size;
	} rd_xfer_reg;

	PDEBUG(D_PIOCTL, "pvfsdev: ioctl.\n");

	switch(cmd){
		case(TEST_UPWAIT):
			/* this will exercise the communication mechanism that
			 * sends an upcall and sleeps until the matching downcall
			 * arrives - for testing purposes during development
			 */
			 
			PDEBUG(D_PIOCTL, "pvfsdev: initialized test_upwait\n");

			loop_up = kmalloc(sizeof(struct pvfs_upcall), GFP_KERNEL);
			if(!loop_up){
				kfree(loop_up);
				return(-ENOMEM);
			}
			loop_up->magic = PVFS_UPCALL_MAGIC;
			loop_up->type = TEST_UPWAIT_COMM;

			test_req = kmalloc(sizeof(struct pvfs_downcall), GFP_KERNEL);
			if(!test_req){
				kfree(test_req);
				kfree(loop_up);
				return(-ENOMEM);
			}
			ret = pvfsdev_enqueue(loop_up, test_req, 1);
			if(ret < 0){
				kfree(test_req);
				kfree(loop_up);
				return(ret);
			}
			kfree(test_req);
			kfree(loop_up);
			return(0);
		case(TEST_RD_XFER):
			/* this will exercise the communication mechanism that
			 * can transfer a variable length memory region from one
			 * user program to another.
			 */
			 
			PDEBUG(D_PIOCTL, "pvfsdev: initialized test_rd_xfer\n");
			copy_from_user(&rd_xfer_reg, (struct ioclt_mem_reg*)arg, sizeof(struct
				ioctl_mem_reg));

			loop_up = kmalloc(sizeof(struct pvfs_upcall), GFP_KERNEL);
			if(!loop_up){
				kfree(loop_up);
				return(-ENOMEM);
			}

			loop_up->magic = PVFS_UPCALL_MAGIC;
			loop_up->type = TEST_RD_XFER;
			loop_up->xfer.ptr = rd_xfer_reg.ptr;
			loop_up->xfer.size = rd_xfer_reg.size;

			test_req = kmalloc(sizeof(struct pvfs_downcall), GFP_KERNEL);
			if(!test_req){
				kfree(test_req);
				kfree(loop_up);
				return(-ENOMEM);
			}
			ret = pvfsdev_enqueue(loop_up, test_req, 1);
			if(ret < 0){
				kfree(test_req);
				kfree(loop_up);
				return(ret);
			}
			kfree(test_req);
			kfree(loop_up);
			return(0);

		case(TEST_WR_XFER):
			/* this will exercise the communication mechanism that
			 * can transfer a variable length memory region from one
			 * user program to another.
			 */
			 
			PDEBUG(D_PIOCTL, "pvfsdev: initialized test_wr_xfer\n");
			copy_from_user(&rd_xfer_reg, (struct ioclt_mem_reg*)arg, sizeof(struct
				ioctl_mem_reg));

			loop_up = kmalloc(sizeof(struct pvfs_upcall), GFP_KERNEL);
			if(!loop_up){
				kfree(loop_up);
				return -ENOMEM;
			}

			loop_up->magic = PVFS_UPCALL_MAGIC;
			loop_up->type = TEST_WR_XFER;
			loop_up->xfer.ptr = rd_xfer_reg.ptr;
			loop_up->xfer.size = rd_xfer_reg.size;

			test_req = kmalloc(sizeof(struct pvfs_downcall), GFP_KERNEL);
			if(!test_req){
				kfree(test_req);
				kfree(loop_up);
				return -ENOMEM;
			}
			ret = pvfsdev_enqueue(loop_up, test_req, 1);
			if(ret < 0){
				kfree(test_req);
				kfree(loop_up);
				return ret;
			}
			kfree(test_req);
			kfree(loop_up);
			return 0;


		case(TEST_CHEAT_DEC):
			/* this is for development purposes...
			 * manually decrements the module use count */
			PDEBUG(D_PIOCTL, "pvfsdev: manually decrementing module count.\n");
			MOD_DEC_USE_COUNT;
			return 0;
		default:
			return -ENOSYS;
	}
	
}




/*******************************************************************
 * support functions:
 */


/* queue_len()
 *
 * for debugging, (and also pvfsdev_poll) see how long the outgoing
 * queue is..
 */
static int queue_len(struct list_head *my_list){
	struct list_head *check;
	int count = 0;

	spin_lock(&pvfsdev_up_lock);
	check = my_list->next;
	while(check != my_list){
		count++;
		check = check->next;
	}
	spin_unlock(&pvfsdev_up_lock);
	return count;
}


/* pvfsdev_enqueue()
 *
 * this adds upcalls to the outgoing queue.  If the flag is set,
 * then it will wait on a matching downcall before it returns.
 * Otherwise it just sends the upcall on its way.
 *
 * TODO: Optimize better for the no-patch case when we feel like it.
 * Returns 0 on success, -errno on failure.
 */
int pvfsdev_enqueue(struct pvfs_upcall *in_upcall, struct pvfs_downcall
*out_downcall, int waitflag)
{

	struct pvfsdev_upq_entry *q_upcall;
	struct pvfsdev_seq_entry *q_seq;
#ifdef HAVE_DECLARE_WAITQUEUE
	/* assume we also have DECLARE_WAITQUEUE */
	DECLARE_WAITQUEUE(my_wait, current);
#else
	struct wait_queue my_wait;
#endif
	struct task_struct *tsk = current;
	struct pvfs_upcall trash_up;
	pvfs_seq_t my_seq = 0;	
	int err = 0;
	int mem_xfer = 0;

#ifdef HAVE_PVFS_BUFMAP
	void *pvfs_map_buf = NULL;
	int rw = -1;
#endif
	void *copybuf = NULL;
	int mem_read = 0;

	if ((waitflag != 0) && (waitflag != 1)) {
		/* not called properly */
		return -EINVAL;
	}

	/* need to check the nr_daemons variable and be sure we are ready
	 * to take device requests and pass them to pvfsd. 
	 */
	if (nr_daemons < 1) {
		return -ENODEV;
	}

	if (!waitflag) {
		q_upcall = kmalloc(sizeof(struct pvfsdev_upq_entry), GFP_KERNEL);
		if(!q_upcall){
			return -ENOMEM;
		}
		
		q_upcall->pending_up = *in_upcall;
		q_upcall->pending_up.seq = -1;

		spin_lock(&pvfsdev_up_lock);
		list_add(&(q_upcall->req_list), &up_pending);
		spin_unlock(&pvfsdev_up_lock);

		/* wake stuff up - see poll function */
		wake_up_interruptible(&pvfsdev_waitq);
		
		return 0;
	}
	
	q_seq = kmalloc(sizeof(struct pvfsdev_seq_entry), GFP_KERNEL);
	if (!q_seq) {
		return -ENOMEM;
	}
	q_seq->invalid = 0;

	/* here we are going to check to see if the upcall involves a
	 * memory transfer- if so we are going to go ahead and set up the
	 * pvfs io mapping, if needed.
	 */
	switch (in_upcall->type) {
		case TEST_RD_XFER:
		case READ_OP:
		case WRITE_OP:
		case TEST_WR_XFER:
			/* note that writes are a two step operation.  When we
			 * submit this upcall, the sequence structure needs to be
			 * set so that the step is zero.  
			 */
			if ((in_upcall->xfer.to_kmem != 0) || (is_fake_useraddr())) {
				/* not a user-space buffer; don't map */
				q_seq->kernelbuf.ptr = in_upcall->xfer.ptr;
				q_seq->kernelbuf.size = in_upcall->xfer.size;
				q_seq->kernel_read = 1;
				q_seq->step = 0;
				break;
			}

			/* This following bit of code makes sure that the sum of 
			 * all of the buffers used for the MAPPED or DYNAMIC buffers
			 * doesn't exceed the maxsz defined at module load time.
			 */
			if (pvfs_buffer == PVFS_BUFFER_DYNAMIC || pvfs_buffer ==
				PVFS_BUFFER_MAPPED){

				spin_lock(&pvfsdev_buffer_pool_lock);
				if((pvfsdev_buffer_size + in_upcall->xfer.size) <= pvfs_maxsz){
					pvfsdev_buffer_size += in_upcall->xfer.size;
					spin_unlock(&pvfsdev_buffer_pool_lock);
				}
				else{
					spin_unlock(&pvfsdev_buffer_pool_lock);
					/* uh oh, we have exceeded the max buffer size available */
					PDEBUG(D_SPECIAL, "exceeded buffer size: sleeping.\n");	
					my_wait.task = tsk;
					add_wait_queue(&pvfsdev_buffer_pool_wait, &my_wait);
				dynamic_repeat:
					tsk->state = TASK_INTERRUPTIBLE;
					run_task_queue(&tq_disk);

					/* this lets this mechanism be interrupted if needed */
					if (signal_pending(current)) {
#ifdef HAVE_LINUX_STRUCT_TASK_STRUCT_PENDING
						if (sigismember(&(current->pending.signal), SIGKILL)
						|| sigismember(&(current->pending.signal), SIGINT))
#else
						if (sigismember(&(current->signal), SIGKILL)
						|| sigismember(&(current->signal), SIGINT))
#endif
						{
							PDEBUG(D_PSDEV, "pvfsdev: recieved signal- bailing out.\n");
							tsk->state = TASK_RUNNING;
							remove_wait_queue(&pvfsdev_buffer_pool_wait, &my_wait);
							return -EINTR; 
						}
					}

					/* now we can look again to see if we can allocate a
					 * buffer */
					spin_lock(&pvfsdev_buffer_pool_lock);
					if((pvfsdev_buffer_size + in_upcall->xfer.size) > pvfs_maxsz){
						spin_unlock(&pvfsdev_buffer_pool_lock);
						schedule();
						/* go back to sleep if we can't */
						goto dynamic_repeat;
					}

					/* the memory is available- let's rock! */
					pvfsdev_buffer_size += in_upcall->xfer.size;
					spin_unlock(&pvfsdev_buffer_pool_lock);

					tsk->state = TASK_RUNNING;
					remove_wait_queue(&pvfsdev_buffer_pool_wait, &my_wait);
				}
			}


			/* Here we allocate our buffer for the dynamic copy, or simply
			 * set the pointer to point to the static buffer in the static
			 * case.  This value isn't used by the mapped buffer code.
			 */
			if (pvfs_buffer == PVFS_BUFFER_DYNAMIC) {
				copybuf = (void*)vmalloc(in_upcall->xfer.size);
				if(copybuf == NULL){
					PERROR("pvfsdev: could not allocate memory!\n");
					spin_lock(&pvfsdev_buffer_pool_lock);
						pvfsdev_buffer_size -= in_upcall->xfer.size;
					spin_unlock(&pvfsdev_buffer_pool_lock);
					return(-ENOMEM);
				}
			}
			else if (pvfs_buffer == PVFS_BUFFER_STATIC) {
				/* go ahead and decrement the semaphore while we are at it */
				down(&static_buf_sem);
				copybuf = static_buf;
			}

			if ((in_upcall->type == WRITE_OP)||(in_upcall->type == TEST_WR_XFER))
			{
				/* file system write operation */
				if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
#ifdef HAVE_PVFS_BUFMAP
					rw = PVFS_BUF_READ; /* only used by the mapped technique */
#endif
				}
				else {
					err = copy_from_user(copybuf, in_upcall->xfer.ptr, 
					in_upcall->xfer.size);
				}
			}
			else {
				/* file system read operation */
				if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
#ifdef HAVE_PVFS_BUFMAP
					rw = PVFS_BUF_WRITE;
#endif
				}
				else {
					/* this is a read xfer we need to handle later */
					mem_read = 1;
				}
			}

#ifdef HAVE_PVFS_BUFMAP
			if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
				err = pvfs_map_userbuf(rw, in_upcall->xfer.ptr,
				in_upcall->xfer.size, &pvfs_map_buf);
			}
#endif
			if (err) {
				PERROR("pvfsdev: pvfs memory transfer setup failure.\n");
				spin_lock(&pvfsdev_buffer_pool_lock);
					pvfsdev_buffer_size -= in_upcall->xfer.size;
				spin_unlock(&pvfsdev_buffer_pool_lock);
				return err;
			}

			/* set up our queue entry */
			if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
#ifdef HAVE_PVFS_BUFMAP
				q_seq->iobuf = pvfs_map_buf;
#endif
			}
			else {
				q_seq->iobuf = copybuf;
			}
			q_seq->step = 0;
			q_seq->kernel_read = 0;
			mem_xfer = 1;

			break;
		case GETDENTS_OP:
			if (in_upcall->xfer.to_kmem == 0) {
				/* not supported at this time */
				PDEBUG(D_SPECIAL, "pvfsdev: user-space address to GETDENTS?\n");
				return -EINVAL;
			}
			q_seq->kernelbuf.ptr = in_upcall->xfer.ptr;
			q_seq->kernelbuf.size = in_upcall->xfer.size;
			q_seq->kernel_read = 1;
			
			break;
		default:
			/* not a memory transfer operation */
			break;
	}

	q_upcall = kmalloc(sizeof(struct pvfsdev_upq_entry), GFP_KERNEL);
	if(!q_upcall){
		/* TODO */
		return -ENOMEM;
	}
	
	q_upcall->pending_up = *in_upcall;

	my_seq = inc_locked_seq();
	q_upcall->pending_up.seq = my_seq;
	q_seq->seq = my_seq;

	down(&seq_sem);
	list_add(&(q_seq->seq_list), &seq_pending);
	up(&seq_sem);
	
	spin_lock(&pvfsdev_up_lock);
	list_add(&(q_upcall->req_list), &up_pending);
	spin_unlock(&pvfsdev_up_lock);

	/* wake up processes waiting on outgoing queue */
	wake_up_interruptible(&pvfsdev_waitq);
	
	/* now I want to wait here until a downcall comes in 
	 * that matches the sequence number
	 */
	my_wait.task = tsk;
	add_wait_queue(&pvfsdev_seq_wait, &my_wait);
seq_repeat:
	tsk->state = TASK_INTERRUPTIBLE;
	run_task_queue(&tq_disk);

	/* this lets this mechanism be interrupted if needed */
	if (signal_pending(current)) {
#ifdef HAVE_LINUX_STRUCT_TASK_STRUCT_PENDING
		if (sigismember(&(current->pending.signal), SIGKILL)
		|| sigismember(&(current->pending.signal), SIGINT))
#else
		if (sigismember(&(current->signal), SIGKILL)
		|| sigismember(&(current->signal), SIGINT))
#endif
		{
			PDEBUG(D_PSDEV, "pvfsdev: recieved signal- bailing out.\n");
			/* need to do something reasonable here... */
			/* so subsequent calls don't freak out :) */

			/* at least need to dequeue above stuff and make sure the
			 * module use count is correct
			 */
			tsk->state = TASK_RUNNING;
			remove_wait_queue(&pvfsdev_seq_wait, &my_wait);
			/* we only invalidated the sequence- not remove it.  this
			 * way the other side can still match the downcall and
			 * handle it without pvfsd bombing
			 */
			invalidate_seq_pending(&seq_pending, my_seq);

			up_dequeue(&up_pending, my_seq, &trash_up);

			if(mem_xfer && (pvfs_buffer == PVFS_BUFFER_MAPPED)){
#ifdef HAVE_PVFS_BUFMAP
				pvfs_unmap_userbuf(pvfs_map_buf);
				spin_lock(&pvfsdev_buffer_pool_lock);
					pvfsdev_buffer_size -= in_upcall->xfer.size;
				spin_unlock(&pvfsdev_buffer_pool_lock);
#endif
			}
			else if(mem_xfer && (pvfs_buffer == PVFS_BUFFER_DYNAMIC)){
				if (copybuf != NULL) {
					vfree(copybuf);
					copybuf = NULL;
				}
				spin_lock(&pvfsdev_buffer_pool_lock);
					pvfsdev_buffer_size -= in_upcall->xfer.size;
				spin_unlock(&pvfsdev_buffer_pool_lock);
			}
			return -EINTR; 
		}
	}

	/* here I need to check the seq_pending list and see if my sequence
	 * number has been removed, indicating a matching downcall
	 */
	if (seq_is_pending(&seq_pending, my_seq, NULL)) {
		schedule();
		goto seq_repeat;
	}
	tsk->state = TASK_RUNNING;
	remove_wait_queue(&pvfsdev_seq_wait, &my_wait);

	/* now I need to pull my matched downcall off of the downcall
	 * list and return
	 */
	err = down_dequeue(&down_pending, my_seq, out_downcall); 
	if (err < 0) {
		if(mem_xfer && (pvfs_buffer == PVFS_BUFFER_MAPPED)){
#ifdef HAVE_PVFS_BUFMAP
			pvfs_unmap_userbuf(pvfs_map_buf);
			spin_lock(&pvfsdev_buffer_pool_lock);
				pvfsdev_buffer_size -= in_upcall->xfer.size;
			spin_unlock(&pvfsdev_buffer_pool_lock);
#endif
		}
		else if(mem_xfer && (pvfs_buffer == PVFS_BUFFER_DYNAMIC)){
			if (copybuf != NULL) {
				vfree(copybuf);
				copybuf = NULL;
			}
			spin_lock(&pvfsdev_buffer_pool_lock);
				pvfsdev_buffer_size -= in_upcall->xfer.size;
			spin_unlock(&pvfsdev_buffer_pool_lock);
		}
		return err;
	}

	if (mem_xfer) {
		if (pvfs_buffer == PVFS_BUFFER_MAPPED) {
#ifdef HAVE_PVFS_BUFMAP
			/* at this point, I can free the kiobuf if a memory transfer was
			 * carried out.
			 */
			pvfs_unmap_userbuf(pvfs_map_buf);

			spin_lock(&pvfsdev_buffer_pool_lock);
			pvfsdev_buffer_size -= in_upcall->xfer.size;
			spin_unlock(&pvfsdev_buffer_pool_lock);

			/* let the other processes have a shot at the buffer */
			wake_up_interruptible(&pvfsdev_buffer_pool_wait);
#endif
		}
		else {
			/* at this point, I need to see if this was a read transfer, in
			 * which case it is time to send the data up the the
			 * application...
			 */
			if (mem_read) {
				err = copy_to_user(in_upcall->xfer.ptr, copybuf, 
				in_upcall->xfer.size);
			}
			if (pvfs_buffer == PVFS_BUFFER_DYNAMIC) {
				if (copybuf != NULL) {
					vfree(copybuf);
					copybuf = NULL;
				}
				spin_lock(&pvfsdev_buffer_pool_lock);
				pvfsdev_buffer_size -= in_upcall->xfer.size;
				spin_unlock(&pvfsdev_buffer_pool_lock);

				/* let the other processes have a shot at the buffer */
				wake_up_interruptible(&pvfsdev_buffer_pool_wait);
			}
			else {
				/* implicitly PVFS_BUFFER_STATIC case */
				up(&static_buf_sem);
			}
		}
	}
	return err;
}


/* seq_is_pending()
 *
 * use this to find out if a sequence number is in the seq list
 *
 * now the invalid integer will be set to alert the caller that they
 * have matched a sequence number that has been declared invalid.
 * if they caller does not want to know about that they should set
 * that pointer to NULL
 */
static int seq_is_pending(struct list_head *my_list, pvfs_seq_t my_seq,
int *invalid)
{
	/* for debugging I'm going to print them all out as I go... */
	struct list_head *check;
	struct pvfsdev_seq_entry* check_seq;
	int flag = 0;

	check = my_list->next;
	while(check != my_list){
		check_seq = list_entry(check, struct pvfsdev_seq_entry,
			seq_list);
		if(check_seq->seq == my_seq){
			if(invalid != NULL){
				*invalid = check_seq->invalid;
			}
			flag = 1;
		}
		check = check->next;
	}
	return flag;
}


/* down_dequeue()
 *
 * use this to figure out which downcall to dequeue based on which
 * one matches your sequence number-return null on error...
 *
 * now also returns the fs from the downcall side :)
 *
 * Returns 0 on success, -errno on failure.
 */
static int down_dequeue(struct list_head *my_list, pvfs_seq_t my_seq,
struct pvfs_downcall* out_downcall)
{ 
	struct list_head *check;
	struct pvfsdev_downq_entry *check_down;


	spin_lock(&pvfsdev_down_lock);
	check=my_list->next;
	while(check != my_list){
		check_down = list_entry(check, struct pvfsdev_downq_entry,
			req_list);
		if(check_down->pending_down.seq == my_seq){
			*out_downcall = check_down->pending_down;
			list_del(check);
			kfree(check_down);
			spin_unlock(&pvfsdev_down_lock);
			return 0;
		}
		else{
			check=check->next;
		}
	}
	spin_unlock(&pvfsdev_down_lock);

	PDEBUG(D_DOWNCALL, "pvfsdev: didn't find matching downcall in queue!\n");
	return -EINVAL;
}
		

/* pvfsdev_down_enqueue()
 *
 * this adds downcalls to the incoming queue
 *
 * Returns 0 on success, -errno on failure.
 */
static int pvfsdev_down_enqueue(struct pvfs_downcall *in_downcall)
{
	struct pvfsdev_downq_entry *q_downcall;

	/* we also need to handle removing the sequence number from the
	 * list of pending sequence numbers for upcall/downcall pairs
	 */

	q_downcall = kmalloc(sizeof(struct pvfsdev_downq_entry), GFP_KERNEL);
	if(!q_downcall){
		return -ENOMEM;
	}
	
	q_downcall->pending_down = *in_downcall;

	spin_lock(&pvfsdev_down_lock);
	list_add(&(q_downcall->req_list), &down_pending);
	spin_unlock(&pvfsdev_down_lock);

	/* take the sequence number out of the sequence number list */
	if(remove_seq_pending(&seq_pending, in_downcall->seq) < 0){
		PDEBUG(D_DOWNCALL, "pvfsdev: remove of seq_entry failed... \n");
	}

	/* wake stuff up - see poll function */
	wake_up_interruptible(&pvfsdev_seq_wait);
	
	return 0;
}


/* remove_seq_pending()
 *
 * use this to remove a sequence number from the pending sequence
 * list
 *
 * Returns 0 on success, -errno on failure.
 */
static int remove_seq_pending(struct list_head *my_list, pvfs_seq_t my_seq)
{
	struct list_head *check = NULL;
	struct pvfsdev_seq_entry* check_seq = NULL;
	int flag = 0;

	down(&seq_sem);
	check = my_list->next;
	while ((check != my_list) && (!flag)) {
		check_seq = list_entry(check, struct pvfsdev_seq_entry,
			seq_list);
		if (check_seq->seq == my_seq) {
			flag = 1;
		}
		else {
			check = check->next;
		}
	}
	if (flag) {
		list_del(check);
		kfree(check_seq);
		up(&seq_sem);
		return 0;
	}
	else {
		up(&seq_sem);
		return -EINVAL;
	}
}


/* inc_locked_seq()
 *
 * this simply increments the communication sequence number and
 * returns its new value.
 */
static pvfs_seq_t inc_locked_seq()
{
	pvfs_seq_t ret = 0;

	spin_lock(&pvfsdev_current_seq_lock);
	if (current_seq < PVFSDEV_MAX_SEQ) {
		current_seq++;
	}
	else {
		current_seq = 1;
	}
	ret = current_seq;
	spin_unlock(&pvfsdev_current_seq_lock);

	return ret;
}


/* up_dequeue()
 *
 * this function dequeues specific pending upcalls- mostly used for
 * cleanup mechanisms
 *
 * Returns 0 on success, -errno on failure.
 */
static int up_dequeue(struct list_head *my_list, pvfs_seq_t my_seq,
struct pvfs_upcall* out_upcall)
{ 

	struct pvfsdev_upq_entry *out_entry;
	struct list_head *check = NULL;

	
	spin_lock(&pvfsdev_up_lock);
	check = up_pending.prev;
	while (check != &up_pending) {
		out_entry = list_entry(up_pending.prev, struct pvfsdev_upq_entry, req_list);
		if(out_entry->pending_up.seq == my_seq){
			*out_upcall = out_entry->pending_up;
			list_del(up_pending.prev);
			kfree(out_entry);
			/* we need to get rid of the sequence entry for this upcall too, because there
			 * will never be a matching downcall to clear it. */
			if(remove_seq_pending(&seq_pending, my_seq) < 0){
				PDEBUG(D_DOWNCALL, "pvfsdev: remove of seq_entry failed... \n");
			}
			spin_unlock(&pvfsdev_up_lock);
			return 0;
		}
		check = check->prev;
	}

	spin_unlock(&pvfsdev_up_lock);
	return -EINVAL;
}


/* match_seq_entry()
 *
 * this is going to be a DANGEROUS function
 *
 * it will return a pointer to the seq entry with a given sequence
 * number, but keep in mind that the seq entry in question could be
 * dequeued while you are working on it.  I am using this on the
 * assumption that it will only be called in conjunction with a
 * spinlock on the sequence queue
 */
static struct pvfsdev_seq_entry* match_seq_entry(struct list_head *my_list, 
	pvfs_seq_t my_seq){

	struct list_head *check = NULL;
	struct pvfsdev_seq_entry* check_seq = NULL;
	int flag = 0;

	check = my_list->next;
	while ((check != my_list) && (!flag)) {
		check_seq = list_entry(check, struct pvfsdev_seq_entry, seq_list);
		if (check_seq->seq == my_seq) {
			flag = 1;
		}
		else {
			check = check->next;
		}
	}
	if (flag) {
		return check_seq;
	}
	else {
		return NULL;
	}
}

/* invalidate_seq_pending()
 *
 * this function will be used to set the invalid flag in a sequence
 * list entry. future operations on that sequence will be
 * aware that a previous step in the transfer has failed.
 *
 * Returns 0 on success, -errno on failure.
 */
static int invalidate_seq_pending(struct list_head *my_list, pvfs_seq_t my_seq)
{

	struct list_head *check = NULL;
	struct pvfsdev_seq_entry* check_seq = NULL;
	int flag = 0;

	down(&seq_sem);
	check = my_list->next;
	while ((check != my_list) && (!flag)) {
		check_seq = list_entry(check, struct pvfsdev_seq_entry,
			seq_list);
		if (check_seq->seq == my_seq) {
			flag = 1;
		}
		else {
			check = check->next;
		}
	}
	if (flag) {
		check_seq->invalid = 1;
		up(&seq_sem);
		return 0;
	}
	else {
		up(&seq_sem);
		return -EINVAL;
	}

}

/* is_fake_useraddr()
 *
 * This function tests for the case where someone has used the set_fs()
 * trick to pass a kernel address to a function that normally takes a
 * user address.
 *
 * Returns 0 if this appears to be a real user address, non-zero if not.
 */
static int is_fake_useraddr(void)
{
	mm_segment_t check_fs;
	mm_segment_t check_ds;

	check_fs = get_fs();
	check_ds = get_ds();
	
	if (check_fs.seg == check_ds.seg) {
		PDEBUG(D_SPECIAL, "is_fake_useraddr detected fake address\n");
		return 1;
	}
	return 0;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
