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

#ifndef _RTE_ZLIB_PMD_PRIVATE_H_
#define _RTE_ZLIB_PMD_PRIVATE_H_

#include <zlib.h>
#include <rte_comp.h>
#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#define COMPRESSDEV_NAME_ZLIB_PMD	compress_zlib
/**< ZLIB PMD device name */

#define ZLIB_PMD_MAX_NB_QUEUE_PAIRS	0
/**< ZLIB PMD specified queue pairs */

#define DEF_MEM_LEVEL			8

#define ZLIB_LOG_ERR(fmt, args...) \
	RTE_LOG(ERR, COMPRESSDEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(COMPRESSDEV_NAME_ZLIB_PMD), \
			__func__, __LINE__, ## args)

#define ZLIB_LOG_INFO(fmt, args...) \
	RTE_LOG(INFO, COMPRESSDEV, "[%s] %s() line %u: " fmt "\n",  \
			RTE_STR(COMPRESSDEV_NAME_ZLIB_PMD), \
			__func__, __LINE__, ## args)

struct zlib_private {
	uint32_t max_nb_queue_pairs;
	char mp_name[RTE_MEMPOOL_NAMESIZE];
};

struct zlib_qp {
	struct rte_ring *processed_pkts;
	/**< Ring for placing process packets */
	struct rte_compressdev_stats qp_stats;
	/**< Queue pair statistics */
	uint16_t id;
	/**< Queue Pair Identifier */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/**< Unique Queue Pair Name */
} __rte_cache_aligned;

/* Algorithm handler function prototype */
typedef void (*comp_func_t)(struct rte_comp_op *op, z_stream *strm);

typedef int (*comp_free_t)(z_stream *strm);

/** ZLIB Stream structure */
struct zlib_stream {
	z_stream strm;
	/**< zlib stream structure */
	comp_func_t comp;
	/**< Operation (compression/decompression) */
	comp_free_t free;
	/**< Free Operation (compression/decompression) */
} __rte_cache_aligned;

/** ZLIB private xform structure */
struct zlib_priv_xform {
    struct zlib_stream stream;    
} __rte_cache_aligned;

/** Set ZLIB compression private-xform/Stream parameters */
extern int
zlib_set_stream_parameters(const struct rte_comp_xform *xform, struct zlib_stream *stream); 

/** Device specific operations function pointer structure */
extern struct rte_compressdev_ops *rte_zlib_pmd_ops;

#endif /* _RTE_ZLIB_PMD_PRIVATE_H_ */
