/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Cavium Networks
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_byteorder.h>

#include <math.h>
#include <assert.h>
#include "zlib_pmd_private.h"

static uint8_t compressdev_driver_id;
int zlib_logtype_driver;

static int zlib_remove(struct rte_vdev_device *vdev);

static int
zlib_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;
	struct zlib_private *internals;

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct zlib_private), init_params);
	if (dev == NULL) {
		ZLIB_LOG_ERR("driver %s: create failed", init_params->name);
		return -ENODEV;
	}

	dev->driver_id = compressdev_driver_id;
	dev->dev_ops = rte_zlib_pmd_ops;

	dev->feature_flags = 0;
	dev->feature_flags |= RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
				RTE_COMP_FF_NONCOMPRESSED_BLOCKS |
				RTE_COMP_FF_ADLER32_CHECKSUM;

	internals = dev->data->dev_private;
	internals->max_nb_queue_pairs = ZLIB_PMD_MAX_NB_QUEUE_PAIRS;

	return 0;
}

static int
zlib_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		rte_socket_id()
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);

	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);
	rte_compressdev_pmd_parse_input_args(&init_params, input_args);

	return zlib_create(name, vdev, &init_params);
}

static int
zlib_remove(struct rte_vdev_device *vdev)
{
	struct rte_compressdev *compressdev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	compressdev = rte_compressdev_pmd_get_named_dev(name);
	if (compressdev == NULL)
		return -ENODEV;

	return rte_compressdev_pmd_destroy(compressdev);
}

static struct rte_vdev_driver zlib_pmd_drv = {
	.probe = zlib_probe,
	.remove = zlib_remove
};

RTE_PMD_REGISTER_VDEV(COMPRESSDEV_NAME_ZLIB_PMD, zlib_pmd_drv);
RTE_PMD_REGISTER_ALIAS(COMPRESSDEV_NAME_ZLIB_PMD, compressdev_zlib_pmd);

RTE_INIT(zlib_init_log);

static void
zlib_init_log(void)
{
	zlib_logtype_driver = rte_log_register("compress_zlib");
	if (zlib_logtype_driver >= 0)
		rte_log_set_level(zlib_logtype_driver, RTE_LOG_INFO);
}
