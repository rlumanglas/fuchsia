// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: DO NOT EDIT THIS FILE!
// It is automatically generated by //src/graphics/lib/magma/include/magma_abi/magma_h_gen.py

// clang-format off

#ifndef SRC_GRAPHICS_LIB_MAGMA_INCLUDE_MAGMA_ABI_MAGMA_H_
#define SRC_GRAPHICS_LIB_MAGMA_INCLUDE_MAGMA_ABI_MAGMA_H_

#include "magma_common_defs.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

///
/// \brief Releases the given connection.
/// \param connection An open connection.
///
void magma_release_connection(
    magma_connection_t connection);

///
/// \brief Returns the first recorded error since the last time this function was called, and clears
///        the recorded error. Incurs a round-trip to the system driver
/// \param connection An open connection.
///
magma_status_t magma_get_error(
    magma_connection_t connection);

///
/// \brief Creates a context on the given connection.
/// \param connection An open connection.
/// \param context_id_out The returned context id.
///
void magma_create_context(
    magma_connection_t connection,
    uint32_t* context_id_out);

///
/// \brief Releases the context associated with the given id.
/// \param connection An open connection.
/// \param context_id A valid context id.
///
void magma_release_context(
    magma_connection_t connection,
    uint32_t context_id);

///
/// \brief Creates a memory buffer of at least the given size.
/// \param connection An open connection.
/// \param size Requested buffer size.
/// \param size_out The returned buffer's actual size.
/// \param buffer_out The returned buffer.
///
magma_status_t magma_create_buffer(
    magma_connection_t connection,
    uint64_t size,
    uint64_t* size_out,
    magma_buffer_t* buffer_out);

///
/// \brief Releases the given memory buffer.
/// \param connection An open connection.
/// \param buffer A valid buffer.
///
void magma_release_buffer(
    magma_connection_t connection,
    magma_buffer_t buffer);

///
/// \brief Returns a unique id for the given buffer. For performance reasons it's recommended to
///        cache the id rather than call this repeatedly.
/// \param buffer A valid buffer.
///
uint64_t magma_get_buffer_id(
    magma_buffer_t buffer);

///
/// \brief Returns the actual size of the given buffer.
/// \param buffer A valid buffer.
///
uint64_t magma_get_buffer_size(
    magma_buffer_t buffer);

///
/// \brief Cleans, and optionally invalidates, the cache for the region of memory specified by the
///        given buffer, offset, and size, and write the contents to ram.
/// \param buffer A valid buffer.
/// \param offset An offset into the buffer. Must be less than or equal to the buffer's size.
/// \param size Size of region to be cleaned. Offset + size must be less than or equal to the
///        buffer's size.
/// \param operation One of MAGMA_CACHE_OPERATION_CLEAN or MAGMA_CACHE_OPERATION_CLEAN_INVALIDATE.
///
magma_status_t magma_clean_cache(
    magma_buffer_t buffer,
    uint64_t offset,
    uint64_t size,
    magma_cache_operation_t operation);

///
/// \brief Configures the cache for the given buffer.
/// \param buffer A valid buffer.
/// \param policy One of MAGMA_CACHE_POLICY_[CACHED|WRITE_COMBINING|UNCACHED].
///
magma_status_t magma_set_cache_policy(
    magma_buffer_t buffer,
    magma_cache_policy_t policy);

///
/// \brief Queries the cache policy for a buffer.
/// \param buffer A valid buffer.
/// \param cache_policy_out The returned cache policy.
///
magma_status_t magma_get_buffer_cache_policy(
    magma_buffer_t buffer,
    magma_cache_policy_t* cache_policy_out);

///
/// \brief Maps a number of pages from the given buffer onto the GPU in the connection's address
///        space at the given address. Depending on the MSD this may automatically commit and
///        populate that range.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param page_offset Offset into the buffer in pages.
/// \param page_count Number of pages to map.
/// \param gpu_va Destination GPU virtual address for the mapping.
/// \param map_flags A valid MAGMA_GPU_MAP_FLAGS value.
///
void magma_map_buffer_gpu(
    magma_connection_t connection,
    magma_buffer_t buffer,
    uint64_t page_offset,
    uint64_t page_count,
    uint64_t gpu_va,
    uint64_t map_flags);

///
/// \brief Releases the mapping at the given address from the GPU.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param gpu_va A GPU virtual address associated with an existing mapping of the given buffer.
///
void magma_unmap_buffer_gpu(
    magma_connection_t connection,
    magma_buffer_t buffer,
    uint64_t gpu_va);

///
/// \brief Deprecated. Ensures that the given page range of a buffer is backed by physical memory.
///        If the buffer is mapped, also ensures that the CPU page tables are populated to avoid
///        unnecessary page faults when supported by the platform.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param page_offset Page offset into the buffer.
/// \param page_count Number of pages to commit.
///
magma_status_t magma_commit_buffer(
    magma_connection_t connection,
    magma_buffer_t buffer,
    uint64_t page_offset,
    uint64_t page_count);

///
/// \brief Exports the given buffer, returning a handle that may be imported into another
///        connection.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param buffer_handle_out The returned handle.
///
magma_status_t magma_export(
    magma_connection_t connection,
    magma_buffer_t buffer,
    magma_handle_t* buffer_handle_out);

///
/// \brief Imports and takes ownership of the buffer referred to by the given handle.
/// \param connection An open connection.
/// \param buffer_handle A valid handle.
/// \param buffer_out The returned buffer.
///
magma_status_t magma_import(
    magma_connection_t connection,
    magma_handle_t buffer_handle,
    magma_buffer_t* buffer_out);

///
/// \brief Submits a command buffer for execution on the GPU, with associated resources.
/// \param connection An open connection.
/// \param context_id A valid context ID.
/// \param command_buffer A pointer to the command buffer to execute.
/// \param resources An array of |command_buffer->resource_count| resources associated with the
///        command buffer.
/// \param semaphore_ids An array of semaphore ids; first should be
///        |command_buffer->wait_semaphore_count| wait semaphores followed by
///        |command_buffer->signal_signal_semaphores| signal semaphores.
///
void magma_execute_command_buffer_with_resources(
    magma_connection_t connection,
    uint32_t context_id,
    struct magma_system_command_buffer* command_buffer,
    struct magma_system_exec_resource* resources,
    uint64_t* semaphore_ids);

///
/// \brief Submits a series of commands for execution on the GPU without using a command buffer.
/// \param connection An open connection.
/// \param context_id A valid context ID.
/// \param command_count The number of commands in the provided buffer.
/// \param command_buffers An array of command_count magma_inline_command_buffer structs.
///
void magma_execute_immediate_commands2(
    magma_connection_t connection,
    uint32_t context_id,
    uint64_t command_count,
    struct magma_inline_command_buffer* command_buffers);

///
/// \brief Creates a semaphore.
/// \param connection An open connection.
/// \param semaphore_out The returned semaphore.
///
magma_status_t magma_create_semaphore(
    magma_connection_t connection,
    magma_semaphore_t* semaphore_out);

///
/// \brief Releases the given semaphore.
/// \param connection An open connection.
/// \param semaphore A valid semaphore.
///
void magma_release_semaphore(
    magma_connection_t connection,
    magma_semaphore_t semaphore);

///
/// \brief Returns a unique id for the given semaphore.
/// \param semaphore A valid semaphore.
///
uint64_t magma_get_semaphore_id(
    magma_semaphore_t semaphore);

///
/// \brief Signals the given semaphore.
/// \param semaphore A valid semaphore.
///
void magma_signal_semaphore(
    magma_semaphore_t semaphore);

///
/// \brief Resets the given semaphore.
/// \param semaphore A valid semaphore.
///
void magma_reset_semaphore(
    magma_semaphore_t semaphore);

///
/// \brief Exports the given semaphore, returning a handle that may be imported into another
///        connection
/// \param connection An open connection.
/// \param semaphore A valid semaphore.
/// \param semaphore_handle_out The returned handle.
///
magma_status_t magma_export_semaphore(
    magma_connection_t connection,
    magma_semaphore_t semaphore,
    magma_handle_t* semaphore_handle_out);

///
/// \brief Imports and takes ownership of the semaphore referred to by the given handle.
/// \param connection An open connection.
/// \param semaphore_handle A valid semaphore handle.
/// \param semaphore_out The returned semaphore.
///
magma_status_t magma_import_semaphore(
    magma_connection_t connection,
    magma_handle_t semaphore_handle,
    magma_semaphore_t* semaphore_out);

///
/// \brief Returns a handle that can be waited on to determine when the connection has data in the
///        notification channel. This channel has the same lifetime as the connection and must not
///        be closed by the client.
/// \param connection An open connection.
///
magma_handle_t magma_get_notification_channel_handle(
    magma_connection_t connection);

///
/// \brief Initializes tracing
/// \param channel An open connection to a tracing provider.
///
magma_status_t magma_initialize_tracing(
    magma_handle_t channel);

///
/// \brief Imports and takes ownership of a channel to a device.
/// \param device_channel A channel connecting to a gpu class device.
/// \param device_out Returned device.
///
magma_status_t magma_device_import(
    magma_handle_t device_channel,
    magma_device_t* device_out);

///
/// \brief Releases a handle to a device
/// \param device An open device.
///
void magma_device_release(
    magma_device_t device);

///
/// \brief Performs a query and returns a result synchronously.
/// \param device An open device.
/// \param id Either MAGMA_QUERY_DEVICE_ID, or a vendor-specific id starting from
///        MAGMA_QUERY_VENDOR_PARAM_0.
/// \param value_out Returned value.
///
magma_status_t magma_query2(
    magma_device_t device,
    uint64_t id,
    uint64_t* value_out);

///
/// \brief Performs a query for a large amount of data and puts that into a buffer. Returns
///        synchronously
/// \param device An open device.
/// \param id A vendor-specific ID.
/// \param handle_out Handle to the returned buffer.
///
magma_status_t magma_query_returns_buffer2(
    magma_device_t device,
    uint64_t id,
    magma_handle_t* handle_out);

///
/// \brief Opens a connection to a device.
/// \param device An open device
/// \param connection_out Returned connection.
///
magma_status_t magma_create_connection2(
    magma_device_t device,
    magma_connection_t* connection_out);

///
/// \brief Initializes logging; used for debug and some exceptional error reporting.
/// \param channel An open connection to the syslog service.
///
magma_status_t magma_initialize_logging(
    magma_handle_t channel);

///
/// \brief Waits for at least one of the given items to meet a condition. Does not reset any
///        semaphores. Results are returned in the items array.
/// \param items Array of poll items. Type should be either MAGMA_POLL_TYPE_SEMAPHORE or
///        MAGMA_POLL_TYPE_HANDLE. Condition may be set to MAGMA_POLL_CONDITION_SIGNALED OR
///        MAGMA_POLL_CONDITION_READABLE. If condition is 0 the item is ignored. Item results are
///        set to the condition that was satisfied, otherwise 0. If the same item is given twice the
///        behavior is undefined.
/// \param count Number of poll items in the array.
/// \param timeout_ns Time in ns to wait before returning MAGMA_STATUS_TIMED_OUT.
///
magma_status_t magma_poll(
    magma_poll_item_t* items,
    uint32_t count,
    uint64_t timeout_ns);

///
/// \brief Tries to enable access to performance counters. Returns MAGMA_STATUS_OK if counters were
///        successfully enabled or MAGMA_STATUS_ACCESS_DENIED if channel is for the wrong device and
///        counters were not successfully enabled previously.
/// \param connection An open connection to a device.
/// \param channel A handle to a channel to a gpu-performance-counter device.
///
magma_status_t magma_connection_access_performance_counters(
    magma_connection_t connection,
    magma_handle_t channel);

///
/// \brief Enables a set of performance counters (the precise definition depends on the GPU driver).
///        Disables enabled performance counters that are not in the new set. Performance counters
///        will also be automatically disabled on connection close. Performance counter access must
///        have been enabled using magma_connection_access_performance_counters before calling this
///        method.
/// \param connection An open connection to a device.
/// \param counters An implementation-defined list of counters.
/// \param counters_count The number of entries in |counters|.
///
magma_status_t magma_connection_enable_performance_counters(
    magma_connection_t connection,
    uint64_t* counters,
    uint64_t counters_count);

///
/// \brief Create a pool of buffers that performance counters can be dumped into. Performance
///        counter access must have been enabled using magma_connection_access_performance_counters
///        before calling this method.
/// \param connection An open connection to a device.
/// \param pool_out A new pool id. Must not currently be in use.
/// \param notification_handle_out A handle that should be waited on.
///
magma_status_t magma_connection_create_performance_counter_buffer_pool(
    magma_connection_t connection,
    magma_perf_count_pool_t* pool_out,
    magma_handle_t* notification_handle_out);

///
/// \brief Releases a pool of performance counter buffers. Performance counter access must have been
///        enabled using magma_connection_access_performance_counters before calling this method.
/// \param connection An open connection to a device.
/// \param pool An existing pool id.
///
magma_status_t magma_connection_release_performance_counter_buffer_pool(
    magma_connection_t connection,
    magma_perf_count_pool_t pool);

///
/// \brief Adds a an array of buffers + offset to the pool. |offsets[n].buffer_id| is the koid of a
///        buffer that was previously imported using ImportBuffer(). The same buffer may be added to
///        multiple pools. The pool will hold on to a reference to the buffer even after
///        ReleaseBuffer is called.  When dumped into this entry, counters will be written starting
///        at |offsets[n].offset| bytes into the buffer, and up to |offsets[n].offset| +
///        |offsets[n].size|. |offsets[n].size| must be large enough to fit all enabled counters.
///        Performance counter access must have been enabled using
///        magma_connection_access_performance_counters before calling this method.
/// \param connection An open connection to a device.
/// \param pool An existing pool.
/// \param offsets An array of offsets to add.
/// \param offsets_count The number of elements in offsets.
///
magma_status_t magma_connection_add_performance_counter_buffer_offsets_to_pool(
    magma_connection_t connection,
    magma_perf_count_pool_t pool,
    const struct magma_buffer_offset* offsets,
    uint64_t offsets_count);

///
/// \brief Removes every offset of a buffer from the pool. Performance counter access must have been
///        enabled using magma_connection_access_performance_counters before calling this method.
/// \param connection An open connection to a device.
/// \param pool_id An existing pool.
/// \param buffer A magma_buffer
///
magma_status_t magma_connection_remove_performance_counter_buffer_from_pool(
    magma_connection_t connection,
    magma_perf_count_pool_t pool_id,
    magma_buffer_t buffer);

///
/// \brief Triggers dumping of the performance counters into a buffer pool. May fail silently if
///        there are no buffers in the pool. |trigger_id| is an arbitrary ID assigned by the client
///        that can be returned in OnPerformanceCounterReadCompleted. Performance counter access
///        must have been enabled using magma_connection_access_performance_counters before calling
///        this method.
/// \param connection An open connection to a device.
/// \param pool An existing pool
/// \param trigger_id An arbitrary ID assigned by the client that will be returned in
///        OnPerformanceCounterReadCompleted.
///
magma_status_t magma_connection_dump_performance_counters(
    magma_connection_t connection,
    magma_perf_count_pool_t pool,
    uint32_t trigger_id);

///
/// \brief Sets the values of all listed performance counters to 0. May not be supported by some
///        hardware. Performance counter access must have been enabled using
///        magma_connection_access_performance_counters before calling this method.
/// \param connection An open connection to a device.
/// \param counters An implementation-defined list of counters.
/// \param counters_count The number of entries in |counters|.
///
magma_status_t magma_connection_clear_performance_counters(
    magma_connection_t connection,
    uint64_t* counters,
    uint64_t counters_count);

///
/// \brief Reads one performance counter completion event, if available.
/// \param connection An open connection to a device.
/// \param pool An existing pool.
/// \param trigger_id_out The trigger ID for this event.
/// \param buffer_id_out The buffer ID for this event.
/// \param buffer_offset_out The buffer offset for this event.
/// \param time_out The monotonic time this event happened.
/// \param result_flags_out A set of flags giving more information about this event.
///
magma_status_t magma_connection_read_performance_counter_completion(
    magma_connection_t connection,
    magma_perf_count_pool_t pool,
    uint32_t* trigger_id_out,
    uint64_t* buffer_id_out,
    uint32_t* buffer_offset_out,
    uint64_t* time_out,
    uint32_t* result_flags_out);

///
/// \brief Sets a name for the buffer for use in debugging tools.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param name The 0-terminated name of the buffer. May be truncated.
///
magma_status_t magma_buffer_set_name(
    magma_connection_t connection,
    magma_buffer_t buffer,
    const char* name);

///
/// \brief Perform an operation on a range of a buffer
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param options Options for the operation.
/// \param start_offset Byte offset into the buffer.
/// \param length Length (in bytes) of the region to operate on.
///
magma_status_t magma_buffer_range_op(
    magma_connection_t connection,
    magma_buffer_t buffer,
    uint32_t options,
    uint64_t start_offset,
    uint64_t length);

///
/// \brief Get information on a magma buffer
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param info_out Pointer to struct that receives the buffer info.
///
magma_status_t magma_buffer_get_info(
    magma_connection_t connection,
    magma_buffer_t buffer,
    magma_buffer_info_t* info_out);

///
/// \brief Gets a platform handle for the given buffer. This can be used to perform a CPU mapping of
///        the buffer using the standard syscall.  The handle may be released without invalidating
///        such CPU mappings.
/// \param connection An open connection.
/// \param buffer A valid buffer.
/// \param handle_out Pointer to the returned handle.
///
magma_status_t magma_get_buffer_handle(
    magma_connection_t connection,
    magma_buffer_t buffer,
    magma_handle_t* handle_out);

///
/// \brief Reads a notification from the channel into the given buffer.  Message sizes may vary
///        depending on the MSD.  If the buffer provided is too small for the message,
///        MAGMA_STATUS_INVALID_ARGS will be returned and the size of message will be returned in
///        the buffer_size_out parameter.
/// \param connection An open connection.
/// \param buffer Buffer into which to read notification data.
/// \param buffer_size Size of the given buffer.
/// \param buffer_size_out Returned size of the notification data written to the buffer, or 0 if
///        there are no messages pending.
/// \param more_data_out True if there is more notification data waiting.
///
magma_status_t magma_read_notification_channel2(
    magma_connection_t connection,
    void* buffer,
    uint64_t buffer_size,
    uint64_t* buffer_size_out,
    magma_bool_t* more_data_out);

///
/// \brief Creates an image buffer backed by a buffer collection given a DRM format and optional
///        modifier, as specified in the create info.
/// \param connection An open connection.
/// \param create_info Input parameters describing the image.
/// \param image_out The image buffer.
///
magma_status_t magma_virt_create_image(
    magma_connection_t connection,
    magma_image_create_info_t* create_info,
    magma_buffer_t* image_out);

///
/// \brief Returns parameters for an image created with virtmagma_create_image.
/// \param connection An open connection.
/// \param image The image buffer.
/// \param image_info_out Output parameters describing the image.
///
magma_status_t magma_virt_get_image_info(
    magma_connection_t connection,
    magma_buffer_t image,
    magma_image_info_t* image_info_out);

#if defined(__cplusplus)
}
#endif

#endif // SRC_GRAPHICS_LIB_MAGMA_INCLUDE_MAGMA_ABI_MAGMA_H_
