#pragma once

#include <ovbase.h>

struct ovl_source;

/**
 * @brief Vtable for the source.
 */
struct ovl_source_vtable {
  /**
   * Callback function to destroy the data source.
   *
   * @param sp Pointer to the data source.
   */
  void (*destroy)(struct ovl_source **const sp);
  /**
   * Callback function to read data.
   *
   * @param s      Pointer to the data source.
   * @param p      Pointer to the buffer to store data.
   * @param offset Offset in the data source from which to start reading.
   * @param len    Maximum number of bytes to read (can be up to SIZE_MAX - 1).
   * @return The number of bytes read on success, or SIZE_MAX if an error
   * occurs.
   */
  NODISCARD size_t (*read)(struct ovl_source *const s, void *const p, uint64_t const offset, size_t const len);
  /**
   * Callback function to query the data source size.
   *
   * @param s Pointer to the data source.
   * @return The size of the data source on success, or UINT64_MAX if an error
   * occurs.
   */
  NODISCARD uint64_t (*size)(struct ovl_source *const s);
};

/**
 * This structure is used as a data source for various audio format decoders.
 */
struct ovl_source {
  /**
   * @brief Vtable for the source.
   *
   * It is recommended to use the helper functions (e.g., source_read)
   * instead of directly calling the functions in this vtable.
   */
  struct ovl_source_vtable const *vtable;
};

/**
 * Destroy the data source.
 *
 * @param sp Pointer to the data source.
 */
static inline void ovl_source_destroy(struct ovl_source **const sp) { (*sp)->vtable->destroy(sp); }

/**
 * Read data from the data source.
 *
 * @param s      Pointer to the data source.
 * @param p      Pointer to the buffer to store data.
 * @param offset Offset in the data source from which to start reading.
 * @param len    Maximum number of bytes to read (can be up to SIZE_MAX - 1).
 * @return The number of bytes read on success, or SIZE_MAX if an error occurs.
 */
static inline NODISCARD size_t ovl_source_read(struct ovl_source *const s,
                                               void *const p,
                                               uint64_t const offset,
                                               size_t const len) {
  return s->vtable->read(s, p, offset, len);
}

/**
 * Query the data source size.
 *
 * @param s Pointer to the data source.
 * @return The size of the data source on success, or UINT64_MAX if an error
 * occurs.
 */
static inline NODISCARD uint64_t ovl_source_size(struct ovl_source *const s) { return s->vtable->size(s); }
