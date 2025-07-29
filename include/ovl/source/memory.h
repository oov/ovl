#pragma once

#include <ovbase.h>

struct ovl_source;

/**
 * @brief Create a source that reads from a memory buffer.
 *
 * @param ptr Pointer to the memory buffer. Must not be NULL if size > 0.
 * @param sz Size of the buffer in bytes.
 * @param[out] sp Pointer to receive the created source object.
 * @return error code indicating success or failure.
 */
NODISCARD error ovl_source_memory_create(void const *const ptr, size_t const sz, struct ovl_source **const sp);
