/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_FS_ZFS_ZNODE_H
#define	_SYS_FS_ZFS_ZNODE_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef _KERNEL
#include <sys/isa_defs.h>
#include <sys/types32.h>
#include <sys/list.h>
#include <sys/dmu.h>
#include <sys/zfs_vfsops.h>
#endif
#include <sys/zfs_acl.h>
#include <sys/zil.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Define special zfs pflags
 */
#define	ZFS_XATTR	0x1		/* is an extended attribute */
#define	ZFS_INHERIT_ACE	0x2		/* ace has inheritable ACEs */

#define	MASTER_NODE_OBJ	1

/*
 * special attributes for master node.
 */

#define	ZFS_FSID		"FSID"
#define	ZFS_DELETE_QUEUE	"DELETE_QUEUE"
#define	ZFS_ROOT_OBJ		"ROOT"
#define	ZFS_VERSION_OBJ		"VERSION"
#define	ZFS_PROP_BLOCKPERPAGE	"BLOCKPERPAGE"
#define	ZFS_PROP_NOGROWBLOCKS	"NOGROWBLOCKS"

#define	ZFS_FLAG_BLOCKPERPAGE	0x1
#define	ZFS_FLAG_NOGROWBLOCKS	0x2

/*
 * ZFS version - rev'd whenever an incompatible on-disk format change
 * occurs.  Independent of SPA/DMU/ZAP versioning.
 */

#define	ZFS_VERSION		1ULL

#define	ZFS_MAX_BLOCKSIZE	(SPA_MAXBLOCKSIZE)

/* Path component length */
/*
 * The generic fs code uses MAXNAMELEN to represent
 * what the largest component length is.  Unfortunately,
 * this length includes the terminating NULL.  ZFS needs
 * to tell the users via pathconf() and statvfs() what the
 * true maximum length of a component is, excluding the NULL.
 */
#define	ZFS_MAXNAMELEN	(MAXNAMELEN - 1)

/*
 * This is the persistent portion of the znode.  It is stored
 * in the "bonus buffer" of the file.  Short symbolic links
 * are also stored in the bonus buffer.
 */
typedef struct znode_phys {
	uint64_t zp_atime[2];		/*  0 - last file access time */
	uint64_t zp_mtime[2];		/* 16 - last file modification time */
	uint64_t zp_ctime[2];		/* 32 - last file change time */
	uint64_t zp_crtime[2];		/* 48 - creation time */
	uint64_t zp_gen;		/* 64 - generation (txg of creation) */
	uint64_t zp_mode;		/* 72 - file mode bits */
	uint64_t zp_size;		/* 80 - size of file */
	uint64_t zp_parent;		/* 88 - directory parent (`..') */
	uint64_t zp_links;		/* 96 - number of links to file */
	uint64_t zp_xattr;		/* 104 - DMU object for xattrs */
	uint64_t zp_rdev;		/* 112 - dev_t for VBLK & VCHR files */
	uint64_t zp_flags;		/* 120 - persistent flags */
	uint64_t zp_uid;		/* 128 - file owner */
	uint64_t zp_gid;		/* 136 - owning group */
	uint64_t zp_pad[4];		/* 144 - future */
	zfs_znode_acl_t zp_acl;		/* 176 - 263 ACL */
	/*
	 * Data may pad out any remaining bytes in the znode buffer, eg:
	 *
	 * |<---------------------- dnode_phys (512) ------------------------>|
	 * |<-- dnode (192) --->|<----------- "bonus" buffer (320) ---------->|
	 *			|<---- znode (264) ---->|<---- data (56) ---->|
	 *
	 * At present, we only use this space to store symbolic links.
	 */
} znode_phys_t;

/*
 * Directory entry locks control access to directory entries.
 * They are used to protect creates, deletes, and renames.
 * Each directory znode has a mutex and a list of locked names.
 */
#ifdef _KERNEL
typedef struct zfs_dirlock {
	char		*dl_name;	/* directory entry being locked */
	uint32_t	dl_sharecnt;	/* 0 if exclusive, > 0 if shared */
	uint16_t	dl_namesize;	/* set if dl_name was allocated */
	kcondvar_t	dl_cv;		/* wait for entry to be unlocked */
	struct znode	*dl_dzp;	/* directory znode */
	struct zfs_dirlock *dl_next;	/* next in z_dirlocks list */
} zfs_dirlock_t;

struct zcache_state;

typedef struct znode {
	struct zfsvfs	*z_zfsvfs;
	vnode_t		*z_vnode;
	list_node_t 	z_list_node;	/* deleted znodes */
	uint64_t	z_id;		/* object ID for this znode */
	kmutex_t	z_lock;		/* znode modification lock */
	krwlock_t	z_map_lock;	/* page map lock */
	krwlock_t	z_grow_lock;	/* grow block size lock */
	krwlock_t	z_append_lock;	/* append-mode lock */
	zfs_dirlock_t	*z_dirlocks;	/* directory entry lock list */
	uint8_t		z_active;	/* znode is in use */
	uint8_t		z_reap;		/* reap file at last reference */
	uint8_t		z_atime_dirty;	/* atime needs to be synced */
	uint8_t		z_dbuf_held;	/* Is z_dbuf already held? */
	uint_t		z_mapcnt;	/* number of memory maps to file */
	uint_t		z_blksz;	/* block size in bytes */
	uint_t		z_seq;		/* modification sequence number */
	uint64_t	z_last_itx;	/* last ZIL itx on this znode */
	kmutex_t	z_acl_lock;	/* acl data lock */
	list_node_t	z_link_node;	/* all znodes in fs link */
	list_node_t	z_zcache_node;
	struct zcache_state *z_zcache_state;
	uint64_t	z_zcache_access;

	/*
	 * These are dmu managed fields.
	 */
	znode_phys_t	*z_phys;	/* pointer to persistent znode */
	dmu_buf_t	*z_dbuf;	/* buffer containing the z_phys */
} znode_t;

/*
 * The grow_lock is only applicable to "regular" files.
 * The parent_lock is only applicable to directories.
 */
#define	z_parent_lock	z_grow_lock

/*
 * Convert between znode pointers and vnode pointers
 */
#define	ZTOV(ZP)	((ZP)->z_vnode)
#define	VTOZ(VP)	((znode_t *)(VP)->v_data)

/*
 * ZFS_ENTER() is called on entry to each ZFS vnode and vfs operation.
 * ZFS_EXIT() must be called before exitting the vop.
 */
#define	ZFS_ENTER(zfsvfs) \
	{ \
		atomic_add_32(&(zfsvfs)->z_op_cnt, 1); \
		if ((zfsvfs)->z_unmounted1) { \
			ZFS_EXIT(zfsvfs); \
			return (EIO); \
		} \
	}
#define	ZFS_EXIT(zfsvfs) atomic_add_32(&(zfsvfs)->z_op_cnt, -1)

/*
 * Macros for dealing with dmu_buf_hold
 */
#define	ZFS_OBJ_HASH(obj_num)	(obj_num & (ZFS_OBJ_MTX_SZ - 1))
#define	ZFS_OBJ_MUTEX(zp)	\
	(&zp->z_zfsvfs->z_hold_mtx[ZFS_OBJ_HASH(zp->z_id)])
#define	ZFS_OBJ_HOLD_ENTER(zfsvfs, obj_num) \
	mutex_enter(&zfsvfs->z_hold_mtx[ZFS_OBJ_HASH(obj_num)]);

#define	ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num) \
	mutex_exit(&zfsvfs->z_hold_mtx[ZFS_OBJ_HASH(obj_num)])

/*
 * Macros to encode/decode ZFS stored time values from/to struct timespec
 */
#define	ZFS_TIME_ENCODE(tp, stmp)		\
{						\
	stmp[0] = (uint64_t)(tp)->tv_sec; 	\
	stmp[1] = (uint64_t)(tp)->tv_nsec;	\
}

#define	ZFS_TIME_DECODE(tp, stmp)		\
{						\
	(tp)->tv_sec = (time_t)stmp[0];		\
	(tp)->tv_nsec = (long)stmp[1];		\
}

/*
 * Timestamp defines
 */
#define	ACCESSED		(AT_ATIME)
#define	STATE_CHANGED		(AT_CTIME)
#define	CONTENT_MODIFIED	(AT_MTIME | AT_CTIME)

#define	ZFS_ACCESSTIME_STAMP(zfsvfs, zp) \
	if ((zfsvfs)->z_atime && !((zfsvfs)->z_vfs->vfs_flag & VFS_RDONLY)) \
		zfs_time_stamper(zp, ACCESSED, NULL)

extern int	zfs_init_fs(zfsvfs_t *, znode_t **, cred_t *);
extern void	zfs_set_dataprop(objset_t *);
extern void	zfs_create_fs(objset_t *os, cred_t *cr, dmu_tx_t *tx);
extern void	zfs_time_stamper(znode_t *, uint_t, dmu_tx_t *);
extern void	zfs_time_stamper_locked(znode_t *, uint_t, dmu_tx_t *);
extern int	zfs_grow_blocksize(znode_t *, uint64_t, dmu_tx_t *);
extern int	zfs_freesp(znode_t *, uint64_t, uint64_t, int, dmu_tx_t *,
    cred_t *cr);
extern void	zfs_znode_init(void);
extern void	zfs_znode_fini(void);
extern znode_t	*zfs_znode_alloc(zfsvfs_t *, dmu_buf_t *, uint64_t, int);
extern int	zfs_zget(zfsvfs_t *, uint64_t, znode_t **);
extern void	zfs_zinactive(znode_t *);
extern void	zfs_znode_delete(znode_t *, dmu_tx_t *);
extern void	zfs_znode_free(znode_t *);
extern int	zfs_delete_thread_target(zfsvfs_t *zfsvfs, int nthreads);
extern void	zfs_delete_wait_empty(zfsvfs_t *zfsvfs);
extern void	zfs_zcache_flush(zfsvfs_t *zfsvf);
extern void	zfs_remove_op_tables();
extern int	zfs_create_op_tables();
extern int	zfs_sync(vfs_t *vfsp, short flag, cred_t *cr);

extern uint64_t zfs_log_create(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *dzp, znode_t *zp, char *name);
extern uint64_t zfs_log_remove(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *dzp, char *name);
extern uint64_t zfs_log_link(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *dzp, znode_t *zp, char *name);
extern uint64_t zfs_log_symlink(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *dzp, znode_t *zp, char *name, char *link);
extern uint64_t zfs_log_rename(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *sdzp, char *sname, znode_t *tdzp, char *dname, znode_t *szp);
extern uint64_t zfs_log_write(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, offset_t off, ssize_t len, int ioflag, uio_t *uio);
extern uint64_t zfs_log_truncate(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, uint64_t off, uint64_t len);
extern uint64_t zfs_log_setattr(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, vattr_t *vap, uint_t mask_applied);
extern uint64_t zfs_log_acl(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, int aclcnt, ace_t *z_ace);

extern zil_get_data_t zfs_get_data;
extern zil_replay_func_t *zfs_replay_vector[TX_MAX_TYPE];
extern int zfsfstype;

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_ZFS_ZNODE_H */