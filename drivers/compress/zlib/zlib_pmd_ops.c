/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2018 Cavium Networks. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_compressdev_pmd.h>

#include "zlib_pmd_private.h"

static const struct rte_compressdev_capabilities zlib_pmd_capabilities[] = {
	{   /* Deflate */
		.algo = RTE_COMP_ALGO_DEFLATE,
		.comp_feature_flags = RTE_COMP_PRIV_XFORM_SHAREABLE,
		.window_size = {
			.min = 8,
			.max = 15,
			.increment = 2
		},
	},

	RTE_COMP_END_OF_CAPABILITIES_LIST()

};

/** Configure device */
static int
zlib_pmd_config(__rte_unused struct rte_compressdev *dev,
		__rte_unused struct rte_compressdev_config *config)
{
	struct rte_mempool *mp;

	struct zlib_private *internals = dev->data->dev_private;
    snprintf(internals->mp_name, RTE_MEMPOOL_NAMESIZE,
					"stream_mp_%u", dev->data->dev_id);
	mp = rte_mempool_create(internals->mp_name,
			config->max_nb_priv_xforms + config->max_nb_streams,
			sizeof(struct zlib_priv_xform), 
			0, 0, NULL, NULL, NULL,
			NULL, config->socket_id,
			0);
	if (mp == NULL) {
		ZLIB_LOG_ERR("Cannot create private xform pool on socket %d\n",
				config->socket_id);
		return -ENOMEM;
	}
	return 0;
}

/** Start device */
static int
zlib_pmd_start(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Stop device */
static void
zlib_pmd_stop(__rte_unused struct rte_compressdev *dev)
{

	struct zlib_private *internals = dev->data->dev_private;
	struct rte_mempool *mp = rte_mempool_lookup(internals->mp_name);
	rte_mempool_free(mp); 
}

/** Close device */
static int
zlib_pmd_close(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Get device statistics */
static void
zlib_pmd_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zlib_qp *qp = dev->data->queue_pairs[qp_id];

		stats->enqueued_count += qp->qp_stats.enqueued_count;
		stats->dequeued_count += qp->qp_stats.dequeued_count;

		stats->enqueue_err_count += qp->qp_stats.enqueue_err_count;
		stats->dequeue_err_count += qp->qp_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
zlib_pmd_stats_reset(struct rte_compressdev *dev)
{
	int qp_id;

	for (qp_id = 0; qp_id < dev->data->nb_queue_pairs; qp_id++) {
		struct zlib_qp *qp = dev->data->queue_pairs[qp_id];

		memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	}
}

/** Get device info */
static void
zlib_pmd_info_get(struct rte_compressdev *dev,
		struct rte_compressdev_info *dev_info)
{
	struct zlib_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_name = dev->device->name;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = zlib_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
	}
}

/** Release queue pair */
static int
zlib_pmd_qp_release(struct rte_compressdev *dev, uint16_t qp_id)
{
	struct zlib_qp *qp = dev->data->queue_pairs[qp_id];
	struct rte_ring *r = NULL;

	if (qp != NULL) {
		r = rte_ring_lookup(qp->name);
		if (r)
			rte_ring_free(r);
		rte_free(qp);
		dev->data->queue_pairs[qp_id] = NULL;
	}
	return 0;
}

/** set a unique name for the queue pair based on it's name, dev_id and qp_id */
static int
zlib_pmd_qp_set_unique_name(struct rte_compressdev *dev,
		struct zlib_qp *qp)
{
	unsigned int n = snprintf(qp->name, sizeof(qp->name),
				"zlib_pmd_%u_qp_%u",
				dev->data->dev_id, qp->id);

	if (n >= sizeof(qp->name))
		return -1;

	return 0;
}

/** Create a ring to place process packets on */
static struct rte_ring *
zlib_pmd_qp_create_processed_pkts_ring(struct zlib_qp *qp,
		unsigned int ring_size, int socket_id)
{
	struct rte_ring *r;

	r = rte_ring_lookup(qp->name);
	if (r) {
		if (rte_ring_get_size(r) >= ring_size) {
			ZLIB_LOG_INFO("Reusing existing ring %s for processed"
					" packets", qp->name);
			return r;
		}

		ZLIB_LOG_ERR("Unable to reuse existing ring %s for processed"
				" packets", qp->name);
		return NULL;
	}

	return rte_ring_create(qp->name, ring_size, socket_id,
						RING_F_EXACT_SZ);
}

/** Setup a queue pair */
static int
zlib_pmd_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		uint32_t max_inflight_ops, int socket_id)
{
	struct zlib_qp *qp = NULL;

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		zlib_pmd_qp_release(dev, qp_id);

	/* Allocate the queue pair data structure. */
	qp = rte_zmalloc_socket("ZLIB PMD Queue Pair", sizeof(*qp),
					RTE_CACHE_LINE_SIZE, socket_id);
	if (qp == NULL)
		return (-ENOMEM);

	qp->id = qp_id;
	dev->data->queue_pairs[qp_id] = qp;

	if (zlib_pmd_qp_set_unique_name(dev, qp))
		goto qp_setup_cleanup;

	qp->processed_pkts = zlib_pmd_qp_create_processed_pkts_ring(qp,
			max_inflight_ops, socket_id);
	if (qp->processed_pkts == NULL)
		goto qp_setup_cleanup;

	memset(&qp->qp_stats, 0, sizeof(qp->qp_stats));
	return 0;

qp_setup_cleanup:
	if (qp) {
		rte_free(qp);
		qp = NULL;
	}
	return -1;
}

/** Configure stream */
static int
zlib_pmd_stream_create(__rte_unused struct rte_compressdev *dev,
		const struct rte_comp_xform *xform,
		void **zstream)
{
	int ret = 0;
    struct zlib_stream *stream;
	struct zlib_private *internals = dev->data->dev_private;

	if (xform == NULL) {
		ZLIB_LOG_ERR("invalid xform struct");
		return -EINVAL;
	}

	struct rte_mempool *mp = rte_mempool_lookup(internals->mp_name);
	if (rte_mempool_get(mp, (void **)zstream)) {
		ZLIB_LOG_ERR(
				"Couldn't get object from session mempool");
		return -ENOMEM;
	}
    stream = *((struct zlib_stream **)zstream); 

	ret = zlib_set_stream_parameters(xform, stream);

	if (ret < 0) {
		ZLIB_LOG_ERR("failed configure session parameters");

		memset(stream, 0, sizeof(struct zlib_stream));
		/* Return session to mempool */
		rte_mempool_put(mp, stream);
	}

	return 0;
}

/** Configure private xform */
static int
zlib_pmd_private_xform_create(__rte_unused struct rte_compressdev *dev,
		const struct rte_comp_xform *xform,
		void **private_xform)
{
	int ret = 0;

    ret = zlib_pmd_stream_create(dev, xform, private_xform);
	return ret;
}

/** Clear the memory of stream so it doesn't leave key material behind */
static int
zlib_pmd_stream_free(__rte_unused struct rte_compressdev *dev,
		void *zstream)
{
	struct zlib_stream *stream = zstream;
	if (!stream)
		return -EINVAL;

	stream->free(&stream->strm);
	/* Zero out the whole structure */
	memset(stream, 0, sizeof(struct zlib_stream));
	struct rte_mempool *mp = rte_mempool_from_obj(stream);
	rte_mempool_put(mp, stream);

	return 0;
}

/** Clear the memory of stream so it doesn't leave key material behind */
static int
zlib_pmd_private_xform_free(__rte_unused struct rte_compressdev *dev,
		void *private_xform)
{
    zlib_pmd_stream_free(dev, private_xform);
	
	return 0;
}

struct rte_compressdev_ops zlib_pmd_ops = {
		.dev_configure		= zlib_pmd_config,
		.dev_start		= zlib_pmd_start,
		.dev_stop		= zlib_pmd_stop,
		.dev_close		= zlib_pmd_close,
    
		.stats_get		= zlib_pmd_stats_get,
		.stats_reset		= zlib_pmd_stats_reset,

		.dev_infos_get		= zlib_pmd_info_get,

		/* TODO:Check if no q-pair count needed*/
		.queue_pair_setup	= zlib_pmd_qp_setup,
		.queue_pair_release	= zlib_pmd_qp_release,

		.private_xform_create	= zlib_pmd_private_xform_create,
		.private_xform_free		= zlib_pmd_private_xform_free,

		.stream_create	= zlib_pmd_stream_create,
		.stream_free	= zlib_pmd_stream_free
};

struct rte_compressdev_ops *rte_zlib_pmd_ops = &zlib_pmd_ops;
