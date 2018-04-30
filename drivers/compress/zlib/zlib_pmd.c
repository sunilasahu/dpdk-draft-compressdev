/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_byteorder.h>

#include <zlib.h>
#include <math.h>
#include <assert.h>
#include "zlib_pmd_private.h"

static uint8_t compressdev_driver_id;

/** Process deflate operation */
static void
process_zlib_stateful_deflate(struct rte_comp_op *op)
{
	int ret, flush;
	z_stream *strm = &(op->priv_xform->z_stream);
	uint8_t *src, *dst;
	int l, n = srclen;

	for (mbuf_src = op->m_src; mbuf_src != NULL && offset > rte_pktmbuf_data_len(mbuf_src);
			mbuf_src = mbuf_src->next)
		op->src.offset -= rte_pktmbuf_data_len(mbuf_src);

	if (mbuf_src == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *, op->src.offset);

	sl = rte_pktmbuf_data_len(mbuf_src) - offset;
	for (mbuf_dst = op->m_dst; mbuf_dst != NULL && offset > rte_pktmbuf_data_len(mbuf_dst);
			mbuf_dst = mbuf_dst->next)
		op->dst.offset -= rte_pktmbuf_data_len(mbuf_dst);

	if (mbuf_dst == 0)
		return -1;

	dst = rte_pktmbuf_mtod_offset(mbuf_dst, unsigned char *,
							op->dst.offset);

	dl = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;
	if (unlikely(!src || !dst || !strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid source or destination buffers");
		return;
	}

	if (op->src.length <= sl) 
		sl = op->src.length;

	n = op->src.length;

	while(n > 0)
	{
		strm->avail_in = sl;
		strm->next_in = src;
		flush = (n - sl)?Z_NO_FLUSH:Z_FINISH;
		strm->avail_out = dl;
		strm->next_out = dst;

		ret = deflate(strm, flush);
		op->produced += dl - strm->avail_out;
		while(avail_out == 0 && n) {
			dst = mbuf_dst->next;
			if(!dst)
			{
				ZLIB_LOG_ERR("\nOut of memory for output buffer");
				op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE;
				return;
			}
			mbuf_dst = dst;
			dl = rte_pktmbuf_data_len(dst);
			strm->avail_out = dl;
			strm->next_out = dst;
			ret = deflate(strm, flush);
			op->produced += dl - strm->avail_out;
			n -=  stream.avail_in;
		}
		have = dl - strm.avail_out;
		dst += have;
		dl = strm.avail_out;
		op->consumed += op->src.length - stream.avail_in;
		if(stream.avail_in)
			ZLIB_LOG_ERR("\ncomp err");
	}

	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	if (ret == Z_STREAM_END)
		deflateReset(strm);
}

/** Process deflate operation */
static void
process_zlib_deflate(struct rte_comp_op *op)
{
	int ret, flush;
	z_stream *strm = &(op->priv_xform->z_stream);
	uint8_t *src, *dst;

	for (mbuf_src = op->m_src; mbuf_src != NULL && offset > rte_pktmbuf_data_len(mbuf_src);
			mbuf_src = mbuf_src->next)
		op->src.offset -= rte_pktmbuf_data_len(mbuf_src);

	if (mbuf_src == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *, op->src.offset);

	sl = rte_pktmbuf_data_len(mbuf_src) - offset;
	for (mbuf_dst = op->m_dst; mbuf_dst != NULL && offset > rte_pktmbuf_data_len(mbuf_dst);
			mbuf_dst = mbuf_dst->next)
		op->dst.offset -= rte_pktmbuf_data_len(mbuf_dst);

	if (mbuf_dst == 0)
		return -1;

	dst = rte_pktmbuf_mtod_offset(mbuf_dst, unsigned char *,
							op->dst.offset);

	dl = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;
	if (unlikely(!src || !dst || !strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid source or destination buffers");
		return;
	}

	if (op->src.length <= sl) 
		sl = op->src.length;

	n = op->src.length;

	while(n > 0)
	{
		strm->avail_in = sl;
		strm->next_in = src;
		flush = (n - sl)?Z_NO_FLUSH:Z_FINISH;
		strm->avail_out = dl;
		strm->next_out = dst;

		ret = deflate(strm, flush);
		op->produced += dl - strm->avail_out;
		while(avail_out == 0 && n){
			dst = mbuf_dst->next;
			if(!dst)
			{
				ZLIB_LOG_ERR("\nOut of memory for output buffer");
				op->status = RTE_COMP_OP_STATUS_OUT_OF_SPACE;
				return;
			}
			mbuf_dst = dst;
			dl = rte_pktmbuf_data_len(dst);
			strm->avail_out = dl;
			strm->next_out = dst;
			ret = deflate(strm, flush);
			op->produced += dl - strm->avail_out;
			n -=  stream.avail_in;

		}
		have = dl - strm.avail_out;
		dst += have;
		dl = strm.avail_out;
		op->consumed += op->src.length - stream.avail_in;
		if(stream.avail_in)
			ZLIB_LOG_ERR("\ncomp err");

	}
	if (unlikely(Z_STREAM_END != ret )) {
		op->consumed = op->produced = 0;
		op->status = RTE_COMP_OP_STATUS_ERROR;
		deflateReset(strm);
		return;
	}
	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	if (ret == Z_STREAM_END)
		deflateReset(strm);
}

/** Process inflate operation */
static void
process_zlib_inflate(struct rte_comp_op *op)
{
	int ret;
	unsigned int have;
	z_stream *strm;
	unsigned char *src, *dst;

	for (mbuf_src = op->m_src; mbuf_src != NULL && offset > rte_pktmbuf_data_len(mbuf_src);
			mbuf_src = mbuf_src->next)
		op->src.offset -= rte_pktmbuf_data_len(mbuf_src);

	if (mbuf_src == 0)
		return -1;

	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *, op->src.offset);

	sl = rte_pktmbuf_data_len(mbuf_src) - offset;
	for (mbuf_dst = op->m_dst; mbuf_dst != NULL && offset > rte_pktmbuf_data_len(mbuf_dst);
			mbuf_dst = mbuf_dst->next)
		op->dst.offset -= rte_pktmbuf_data_len(mbuf_dst);

	if (mbuf_dst == 0)
		return -1;

	dst = rte_pktmbuf_mtod_offset(mbuf_dst, unsigned char *,
							op->dst.offset);

	dl = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;

	if (unlikely(!src || !dst || !strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid source or destination buffers");
		return;
	}

	flush = Z_NO_FLUSH;
	while (n > 0 && ret != Z_STREAM_END)
	{
		strm->avail_in = sl;
		strm->next_in = src;
		strm->avail_out = dl;
		strm->next_out = dst;

		ret = inflate(strm, flush);
		op->produced += dl - strm->avail_out;
		while(avail_out == 0 && n){
			dst = mbuf_dst->next;
			if(!dst)
			{
				ZLIB_LOG_ERR("\nOut of memory for output buffer");
				inflateReset(strm);
				op->status = RTE_COMP_OP_STATUS_ERROR;
				return;
			}
			mbuf_dst = dst;
			dl = rte_pktmbuf_data_len(dst);
			strm->avail_out = dl;
			strm->next_out = dst;
			ret = inflate(strm, flush);
			op->produced += dl - strm->avail_out;
			n -=  stream.avail_in;

		}
		have = dl - strm.avail_out;
		dst += have;
		dl = strm.avail_out;
		op->consumed += op->src.length - stream.avail_in;
		if(stream.avail_in)
				ZLIB_LOG_ERR("\ncomp err");

	}
	switch (ret) {
	case Z_NEED_DICT:
	case Z_DATA_ERROR:
	case Z_STREAM_ERROR:
	case Z_MEM_ERROR:
		op->status = RTE_COMP_OP_STATUS_ERROR;
		(void)inflateReset(strm);
		return;
	}

	//have = rte_pktmbuf_data_len(op->m_dst) - strm->avail_out;
//	op->consumed += op->src.length - strm->avail_in;

	op->status = RTE_COMP_OP_STATUS_SUCCESS;

	if (ret == Z_STREAM_END)
		inflateReset(strm);

}

/** Process comp operation for mbuf */
static inline int
process_zlib_op(struct zlib_qp *qp, struct rte_comp_op *op,
			struct zlib_session *sess)
{
	if (unlikely(sess == NULL))
		op->status = RTE_COMP_OP_STATUS_INVALID_SESSION;
	else
	switch (op->priv_xform->type) {
	case RTE_COMP_COMPRESS:
		process_zlib_deflate(op);
		break;
	case RTE_COMP_DECOMPRESS:
		process_zlib_inflate(op);
		break;
	}

	/* whatever is out of op, put it into completion queue with
	 * its status
	 */
	return rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

/** Parse comp xform and set private session parameters */
int
zlib_set_privxform_parameters(struct zlib_priv_xform *priv_xform,
		const struct rte_comp_xform *xform)
{
	int strategy, level, wbits;
	z_stream *strm =  &(priv_xform->strm);

	/* allocate deflate state */
	strm->zalloc = Z_NULL;
	strm->zfree = Z_NULL;
	strm->opaque = Z_NULL;

	switch (xform->type) {
	case RTE_COMP_COMPRESS:
		/** Compression window bits */
		switch (xform->compress.algo) {
		case RTE_COMP_DEFLATE:
			wbits = -(sess->xform.compress.window_size);
			break;
		default:
			ZLIB_LOG_ERR("Compression algorithm not supported\n");
			return -1;
		}

		/** Compression Level */
		switch (xform->compress.level) {
		case RTE_COMP_LEVEL_PMD_DEFAULT:
			level = Z_DEFAULT_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_NONE:
			level = Z_NO_COMPRESSION;
			break;
		case RTE_COMP_LEVEL_MIN:
			level = Z_BEST_SPEED;
			break;
		case RTE_COMP_LEVEL_MAX:
			level = Z_BEST_COMPRESSION;
			break;
		default:
			level = sess->xform.compress.level;

			if (level < RTE_COMP_LEVEL_MIN ||
			   level > RTE_COMP_LEVEL_MAX) {
				ZLIB_LOG_ERR("Compression level not supported\n");
				return -1;
			}
		}

		/** Compression strategy */
		switch (xform->compress.deflate.huffman) {
		case RTE_COMP_DEFAULT:
			strategy = Z_DEFAULT_STRATEGY;
			break;
		case RTE_COMP_FIXED:
			strategy = Z_FIXED;
			break;
		case RTE_COMP_DYNAMIC:
			strategy = Z_HUFFMAN_ONLY;
			break;
		default:
			ZLIB_LOG_ERR("Compression strategy not supported\n");
			return -1;
		}
		priv_xform->type = xform->type;
		if (deflateInit2(strm, level,
					Z_DEFLATED, wbits,
					DEF_MEM_LEVEL, strategy) != Z_OK)
			return -1;
	break;
	case RTE_COMP_DECOMPRESS:
		/** window bits */
		switch (xform->decompress.algo) {
		case RTE_COMP_DEFLATE:
			wbits = -(xform->decompress.window_size);
			break;
		default:
			ZLIB_LOG_ERR("Compression algorithm not supported\n");
			return -1;
		}

		if (inflateInit2(strm, wbits) != Z_OK)
			return -1;
	break;
	default:
		return -1;
	}
	return 0;
}

/** Conclude session stream */
void
zlib_clear_session_parameters(struct zlib_session *sess)
{
	if (sess == NULL)
		return;

	switch (sess->xform.type) {
	case RTE_COMP_COMPRESS:
		(void)deflateEnd(&(sess->strm));
		return;
	case RTE_COMP_DECOMPRESS:
		(void)inflateEnd(&(sess->strm));
		return;
	default:
		return;
	}
}

static uint16_t
zlib_pmd_enqueue_burst(void *queue_pair,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct zlib_qp *qp = queue_pair;
	int ret, i;

	for (i = 0; i < nb_ops; i++) {

		ret = process_zlib_op(qp, ops[i]);

		if (unlikely(ret < 0)) {
			/* increment count if failed to push to completion
			 * queue
			 */
			qp->qp_stats.enqueue_err_count++;
		}
	}

	qp->qp_stats.enqueued_count += nb_ops;

	return nb_ops;
}

static uint16_t
zlib_pmd_dequeue_burst(void *queue_pair,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct zlib_qp *qp = queue_pair;

	unsigned int nb_dequeued = 0;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_pkts,
			(void **)ops, nb_ops, NULL);
	qp->qp_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

static int zlib_remove(struct rte_vdev_device *vdev);

static int
zlib_create(const char *name,
		struct rte_vdev_device *vdev,
		struct rte_compressdev_pmd_init_params *init_params)
{
	struct rte_compressdev *dev;
	struct zlib_private *internals;

	dev = rte_compressdev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		ZLIB_LOG_ERR("driver %s: create failed", init_params->name);
		return -ENODEV;
	}

	dev->driver_id = compressdev_driver_id;
	dev->dev_ops = rte_zlib_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = zlib_pmd_dequeue_burst;
	dev->enqueue_burst = zlib_pmd_enqueue_burst;

	dev->feature_flags = 0;
	dev->feature_flags |= RTE_COMP_FF_SHAREABLE_PRIV_XFORM;

	internals = dev->data->dev_private;
	internals->max_nb_queue_pairs = init_params->max_nb_queue_pairs;

	return 0;
}

static int
zlib_probe(struct rte_vdev_device *vdev)
{
	struct rte_compressdev_pmd_init_params init_params = {
		"",
		sizeof(struct zlib_private),
		rte_socket_id(),
		ZLIB_PMD_MAX_NB_QUEUE_PAIRS,
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

static struct compressdev_driver zlib_compress_drv;

RTE_PMD_REGISTER_VDEV(COMPRESSDEV_NAME_ZLIB_PMD, zlib_pmd_drv);
RTE_PMD_REGISTER_ALIAS(COMPRESSDEV_NAME_ZLIB_PMD, compressdev_zlib_pmd);
RTE_PMD_REGISTER_COMPRESSDEV_DRIVER(zlib_compress_drv, zlib_pmd_drv,
		compressdev_driver_id);
