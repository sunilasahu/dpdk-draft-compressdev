/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _RTE_COMPRESSDEV_H_
#define _RTE_COMPRESSDEV_H_

/**
 * @file rte_compressdev.h
 *
 * RTE Compression Device APIs
 *
 * Defines comp device APIs for the provisioning of compression operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_kvargs.h"
#include "rte_comp.h"
#include "rte_dev.h"
#include <rte_common.h>

extern const char **rte_cyptodev_names;

/* Logging Macros */
extern int compressdev_logtype;
#define COMPRESSDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, compressdev_logtype, "%s(): "fmt "\n", \
			__func__, ##args)

#define RTE_COMPRESSDEV_DETACHED  (0)
#define RTE_COMPRESSDEV_ATTACHED  (1)

#define RTE_COMPRESSDEV_NAME_MAX_LEN	(64)

/**< Max length of name of comp PMD */
/**
 * A macro that points to an offset from the start
 * of the comp operation structure (rte_comp_op)
 *
 * The returned pointer is cast to type t.
 *
 * @param c
 *   The comp operation
 * @param o
 *   The offset from the start of the comp operation
 * @param t
 *   The type to cast the result into
 */
#define rte_comp_op_ctod_offset(c, t, o)	\
	((t)((char *)(c) + (o)))

/**
 * A macro that returns the physical address that points
 * to an offset from the start of the comp operation
 * (rte_comp_op).
 *
 * @param c
 *   The comp operation
 * @param o
 *   The offset from the start of the comp operation
 *   to calculate address from
 */
#define rte_comp_op_ctophys_offset(c, o)	\
	(rte_iova_t)((c)->phys_addr + (o))

/**
 * Parameter log base 2 range description.
 * Final value will be 2^value.
 */
struct rte_param_log2_range {
	uint8_t min;	/**< Minimum log2 value */
	uint8_t max;	/**< Maximum log2 value */
	uint8_t increment;
	/**< If a range of sizes are supported,
	 * this parameter is used to indicate
	 * increments in log base 2 byte value
	 * that are supported between the minimum and maximum
	 */
};

/** Structure used to capture a capability of a comp device */
struct rte_compressdev_capabilities {
	enum rte_comp_algorithm algo;
	/* Compression algorithm */
	uint64_t comp_feature_flags;
	/**< Bitmask of flags for compression service features */
	struct rte_param_log2_range window_size;
	/**< Window size range in bytes */
};


/** Macro used at end of comp PMD list */
#define RTE_COMP_END_OF_CAPABILITIES_LIST() \
	{ RTE_COMP_ALGO_LIST_END }

/**
 * compression device supported feature flags
 *
 * @note New features flags should be added to the end of the list
 *
 * Keep these flags synchronised with rte_compressdev_get_feature_name()
 */

#define	RTE_COMPDEV_FF_HW_ACCELERATED		(1ULL << 0)
/**< Operations are off-loaded to an external hardware accelerator */
#define	RTE_COMPDEV_FF_CPU_SSE			(1ULL << 1)
/**< Utilises CPU SIMD SSE instructions */
#define	RTE_COMPDEV_FF_CPU_AVX			(1ULL << 2)
/**< Utilises CPU SIMD AVX instructions */
#define	RTE_COMPDEV_FF_CPU_AVX2			(1ULL << 3)
/**< Utilises CPU SIMD AVX2 instructions */
#define	RTE_COMPDEV_FF_CPU_AVX512		(1ULL << 4)
/**< Utilises CPU SIMD AVX512 instructions */
#define	RTE_COMPDEV_FF_CPU_NEON			(1ULL << 5)
/**< Utilises CPU NEON instructions */

/**
 * compression service feature flags
 *
 * @note New features flags should be added to the end of the list
 *
 * Keep these flags synchronised with rte_comp_get_feature_name()
 */
#define RTE_COMP_FF_STATEFUL_COMPRESSION	(1ULL << 0)
/**< Stateful compression is supported */
#define RTE_COMP_FF_STATEFUL_DECOMPRESSION	(1ULL << 1)
/**< Stateful decompression is supported */
#define	RTE_COMP_FF_MBUF_SCATTER_GATHER		(1ULL << 2)
/**< Scatter-gather mbufs are supported */
#define RTE_COMP_FF_ADLER32_CHECKSUM		(1ULL << 3)
/**< Adler-32 Checksum is supported */
#define RTE_COMP_FF_CRC32_CHECKSUM		(1ULL << 4)
/**< CRC32 Checksum is supported */
#define RTE_COMP_FF_CRC32_ADLER32_CHECKSUM	(1ULL << 5)
/**< Adler-32/CRC32 Checksum is supported */
#define RTE_COMP_FF_MULTI_PKT_CHECKSUM		(1ULL << 6)
/**< Generation of checksum across multiple stateless packets is supported */
#define RTE_COMP_FF_NONCOMPRESSED_BLOCKS	(1ULL << 7)
/**< Creation of non-compressed blocks using RTE_COMP_LEVEL_NONE is supported */

/**
 * Get the name of a compress device feature flag.
 *
 * @param flag
 *   The mask describing the flag
 *
 * @return
 *   The name of this flag, or NULL if it's not a valid feature flag.
 */
const char * __rte_experimental
rte_compressdev_get_feature_name(uint64_t flag);

/**
 * Get the name of a compress service feature flag
 *
 * @param flag
 *   The mask describing the flag
 *
 * @return
 *   The name of this flag, or NULL if it's not a valid feature flag.
 */
const char * __rte_experimental
rte_comp_get_feature_name(uint64_t flag);

/**  comp device information */
struct rte_compressdev_info {
	const char *driver_name;		/**< Driver name. */
	uint8_t driver_id;			/**< Driver identifier */
	uint64_t feature_flags;			/**< Feature flags */
	const struct rte_compressdev_capabilities *capabilities;
	/**< Array of devices supported capabilities */
	unsigned int max_nb_queue_pairs;
	/**< Maximum number of queues pairs supported by device. */
};


/** comp device statistics */
struct rte_compressdev_stats {
	uint64_t enqueued_count;
	/**< Count of all operations enqueued */
	uint64_t dequeued_count;
	/**< Count of all operations dequeued */

	uint64_t enqueue_err_count;
	/**< Total error count on operations enqueued */
	uint64_t dequeue_err_count;
	/**< Total error count on operations dequeued */
};


/**
 * Get the device identifier for the named compress device.
 *
 * @param name
 *   Device name to select the device structure
 * @return
 *   - Returns compress device identifier on success.
 *   - Return -1 on failure to find named compress device.
 */
int __rte_experimental
rte_compressdev_get_dev_id(const char *name);

/**
 * Get the compress device name given a device identifier.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - Returns compress device name.
 *   - Returns NULL if compress device is not present.
 */
const char * __rte_experimental
rte_compressdev_name_get(uint8_t dev_id);

/**
 * Get the total number of compress devices that have been successfully
 * initialised.
 *
 * @return
 *   - The total number of usable compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_count(void);

/**
 * Get number of comp device defined type.
 *
 * @param driver_id
 *   Driver identifier
 * @return
 *   Returns number of comp device.
 */
uint8_t __rte_experimental
rte_compressdev_device_count_by_driver(uint8_t driver_id);

/**
 * Get number and identifiers of attached comp devices that
 * use the same compress driver.
 *
 * @param driver_name
 *   Driver name
 * @param devices
 *   Output devices identifiers
 * @param nb_devices
 *   Maximal number of devices
 *
 * @return
 *   Returns number of attached compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
		uint8_t nb_devices);
/*
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 if returned is the dev_id value is out of range.
 */
int __rte_experimental
rte_compressdev_socket_id(uint8_t dev_id);

/** Compress device configuration structure */
struct rte_compressdev_config {
	int socket_id;
	/**< Socket on which to allocate resources */
	uint16_t nb_queue_pairs;
	/**< Total number of queue pairs to configure on a device */
};

/**
 * Configure a device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param dev_id
 *   Compress device identifier
 * @param config
 *   The compress device configuration
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int __rte_experimental
rte_compressdev_configure(uint8_t dev_id,
			struct rte_compressdev_config *config);

/**
 * Start an device.
 *
 * The device start step is the last one and consists of setting the configured
 * offload features and in starting the transmit and the receive units of the
 * device.
 * On success, all basic functions exported by the API (link status,
 * receive/transmit, and so on) can be invoked.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - 0: Success, device started.
 *   - <0: Error code of the driver device start function.
 */
int __rte_experimental
rte_compressdev_start(uint8_t dev_id);

/**
 * Stop an device. The device can be restarted with a call to
 * rte_compressdev_start()
 *
 * @param dev_id
 *   Compress device identifier
 */
void __rte_experimental
rte_compressdev_stop(uint8_t dev_id);

/**
 * Close an device. The device cannot be restarted!
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 */
int __rte_experimental
rte_compressdev_close(uint8_t dev_id);

/**
 * Allocate and set up a receive queue pair for a device.
 *
 *
 * @param dev_id
 *   Compress device identifier
 * @param queue_pair_id
 *   The index of the queue pairs to set up. The
 *   value must be in the range [0, nb_queue_pair - 1]
 *   previously supplied to rte_compressdev_configure()
 * @param max_inflight_ops
 *   Max number of ops which the qp will have to
 *   accommodate simultaneously
 * @param socket_id
 *   The *socket_id* argument is the socket identifier
 *   in case of NUMA. The value can be *SOCKET_ID_ANY*
 *   if there is no NUMA constraint for the DMA memory
 *   allocated for the receive queue pair
 * @return
 *   - 0: Success, queue pair correctly set up.
 *   - <0: Queue pair configuration failed
 */
int __rte_experimental
rte_compressdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		uint32_t max_inflight_ops, int socket_id);

/**
 * Get the number of queue pairs on a specific comp device
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - The number of configured queue pairs.
 */
uint16_t __rte_experimental
rte_compressdev_queue_pair_count(uint8_t dev_id);


/**
 * Retrieve the general I/O statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device
 * @param stats
 *   A pointer to a structure of type
 *   *rte_compressdev_stats* to be filled with the
 *   values of device counters
 * @return
 *   - Zero if successful.
 *   - Non-zero otherwise.
 */
int __rte_experimental
rte_compressdev_stats_get(uint8_t dev_id, struct rte_compressdev_stats *stats);

/**
 * Reset the general I/O statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device.
 */
void __rte_experimental
rte_compressdev_stats_reset(uint8_t dev_id);

/**
 * Retrieve the contextual information of a device.
 *
 * @param dev_id
 *   Compress device identifier
 * @param dev_info
 *   A pointer to a structure of type *rte_compressdev_info*
 *   to be filled with the contextual information of the device
 *
 * @note The capabilities field of dev_info is set to point to the first
 * element of an array of struct rte_compressdev_capabilities.
 * The element after the last valid element has it's op field set to
 * RTE_COMP_ALGO_LIST_END.
 */
void __rte_experimental
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info);


typedef uint16_t (*compress_dequeue_pkt_burst_t)(void *qp,
		struct rte_comp_op **ops, uint16_t nb_ops);
/**< Dequeue processed packets from queue pair of a device. */

typedef uint16_t (*compress_enqueue_pkt_burst_t)(void *qp,
		struct rte_comp_op **ops, uint16_t nb_ops);
/**< Enqueue packets for processing on queue pair of a device. */


/** The data structure associated with each comp device. */
struct rte_compressdev {
	compress_dequeue_pkt_burst_t dequeue_burst;
	/**< Pointer to PMD receive function */
	compress_enqueue_pkt_burst_t enqueue_burst;
	/**< Pointer to PMD transmit function */

	struct rte_compressdev_data *data;
	/**< Pointer to device data */
	struct rte_compressdev_ops *dev_ops;
	/**< Functions exported by PMD */
	uint64_t feature_flags;
	/**< Supported features */
	struct rte_device *device;
	/**< Backing device */

	uint8_t driver_id;
	/**< comp driver identifier*/

	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */
} __rte_cache_aligned;


/**
 *
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_compressdev_data {
	uint8_t dev_id;
	/**< Compress device identifier */
	uint8_t socket_id;
	/**< Socket identifier where memory is allocated */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	__extension__
	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs */

	void *dev_private;
	/**< PMD-specific private data */
} __rte_cache_aligned;

struct rte_compressdev *rte_compressdevs;
/**
 *
 * Dequeue a burst of processed compression operations from a queue on the comp
 * device. The dequeued operation are stored in *rte_comp_op* structures
 * whose pointers are supplied in the *ops* array.
 *
 * The rte_compressdev_dequeue_burst() function returns the number of ops
 * actually dequeued, which is the number of *rte_comp_op* data structures
 * effectively supplied into the *ops* array.
 *
 * A return value equal to *nb_ops* indicates that the queue contained
 * at least *nb_ops* operations, and this is likely to signify that other
 * processed operations remain in the devices output queue. Applications
 * implementing a "retrieve as many processed operations as possible" policy
 * can check this specific case and keep invoking the
 * rte_compressdev_dequeue_burst() function until a value less than
 * *nb_ops* is returned.
 *
 * The rte_compressdev_dequeue_burst() function does not provide any error
 * notification to avoid the corresponding overhead.
 *
 * Note: operation ordering is not maintained within the queue pair.
 *
 * @param dev_id
 *   Compress device identifier
 * @param qp_id
 *   The index of the queue pair from which to retrieve
 *   processed operations. The value must be in the range
 *   [0, nb_queue_pair - 1] previously supplied to
 *   rte_compressdev_configure()
 * @param ops
 *   The address of an array of pointers to
 *   *rte_comp_op* structures that must be
 *   large enough to store *nb_ops* pointers in it
 * @param nb_ops
 *   The maximum number of operations to dequeue
 * @return
 *   - The number of operations actually dequeued, which is the number
 *   of pointers to *rte_comp_op* structures effectively supplied to the
 *   *ops* array.
 */
static inline uint16_t
rte_compressdev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct rte_compressdev *dev = &rte_compressdevs[dev_id];

	nb_ops = (*dev->dequeue_burst)
			(dev->data->queue_pairs[qp_id], ops, nb_ops);

	return nb_ops;
}

/**
 * Enqueue a burst of operations for processing on a compression device.
 *
 * The rte_compressdev_enqueue_burst() function is invoked to place
 * comp operations on the queue *qp_id* of the device designated by
 * its *dev_id*.
 *
 * The *nb_ops* parameter is the number of operations to process which are
 * supplied in the *ops* array of *rte_comp_op* structures.
 *
 * The rte_compressdev_enqueue_burst() function returns the number of
 * operations it actually enqueued for processing. A return value equal to
 * *nb_ops* means that all packets have been enqueued.
 *
 * @note All compression operations are Out-of-place (OOP) operations,
 * as the size of the output data is different to the size of the input data.
 *
 * @note The flush flag only applies to operations which return SUCCESS.
 * In OUT_OF_SPACE case whether STATEFUL or STATELESS, data in dest buffer
 * is as if flush flag was FLUSH_NONE.
 * @note flush flag only applies in compression direction. It has no meaning
 * for decompression.
 * @note: operation ordering is not maintained within the queue pair.
 *
 * @param dev_id
 *   Compress device identifier
 * @param qp_id
 *   The index of the queue pair on which operations
 *   are to be enqueued for processing. The value
 *   must be in the range [0, nb_queue_pairs - 1]
 *   previously supplied to *rte_compressdev_configure*
 * @param ops
 *   The address of an array of *nb_ops* pointers
 *   to *rte_comp_op* structures which contain
 *   the operations to be processed
 * @param nb_ops
 *   The number of operations to process
 * @return
 *   The number of operations actually enqueued on the device. The return
 *   value can be less than the value of the *nb_ops* parameter when the
 *   comp devices queue is full or if invalid parameters are specified in
 *   a *rte_comp_op*.
 */
static inline uint16_t
rte_compressdev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops)
{
	struct rte_compressdev *dev = &rte_compressdevs[dev_id];

	return (*dev->enqueue_burst)(
			dev->data->queue_pairs[qp_id], ops, nb_ops);
}


/** Compressdev session */
struct rte_comp_session {
	__extension__ void *sess_private_data[0];
	/**< Private session material */
};


/**
 * Create compression session header (generic with no private data)
 *
 * @param mempool
 *   Symmetric session mempool to allocate session objects from
 * @return
 *  - On success return pointer to compression session
 *  - On failure returns NULL
 */
struct rte_comp_session * __rte_experimental
rte_compressdev_session_create(struct rte_mempool *mempool);

/**
 * Frees comp session header, after checking that all
 * the device private data has been freed, returning it
 * to its original mempool.
 *
 * @param sess
 *   Session header to be freed
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if session is NULL.
 *  - -EBUSY if not all device private data has been freed.
 */
int __rte_experimental
rte_compressdev_session_terminate(struct rte_comp_session *sess);

/**
 * Fill out private session data for the device, based on its device id.
 * The same private session data is shared by all devices exposed by a driver
 * so this only needs to be called for one device per driver-type.
 * All private data stored must be shareable across devices, so read-only.
 * A session initialised for more than one device (of different driver types)
 * must used the same xform for each init.
 *
 * @param dev_id
 *   Compress device identifier
 * @param sess
 *   Session where the private data will be attached to
 * @param xforms
 *   Compress transform operations to apply on flow
 *   processed with this session
 * @param mempool
 *   Mempool from where the private data should be allocated
 *
 * @return
 *  - On success, zero.
 *  - -EINVAL if input parameters are invalid.
 *  - -ENOTSUP if comp device does not support the comp transform.
 *  - -ENOMEM if the private session could not be allocated.
 */
int __rte_experimental
rte_compressdev_session_init(uint8_t dev_id,
			struct rte_comp_session *sess,
			const struct rte_comp_xform *xforms,
			struct rte_mempool *mempool);

/**
 * Frees private data for the device id, based on its device type,
 * returning it to its mempool.
 *
 * @param dev_id
 *   Compress device identifier
 * @param sess
 *   Session containing the reference to the private data
 *
 * @return
 *  - 0 if successful.
 *  - -EINVAL if device is invalid or session is NULL.
 */
int __rte_experimental
rte_compressdev_session_clear(uint8_t dev_id,
			struct rte_comp_session *sess);

/**
 * Get the size of the header session, for all registered drivers.
 *
 * @return
 *   Size of the header session.
 */
unsigned int __rte_experimental
rte_compressdev_get_header_session_size(void);

/**
 * Get the size of the private session data for a device.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @return
 *   - Size of the private data, if successful
 *   - 0 if device is invalid or does not have private session
 */
unsigned int __rte_experimental
rte_compressdev_get_private_session_size(uint8_t dev_id);

/**
 * This should alloc a stream from the device's mempool and initialise it.
 * This handle will be passed to the PMD with every op in the stream.
 *
 * @param dev_id
 *   Compress device identifier
 * @param session
 *   Session previously allocated by
 *   *rte_compressdev_session_create*
 * @param stream
 *   Pointer to PMD's private stream data
 * @param op_type
 *   Op type for which the stream will be used
 *
 * @return
 *
 * TODO: Should qp_id also be added, with constraint that all ops in the same
 * stream should be sent to the same qp?
 *
 */
int __rte_experimental
rte_comp_stream_create(uint8_t dev_id, struct rte_comp_session *sess,
			void **stream, enum rte_comp_op_type op_type);

/**
 * This should clear the stream and return it to the device's mempool.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @param stream
 *   PMD's private stream data
 *
 *
 * @return
 */
int __rte_experimental
rte_comp_stream_free(uint8_t dev_id, void *stream);

/**
 * Provide driver identifier.
 *
 * @param name
 *   Compress driver name
 * @return
 *  The driver type identifier or -1 if no driver found
 */
int __rte_experimental
rte_compressdev_driver_id_get(const char *name);

/**
 * Provide driver name.
 *
 * @param driver_id
 *   The driver identifier
 * @return
 *  The driver name or null if no driver found
 */
const char * __rte_experimental
rte_compressdev_driver_name_get(uint8_t driver_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_COMPRESSDEV_H_ */
