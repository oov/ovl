#pragma once

#include <ovbase.h>

struct ovl_source;

/**
 * @brief Create a source that reads from a memory buffer.
 *
 * @param ptr Pointer to the memory buffer. Must not be NULL if size > 0.
 * @param sz Size of the buffer in bytes.
 * @param[out] sp Pointer to receive the created source object.
 * @param[out] err Error object to receive error information on failure.
 * @return true on success, false on failure.
 */
NODISCARD bool ovl_source_memory_create(void const *const ptr,
                                        size_t const sz,
                                        struct ovl_source **const sp,
                                        struct ov_error *const err);
