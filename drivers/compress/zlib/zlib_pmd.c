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
#define COMPUTE_DST_BUF(mbuf, dst, dlen, ret) \
        ((mbuf = mbuf->next) ? (dst = rte_pktmbuf_mtod(mbuf, uint8_t *)),dlen =rte_pktmbuf_data_len(mbuf) : \
        !(op->status = ((op->op_type == RTE_COMP_OP_STATELESS) ?\
        RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED : \
        RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE) ) )
#if 0
    ret = (mbuf = mbuf->next) ? (dst = rte_pktmbuf_mtod(mbuf, uint8_t *) && dlen = rte_pktmbuf_data_len(mbuf)) : 
#endif                      
static void 
process_zlib_deflate(struct rte_comp_op *op, z_stream *strm)
{
	int ret, flush;
	uint8_t *src, *dst;
	uint32_t sl, dl, have;
   	struct rte_mbuf *mbuf_src = op->m_src;
    	struct rte_mbuf *mbuf_dst = op->m_dst;

	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *, op->src.offset);

	sl = rte_pktmbuf_data_len(mbuf_src) - op->src.offset;

	dst = rte_pktmbuf_mtod_offset(mbuf_dst, unsigned char *,
			op->dst.offset);

	dl = rte_pktmbuf_data_len(mbuf_dst) - op->dst.offset;

	if (unlikely(!src || !dst || !strm)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid source or destination buffers");
		return;
	}
	if (op->src.length <= sl) {
		sl = op->src.length;
		flush = op->flush_flag;
	} else {
		/* if there're more than one mbufs in input,
		 * process intermediate with NO_FLUSH
		 */
		flush = Z_NO_FLUSH;
	}

	do {
		/* Update z_stream with the inputs provided by application */
		strm->next_in = src;
		strm->avail_in = sl;

		do {
			strm->avail_out = dl;
			strm->next_out = dst;
			ret = deflate(strm, flush);
		    	if(unlikely(ret == Z_STREAM_ERROR)) {
				op->status =  RTE_COMP_OP_STATUS_ERROR;
				/* error return, do not process further */
				return;
		    	}
			/* Update op stats */
			op->produced += dl - strm->avail_out;
			op->consumed += sl - strm->avail_in;
		} while((strm->avail_out == 0) && COMPUTE_DST_BUF(mbuf_dst, dst, dl, ret));
        
		/** Compress till the end of compressed blocks provided 
		 * or till Z_FINISH */
        	if (op->status)
            		return;
		if (ret == Z_STREAM_END || (op->consumed == op->src.length))
			break;

		/** Update last output buffer with respect to availed space */
		have = dl - strm->avail_out;
		dst += have;
		dl = strm->avail_out;
		/** Update source buffer to next mbuf*/
		mbuf_src = mbuf_src->next;
		src = rte_pktmbuf_mtod(mbuf_src, uint8_t *);
		sl = rte_pktmbuf_data_len(mbuf_src);

		/** Last block to be compressed, flush value needs to be updated
		 * Update flush with value provided by app for last block,
		 * For stateless flush should be always Z_FINISH */

		if ((op->src.length - op->consumed) <= sl)
		{
			sl = (op->src.length - op->consumed);
			flush = op->flush_flag;
		}

	} while(1);

	return;
}

static void
process_zlib_inflate(struct rte_comp_op *op, z_stream *strm)
{
	int ret, flush;
	uint8_t *src, *dst;
	uint32_t sl, dl, have;
    	struct rte_mbuf *mbuf_src = op->m_src;
    	struct rte_mbuf *mbuf_dst = op->m_dst;

	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *, op->src.offset);

	sl = rte_pktmbuf_data_len(mbuf_src) - op->src.offset;

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

	/** Ignoring flush value provided from application for decompression */
	flush = Z_NO_FLUSH;
	/* initialize status to SUCCESS */
	op->status = RTE_COMP_OP_STATUS_SUCCESS;
	do {
		/** Update z_stream with the inputs provided by application */
		strm->avail_in = sl;
		strm->avail_out = dl;
		do {
			strm->avail_out = dl;
			strm->next_out = dst;
			ret = inflate(strm, flush);
           
		   	switch (ret) {
		        case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
		    	case Z_DATA_ERROR:
		    	case Z_MEM_ERROR:
		    	case Z_STREAM_ERROR:
				op->status = RTE_COMP_OP_STATUS_ERROR;
				return;
		    	}
			/** Update op stats */
			op->produced += dl - strm->avail_out;
			op->consumed += sl - strm->avail_in;
		} while((strm->avail_out == 0) && COMPUTE_DST_BUF(mbuf_dst, dst, dl, ret));
        
		/* if op->status not SUCCESS, return */
		if (op->status != RTE_COMP_STATUS_SUCCESS)
			return;
		
		/** Compress till the end of compressed blocks provided 
		 * or till Z_STREAM_END */
        	if (ret == Z_STREAM_END || (op->consumed == op->src.length))
			break;

		/** Adjust previous output buffer with respect to avail_out */
		have = dl - strm->avail_out;
		dst += have;
		dl = strm->avail_out;
		/** Read next input buffer to be processed */
		mbuf_src = mbuf_src->next;
		src = rte_pktmbuf_mtod(mbuf_src, uint8_t *);
		sl = rte_pktmbuf_data_len(mbuf_src);
		sl = (op->src.length - op->consumed) <= sl ? (op->src.length - op->consumed) : sl;

	} while (1);

	return;
}

/** Process comp operation for mbuf */
static inline int
process_zlib_op(struct zlib_qp *qp, struct rte_comp_op *op)
{
	struct zlib_stream *stream ;

    if( op->src.offset > rte_pktmbuf_data_len(op->m_src) || op->dst.offset > rte_pktmbuf_data_len(op->m_dst)) {
		op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid source or destination buffers");
		goto comp_err;
	}
        
    if (op->op_type == RTE_COMP_OP_STATELESS)
        stream = &((struct zlib_priv_xform *)op->private_xform)->stream;
    else if(op->op_type == RTE_COMP_OP_STATEFUL)
        stream = (struct zlib_stream *)op->stream;
    else {
        op->status = RTE_COMP_OP_STATUS_INVALID_ARGS;
		ZLIB_LOG_ERR("\nInvalid operation type");
        goto comp_err;
    }
	stream->comp(op, &stream->strm);
comp_err:
	/* whatever is out of op, put it into completion queue with
	 * its status
	 */
	return rte_ring_enqueue(qp->processed_pkts, (void *)op);
}

/** Parse comp xform and set private xform/Stream parameters */
int
zlib_set_stream_parameters(const struct rte_comp_xform *xform, struct zlib_stream *stream)
{
	int strategy, level, wbits;
    z_stream *strm = &stream->strm;

	/* allocate deflate state */
	strm->zalloc = Z_NULL;
	strm->zfree = Z_NULL;
	strm->opaque = Z_NULL;

	switch (xform->type) {
		case RTE_COMP_COMPRESS:

			stream->comp = process_zlib_deflate;
			stream->free = deflateEnd;
			/** Compression window bits */
			switch (xform->compress.algo) {
				case RTE_COMP_ALGO_DEFLATE:
					wbits = -(xform->compress.window_size);
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
					level = xform->compress.level;

					if (level < RTE_COMP_LEVEL_MIN ||
							level > RTE_COMP_LEVEL_MAX) {
						ZLIB_LOG_ERR("Compression level not supported\n");
						return -1;
					}
			}

			/** Compression strategy */
			switch (xform->compress.deflate.huffman) {
				case RTE_COMP_HUFFMAN_DEFAULT:
					strategy = Z_DEFAULT_STRATEGY;
					break;
				case RTE_COMP_HUFFMAN_FIXED:
					strategy = Z_FIXED;
					break;
				case RTE_COMP_HUFFMAN_DYNAMIC:
					strategy = Z_HUFFMAN_ONLY;
					break;
				default:
					ZLIB_LOG_ERR("Compression strategy not supported\n");
					return -1;
			}
			if (deflateInit2(strm, level,
						Z_DEFLATED, wbits,
						DEF_MEM_LEVEL, strategy) != Z_OK)
				return -1;
			break;
		case RTE_COMP_DECOMPRESS:

			stream->comp = process_zlib_inflate;
			stream->free = inflateEnd;
			/** window bits */
			switch (xform->decompress.algo) {
				case RTE_COMP_ALGO_DEFLATE:
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

static uint16_t
zlib_pmd_enqueue_burst(void *queue_pair,
			struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct zlib_qp *qp = queue_pair;
	int ret, i;
	int enqd = 0;
	for (i = 0; i < nb_ops; i++) {
		ret = process_zlib_op(qp, ops[i]);
		if (unlikely(ret < 0)) {
			/* increment count if failed to push to completion
			 * queue
			 */
			qp->qp_stats.enqueue_err_count++;
        	} else {
            	qp->qp_stats.enqueued_count ++;
		enqd++;
        	}
	}
	return enqd;
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

	dev = rte_compressdev_pmd_create(name, &vdev->device,
			sizeof(struct zlib_private), init_params);
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
	dev->feature_flags |= RTE_COMP_FF_SHAREABLE_PRIV_XFORM |
			      RTE_COMP_FF_NONCOMPRESSED_BLOCKS |
			      RTE_COMP_FF_ADLER32_CHECKSUM |
			      RTE_COMP_FF_MBUF_SCATTER_GATHER;

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
