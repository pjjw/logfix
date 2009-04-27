/*
 * Heavily based on Jeff Bonwick's code in this discussion:
 * http://opensolaris.org/jive/thread.jspa?messageID=233666
 *
 * Jeb Campbell <jebc@c4solutions.net>
 * */

#include <inttypes.h>
#include <iso/stdio_iso.h>

#include <devid.h>
#include <dirent.h>
#include <errno.h>
#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>

#include <sys/vdev_impl.h>

/*
 *  * Write a label block with a ZBT checksum.
 *   */
static void
label_write(int fd, uint64_t offset, uint64_t size, void *buf)
{
	zio_block_tail_t *zbt, zbt_orig;
	zio_cksum_t zc;
	zio_checksum_info_t *ci = &zio_checksum_table[ZIO_CHECKSUM_LABEL];

	zbt = (zio_block_tail_t *)((char *)buf + size) - 1;
	zbt_orig = *zbt;
	
	ASSERT((uint_t)checksum < ZIO_CHECKSUM_FUNCTIONS);
	ASSERT(ci->ci_func[0] != NULL);

	ZIO_SET_CHECKSUM(&zbt->zbt_cksum, offset, 0, 0, 0);
	
	ci->ci_func[0](buf, size, &zc);

	VERIFY(pwrite64(fd, buf, size, offset) == size);

	*zbt = zbt_orig;
}

int
main(int argc, char **argv)
{
	int fd_pool;
	int fd_log;
	vdev_label_t vl_pool;
	vdev_label_t vl_log;
	nvlist_t *config_pool;
	nvlist_t *config_log;

	uint64_t guid;		// ZPOOL_CONFIG_GUID
	uint64_t is_log;	// ZPOOL_CONFIG_IS_LOG
	nvlist_t *vdev_tree;	// ZPOOL_CONFIG_VDEV_TREE

	char *buf;
	size_t buflen;

	VERIFY(argc == 4);
	VERIFY((fd_pool = open(argv[1], O_RDWR)) != -1);
	VERIFY((fd_log = open(argv[2], O_RDWR)) != -1);
	VERIFY(sscanf(argv[3], "%" SCNu64 , &guid) == 1);
	//guid = 9851295902337437618ULL;

	VERIFY(pread64(fd_pool, &vl_pool, sizeof (vdev_label_t), 0) ==
	    sizeof (vdev_label_t));
	VERIFY(nvlist_unpack(vl_pool.vl_vdev_phys.vp_nvlist,
	    sizeof (vl_pool.vl_vdev_phys.vp_nvlist), &config_pool, 0) == 0);
	VERIFY(pread64(fd_log, &vl_log, sizeof (vdev_label_t), 0) ==
	    sizeof (vdev_label_t));
	VERIFY(nvlist_unpack(vl_log.vl_vdev_phys.vp_nvlist,
	    sizeof (vl_log.vl_vdev_phys.vp_nvlist), &config_log, 0) == 0);

	// save what we want from config_log -- is_log, vdev_tree
	VERIFY(nvlist_lookup_uint64(config_log, ZPOOL_CONFIG_IS_LOG, &is_log) == 0);
	VERIFY(nvlist_lookup_nvlist(config_log, ZPOOL_CONFIG_VDEV_TREE, &vdev_tree) == 0);

	// fix guid for vdev_log
	VERIFY(nvlist_remove_all(vdev_tree, ZPOOL_CONFIG_GUID) == 0);
	VERIFY(nvlist_add_uint64(vdev_tree, ZPOOL_CONFIG_GUID, guid) == 0);

	// remove what we are going to replace on config_pool
	VERIFY(nvlist_remove_all(config_pool, ZPOOL_CONFIG_TOP_GUID) == 0);
	VERIFY(nvlist_remove_all(config_pool, ZPOOL_CONFIG_GUID) == 0);
	VERIFY(nvlist_remove_all(config_pool, ZPOOL_CONFIG_VDEV_TREE) == 0);

	// add back what we want 
	VERIFY(nvlist_add_uint64(config_pool, ZPOOL_CONFIG_TOP_GUID, guid) == 0);
	VERIFY(nvlist_add_uint64(config_pool, ZPOOL_CONFIG_GUID, guid) == 0);
	VERIFY(nvlist_add_uint64(config_pool, ZPOOL_CONFIG_IS_LOG, is_log) == 0);
	VERIFY(nvlist_add_nvlist(config_pool, ZPOOL_CONFIG_VDEV_TREE, vdev_tree) == 0);

	buf = vl_pool.vl_vdev_phys.vp_nvlist;
	buflen = sizeof (vl_pool.vl_vdev_phys.vp_nvlist);
	VERIFY(nvlist_pack(config_pool, &buf, &buflen, NV_ENCODE_XDR, 0) == 0);

	label_write(fd_log, offsetof(vdev_label_t, vl_vdev_phys),
	    VDEV_PHYS_SIZE, &vl_pool.vl_vdev_phys);

	fsync(fd_log);

	return (0);
}
