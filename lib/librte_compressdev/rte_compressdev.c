/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_interrupts.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>

#include "rte_comp.h"
#include "rte_compressdev.h"
#include "rte_compressdev_pmd.h"

static uint8_t nb_drivers;

struct rte_compressdev rte_comp_devices[RTE_COMPRESS_MAX_DEVS];

struct rte_compressdev *rte_compressdevs = &rte_comp_devices[0];

static struct rte_compressdev_global compressdev_globals = {
		.devs			= &rte_comp_devices[0],
		.data			= { NULL },
		.nb_devs		= 0,
		.max_devs		= RTE_COMPRESS_MAX_DEVS
};

struct rte_compressdev_global *rte_compressdev_globals = &compressdev_globals;

/**
 * The compression algorithm strings identifiers.
 * It could be used in application command line.
 */
const char *
rte_comp_algorithm_strings[] = {
	[RTE_COMP_ALGO_DEFLATE]		= "deflate",
	[RTE_COMP_ALGO_LZS]		= "lzs"
};


#define param_range_check(x, y) \
	(((x < y.min) || (x > y.max)) || \
	(y.increment != 0 && (x % y.increment) != 0))


const char * __rte_experimental
rte_compressdev_get_feature_name(uint64_t flag)
{
	switch (flag) {
	case RTE_COMPDEV_FF_HW_ACCELERATED:
		return "HW_ACCELERATED";
	case RTE_COMPDEV_FF_CPU_SSE:
		return "CPU_SSE";
	case RTE_COMPDEV_FF_CPU_AVX:
		return "CPU_AVX";
	case RTE_COMPDEV_FF_CPU_AVX2:
		return "CPU_AVX2";
	case RTE_COMPDEV_FF_CPU_AVX512:
		return "CPU_AVX512";
	case RTE_COMPDEV_FF_CPU_NEON:
		return "CPU_NEON";
	default:
		return NULL;
	}
}

const char * __rte_experimental
rte_comp_get_feature_name(uint64_t flag)
{
	switch (flag) {
	case RTE_COMP_FF_STATEFUL_COMPRESSION:
		return "STATEFUL_COMPRESSION";
	case RTE_COMP_FF_STATEFUL_DECOMPRESSION:
		return "STATEFUL_DECOMPRESSION";
	case RTE_COMP_FF_MBUF_SCATTER_GATHER:
		return "MBUF_SCATTER_GATHER";
	case RTE_COMP_FF_MULTI_PKT_CHECKSUM:
		return "MULTI_PKT_CHECKSUM";
	case RTE_COMP_FF_ADLER32_CHECKSUM:
		return "ADLER32_CHECKSUM";
	case RTE_COMP_FF_CRC32_CHECKSUM:
		return "CRC32_CHECKSUM";
	case RTE_COMP_FF_CRC32_ADLER32_CHECKSUM:
		return "CRC32_ADLER32_CHECKSUM";
	case RTE_COMP_FF_NONCOMPRESSED_BLOCKS:
		return "NONCOMPRESSED_BLOCKS";
	default:
		return NULL;
	}
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_get_dev(uint8_t dev_id)
{
	return &rte_compressdev_globals->devs[dev_id];
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_get_named_dev(const char *name)
{
	struct rte_compressdev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < rte_compressdev_globals->max_devs; i++) {
		dev = &rte_compressdev_globals->devs[i];

		if ((dev->attached == RTE_COMPRESSDEV_ATTACHED) &&
				(strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

unsigned int __rte_experimental
rte_compressdev_pmd_is_valid_dev(uint8_t dev_id)
{
	struct rte_compressdev *dev = NULL;

	if (dev_id >= rte_compressdev_globals->nb_devs)
		return 0;

	dev = rte_compressdev_pmd_get_dev(dev_id);
	if (dev->attached != RTE_COMPRESSDEV_ATTACHED)
		return 0;
	else
		return 1;
}


int __rte_experimental
rte_compressdev_get_dev_id(const char *name)
{
	unsigned int i;

	if (name == NULL)
		return -1;

	for (i = 0; i < rte_compressdev_globals->nb_devs; i++)
		if ((strcmp(rte_compressdev_globals->devs[i].data->name, name)
				== 0) &&
				(rte_compressdev_globals->devs[i].attached ==
						RTE_COMPRESSDEV_ATTACHED))
			return i;

	return -1;
}

uint8_t __rte_experimental
rte_compressdev_count(void)
{
	return rte_compressdev_globals->nb_devs;
}

uint8_t __rte_experimental
rte_compressdev_device_count_by_driver(uint8_t driver_id)
{
	uint8_t i, dev_count = 0;

	for (i = 0; i < rte_compressdev_globals->max_devs; i++)
		if (rte_compressdev_globals->devs[i].driver_id == driver_id &&
			rte_compressdev_globals->devs[i].attached ==
					RTE_COMPRESSDEV_ATTACHED)
			dev_count++;

	return dev_count;
}

uint8_t __rte_experimental
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
	uint8_t nb_devices)
{
	uint8_t i, count = 0;
	struct rte_compressdev *devs = rte_compressdev_globals->devs;
	uint8_t max_devs = rte_compressdev_globals->max_devs;

	for (i = 0; i < max_devs && count < nb_devices;	i++) {

		if (devs[i].attached == RTE_COMPRESSDEV_ATTACHED) {
			int cmp;

			cmp = strncmp(devs[i].device->driver->name,
					driver_name,
					strlen(driver_name));

			if (cmp == 0)
				devices[count++] = devs[i].data->dev_id;
		}
	}

	return count;
}


int __rte_experimental
rte_compressdev_socket_id(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id))
		return -1;

	dev = rte_compressdev_pmd_get_dev(dev_id);

	return dev->data->socket_id;
}

static inline int
rte_compressdev_data_alloc(uint8_t dev_id, struct rte_compressdev_data **data,
		int socket_id)
{
	char mz_name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	const struct rte_memzone *mz;
	int n;

	/* generate memzone name */
	n = snprintf(mz_name, sizeof(mz_name),
			"rte_compressdev_data_%u", dev_id);
	if (n >= (int)sizeof(mz_name))
		return -EINVAL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		mz = rte_memzone_reserve(mz_name,
				sizeof(struct rte_compressdev_data),
				socket_id, 0);
	} else
		mz = rte_memzone_lookup(mz_name);

	if (mz == NULL)
		return -ENOMEM;

	*data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(*data, 0, sizeof(struct rte_compressdev_data));

	return 0;
}

static uint8_t
rte_compressdev_find_free_device_index(void)
{
	uint8_t dev_id;

	for (dev_id = 0; dev_id < RTE_COMPRESS_MAX_DEVS; dev_id++) {
		if (rte_comp_devices[dev_id].attached ==
				RTE_COMPRESSDEV_DETACHED)
			return dev_id;
	}
	return RTE_COMPRESS_MAX_DEVS;
}

struct rte_compressdev * __rte_experimental
rte_compressdev_pmd_allocate(const char *name, int socket_id)
{
	struct rte_compressdev *compressdev;
	uint8_t dev_id;

	if (rte_compressdev_pmd_get_named_dev(name) != NULL) {
		COMPRESSDEV_LOG(ERR,
			"comp device with name %s already allocated!", name);
		return NULL;
	}

	dev_id = rte_compressdev_find_free_device_index();
	if (dev_id == RTE_COMPRESS_MAX_DEVS) {
		COMPRESSDEV_LOG(ERR, "Reached maximum number of comp devices");
		return NULL;
	}

	compressdev = rte_compressdev_pmd_get_dev(dev_id);

	if (compressdev->data == NULL) {
		struct rte_compressdev_data *compressdev_data =
				compressdev_globals.data[dev_id];

		int retval = rte_compressdev_data_alloc(dev_id,
				&compressdev_data, socket_id);

		if (retval < 0 || compressdev_data == NULL)
			return NULL;

		compressdev->data = compressdev_data;

		snprintf(compressdev->data->name, RTE_COMPRESSDEV_NAME_MAX_LEN,
				"%s", name);

		compressdev->data->dev_id = dev_id;
		compressdev->data->socket_id = socket_id;
		compressdev->data->dev_started = 0;

		compressdev->attached = RTE_COMPRESSDEV_ATTACHED;

		compressdev_globals.nb_devs++;
	}

	return compressdev;
}

int __rte_experimental
rte_compressdev_pmd_release_device(struct rte_compressdev *compressdev)
{
	int ret;

	if (compressdev == NULL)
		return -EINVAL;

	/* Close device only if device operations have been set */
	if (compressdev->dev_ops) {
		ret = rte_compressdev_close(compressdev->data->dev_id);
		if (ret < 0)
			return ret;
	}

	compressdev->attached = RTE_COMPRESSDEV_DETACHED;
	compressdev_globals.nb_devs--;
	return 0;
}

uint16_t __rte_experimental
rte_compressdev_queue_pair_count(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	dev = &rte_comp_devices[dev_id];
	return dev->data->nb_queue_pairs;
}

static int
rte_compressdev_queue_pairs_config(struct rte_compressdev *dev,
		uint16_t nb_qpairs, int socket_id)
{
	struct rte_compressdev_info dev_info;
	void **qp;
	unsigned int i;

	if ((dev == NULL) || (nb_qpairs < 1)) {
		COMPRESSDEV_LOG(ERR, "invalid param: dev %p, nb_queues %u",
							dev, nb_qpairs);
		return -EINVAL;
	}

	COMPRESSDEV_LOG(DEBUG, "Setup %d queues pairs on device %u",
			nb_qpairs, dev->data->dev_id);

	memset(&dev_info, 0, sizeof(struct rte_compressdev_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);

	if (nb_qpairs > (dev_info.max_nb_queue_pairs)) {
		COMPRESSDEV_LOG(ERR, "Invalid num queue_pairs (%u) for dev %u",
				nb_qpairs, dev->data->dev_id);
		return -EINVAL;
	}

	if (dev->data->queue_pairs == NULL) { /* first time configuration */
		dev->data->queue_pairs = rte_zmalloc_socket(
				"compressdev->queue_pairs",
				sizeof(dev->data->queue_pairs[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE, socket_id);

		if (dev->data->queue_pairs == NULL) {
			dev->data->nb_queue_pairs = 0;
			COMPRESSDEV_LOG(ERR,
			"failed to get memory for qp meta data, nb_queues %u",
							nb_qpairs);
			return -(ENOMEM);
		}
	} else { /* re-configure */
		int ret;
		uint16_t old_nb_queues = dev->data->nb_queue_pairs;

		qp = dev->data->queue_pairs;

		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_release,
				-ENOTSUP);

		for (i = nb_qpairs; i < old_nb_queues; i++) {
			ret = (*dev->dev_ops->queue_pair_release)(dev, i);
			if (ret < 0)
				return ret;
		}

		qp = rte_realloc(qp, sizeof(qp[0]) * nb_qpairs,
				RTE_CACHE_LINE_SIZE);
		if (qp == NULL) {
			COMPRESSDEV_LOG(ERR,
			"failed to realloc qp meta data, nb_queues %u",
						nb_qpairs);
			return -(ENOMEM);
		}

		if (nb_qpairs > old_nb_queues) {
			uint16_t new_qs = nb_qpairs - old_nb_queues;

			memset(qp + old_nb_queues, 0,
				sizeof(qp[0]) * new_qs);
		}

		dev->data->queue_pairs = qp;

	}
	dev->data->nb_queue_pairs = nb_qpairs;
	return 0;
}

int __rte_experimental
rte_compressdev_configure(uint8_t dev_id, struct rte_compressdev_config *config)
{
	struct rte_compressdev *dev;
	int diag;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	if (dev->data->dev_started) {
		COMPRESSDEV_LOG(ERR,
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	/* Setup new number of queue pairs and reconfigure device. */
	diag = rte_compressdev_queue_pairs_config(dev, config->nb_queue_pairs,
			config->socket_id);
	if (diag != 0) {
		COMPRESSDEV_LOG(ERR,
			"dev%d rte_comp_dev_queue_pairs_config = %d",
				dev_id, diag);
		return diag;
	}

	return (*dev->dev_ops->dev_configure)(dev, config);
}


int __rte_experimental
rte_compressdev_start(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int diag;

	COMPRESSDEV_LOG(DEBUG, "Start dev_id=%" PRIu8, dev_id);

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already started", dev_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	return 0;
}

void __rte_experimental
rte_compressdev_stop(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		COMPRESSDEV_LOG(ERR,
		    "Device with dev_id=%" PRIu8 " already stopped", dev_id);
		return;
	}

	(*dev->dev_ops->dev_stop)(dev);
	dev->data->dev_started = 0;
}

int __rte_experimental
rte_compressdev_close(uint8_t dev_id)
{
	struct rte_compressdev *dev;
	int retval;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -1;
	}

	dev = &rte_comp_devices[dev_id];

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		COMPRESSDEV_LOG(ERR, "Device %u must be stopped before closing",
				dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	retval = (*dev->dev_ops->dev_close)(dev);

	if (retval < 0)
		return retval;

	return 0;
}

int __rte_experimental
rte_compressdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];
	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		COMPRESSDEV_LOG(ERR, "Invalid queue_pair_id=%d", queue_pair_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		COMPRESSDEV_LOG(ERR,
		    "device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_pair_setup, -ENOTSUP);

	return (*dev->dev_ops->queue_pair_setup)(dev, queue_pair_id,
			max_inflight_ops, socket_id);
}


int __rte_experimental
rte_compressdev_stats_get(uint8_t dev_id, struct rte_compressdev_stats *stats)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return -ENODEV;
	}

	if (stats == NULL) {
		COMPRESSDEV_LOG(ERR, "Invalid stats ptr");
		return -EINVAL;
	}

	dev = &rte_comp_devices[dev_id];
	memset(stats, 0, sizeof(*stats));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	(*dev->dev_ops->stats_get)(dev, stats);
	return 0;
}

void __rte_experimental
rte_compressdev_stats_reset(uint8_t dev_id)
{
	struct rte_compressdev *dev;

	if (!rte_compressdev_pmd_is_valid_dev(dev_id)) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%" PRIu8, dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->stats_reset);
	(*dev->dev_ops->stats_reset)(dev);
}


void __rte_experimental
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info)
{
	struct rte_compressdev *dev;

	if (dev_id >= compressdev_globals.nb_devs) {
		COMPRESSDEV_LOG(ERR, "Invalid dev_id=%d", dev_id);
		return;
	}

	dev = &rte_comp_devices[dev_id];

	memset(dev_info, 0, sizeof(struct rte_compressdev_info));

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);

	dev_info->driver_name = dev->device->driver->name;
}

int __rte_experimental
rte_compressdev_private_xform_create(uint8_t dev_id,
		struct rte_comp_xform *xform,
		void **priv_xform)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_pmd_get_dev(dev_id);

	if (xform == NULL || priv_xform == NULL || dev == NULL)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->private_xform_create, -ENOTSUP);
	ret = (*dev->dev_ops->private_xform_create)(dev, xform, priv_xform);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to create private_xform: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}

int __rte_experimental
rte_compressdev_private_xform_free(uint8_t dev_id, void *priv_xform)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_pmd_get_dev(dev_id);

	if (dev == NULL || priv_xform == NULL)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->private_xform_free, -ENOTSUP);
	ret = dev->dev_ops->private_xform_free(dev, priv_xform);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to free private xform: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}

int __rte_experimental
rte_compressdev_stream_create(uint8_t dev_id,
		struct rte_comp_xform *xform,
		void **stream)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_pmd_get_dev(dev_id);

	if (xform == NULL || dev == NULL || stream == NULL)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stream_create, -ENOTSUP);
	ret = (*dev->dev_ops->stream_create)(dev, xform, stream);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to create stream: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}


int __rte_experimental
rte_compressdev_stream_free(uint8_t dev_id, void *stream)
{
	struct rte_compressdev *dev;
	int ret;

	dev = rte_compressdev_pmd_get_dev(dev_id);

	if (dev == NULL || stream == NULL)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stream_free, -ENOTSUP);
	ret = dev->dev_ops->stream_free(dev, stream);
	if (ret < 0) {
		COMPRESSDEV_LOG(ERR,
			"dev_id %d failed to free stream: err=%d",
			dev_id, ret);
		return ret;
	};

	return 0;
}


/** Initialise rte_comp_op mempool element */
static void
rte_comp_op_init(struct rte_mempool *mempool,
		__rte_unused void *opaque_arg,
		void *_op_data,
		__rte_unused unsigned int i)
{
	struct rte_comp_op *op = _op_data;

	memset(_op_data, 0, mempool->elt_size);

	op->status = RTE_COMP_OP_STATUS_NOT_PROCESSED;
	op->phys_addr = rte_mem_virt2iova(_op_data);
	op->mempool = mempool;
}


struct rte_mempool * __rte_experimental
rte_comp_op_pool_create(const char *name,
		unsigned int nb_elts, unsigned int cache_size,
		uint16_t user_size, int socket_id)
{
	struct rte_comp_op_pool_private *priv;

	unsigned int elt_size = sizeof(struct rte_comp_op) + user_size;

	/* lookup mempool in case already allocated */
	struct rte_mempool *mp = rte_mempool_lookup(name);

	if (mp != NULL) {
		priv = (struct rte_comp_op_pool_private *)
				rte_mempool_get_priv(mp);

		if (mp->elt_size != elt_size ||
				mp->cache_size < cache_size ||
				mp->size < nb_elts ||
				priv->user_size <  user_size) {
			mp = NULL;
			COMPRESSDEV_LOG(ERR,
		"Mempool %s already exists but with incompatible parameters",
					name);
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(
			name,
			nb_elts,
			elt_size,
			cache_size,
			sizeof(struct rte_comp_op_pool_private),
			NULL,
			NULL,
			rte_comp_op_init,
			NULL,
			socket_id,
			0);

	if (mp == NULL) {
		COMPRESSDEV_LOG(ERR, "Failed to create mempool %s", name);
		return NULL;
	}

	priv = (struct rte_comp_op_pool_private *)
			rte_mempool_get_priv(mp);

	priv->user_size = user_size;

	return mp;
}

TAILQ_HEAD(compressdev_driver_list, compressdev_driver);

static struct compressdev_driver_list compressdev_driver_list =
	TAILQ_HEAD_INITIALIZER(compressdev_driver_list);

int __rte_experimental
rte_compressdev_driver_id_get(const char *name)
{
	struct compressdev_driver *driver;
	const char *driver_name;

	if (name == NULL) {
		COMPRESSDEV_LOG(DEBUG, "name pointer NULL");
		return -1;
	}

	TAILQ_FOREACH(driver, &compressdev_driver_list, next) {
		driver_name = driver->driver->name;
		if (strncmp(driver_name, name, strlen(driver_name)) == 0)
			return driver->id;
	}
	return -1;
}

const char * __rte_experimental
rte_compressdev_name_get(uint8_t dev_id)
{
	struct rte_compressdev *dev = rte_compressdev_pmd_get_dev(dev_id);

	if (dev == NULL)
		return NULL;

	return dev->data->name;
}

const char * __rte_experimental
rte_compressdev_driver_name_get(uint8_t driver_id)
{
	struct compressdev_driver *driver;

	TAILQ_FOREACH(driver, &compressdev_driver_list, next)
		if (driver->id == driver_id)
			return driver->driver->name;
	return NULL;
}

uint8_t
rte_compressdev_allocate_driver(struct compressdev_driver *comp_drv,
		const struct rte_driver *drv)
{
	comp_drv->driver = drv;
	comp_drv->id = nb_drivers;

	TAILQ_INSERT_TAIL(&compressdev_driver_list, comp_drv, next);

	return nb_drivers++;
}

RTE_INIT(rte_compressdev_log);

static void
rte_compressdev_log(void)
{
	compressdev_logtype = rte_log_register("librte.compressdev");
	if (compressdev_logtype >= 0)
		rte_log_set_level(compressdev_logtype, RTE_LOG_NOTICE);
}
