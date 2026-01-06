#pragma once

#include <ovbase.h>

/**
 * @brief File seek method enumeration
 *
 * Specifies the reference point for file seek operations.
 */
enum ovl_file_seek_method {
  ovl_file_seek_method_set, /**< Seek from the beginning of the file */
  ovl_file_seek_method_cur, /**< Seek from the current position */
  ovl_file_seek_method_end, /**< Seek from the end of the file */
};

/**
 * @brief Opaque file handle structure
 */
struct ovl_file;

/**
 * @brief Creates a new file for writing
 *
 * Creates a new file at the specified path. If the file already exists, it will be truncated.
 *
 * @param path File path to create
 * @param file Output parameter for the file handle
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_create(NATIVE_CHAR const *const path, struct ovl_file **const file, struct ov_error *const err);

/**
 * @brief Opens an existing file for reading
 *
 * Opens an existing file at the specified path for read access.
 *
 * @param path File path to open
 * @param file Output parameter for the file handle
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_open(NATIVE_CHAR const *const path, struct ovl_file **const file, struct ov_error *const err);

/**
 * @brief Closes a file handle
 *
 * Closes the file and releases associated resources.
 *
 * @param file File handle to close
 */
void ovl_file_close(struct ovl_file *const file);

/**
 * @brief Reads data from a file
 *
 * Reads up to buffer_bytes bytes from the file into the buffer.
 *
 * @param file File handle
 * @param buffer Buffer to read data into
 * @param buffer_bytes Maximum number of bytes to read
 * @param read Output parameter for the actual number of bytes read
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_read(struct ovl_file *const file,
                             void *const buffer,
                             size_t const buffer_bytes,
                             size_t *const read,
                             struct ov_error *const err);

/**
 * @brief Writes data to a file
 *
 * Writes buffer_bytes bytes from the buffer to the file.
 *
 * @param file File handle
 * @param buffer Buffer containing data to write
 * @param buffer_bytes Number of bytes to write
 * @param written Output parameter for the actual number of bytes written
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_write(struct ovl_file *const file,
                              void const *const buffer,
                              size_t const buffer_bytes,
                              size_t *const written,
                              struct ov_error *const err);

/**
 * @brief Seeks to a position in the file
 *
 * Moves the file pointer to a new position based on the specified method.
 *
 * @param file File handle
 * @param pos Position offset (can be negative for backward seek)
 * @param method Seek reference point (set, cur, or end)
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_seek(struct ovl_file *const file,
                             int64_t pos,
                             enum ovl_file_seek_method const method,
                             struct ov_error *const err);

/**
 * @brief Gets the current file position
 *
 * Retrieves the current position of the file pointer.
 *
 * @param file File handle
 * @param pos Output parameter for the current position
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_tell(struct ovl_file *const file, int64_t *const pos, struct ov_error *const err);

/**
 * @brief Gets the file size
 *
 * Retrieves the total size of the file in bytes.
 *
 * @param file File handle
 * @param size Output parameter for the file size
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_size(struct ovl_file *const file, uint64_t *const size, struct ov_error *const err);

/**
 * @brief Creates a unique file in the specified directory
 *
 * Creates a new file with a unique name based on the base_name in the specified directory.
 * If a file with the same name exists, a numeric suffix is appended to make it unique.
 *
 * @param dir Directory path where the file will be created
 * @param base_name Base filename to use
 * @param file Output parameter for the file handle
 * @param created_path Output parameter for the actual created file path (allocated via OV_ARRAY_GROW, caller must free
 * with OV_ARRAY_DESTROY)
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_create_unique(NATIVE_CHAR const *const dir,
                                      NATIVE_CHAR const *const base_name,
                                      struct ovl_file **const file,
                                      NATIVE_CHAR **const created_path,
                                      struct ov_error *const err);

/**
 * @brief Creates a temporary file
 *
 * Creates a new temporary file with a unique name based on the base_name in the system temporary directory.
 *
 * @param base_name Base filename to use
 * @param file Output parameter for the file handle
 * @param created_path Output parameter for the actual created file path (allocated via OV_ARRAY_GROW, caller must free
 * with OV_ARRAY_DESTROY)
 * @param err Error information output
 * @return bool true on success, false on failure
 */
NODISCARD bool ovl_file_create_temp(NATIVE_CHAR const *const base_name,
                                    struct ovl_file **const file,
                                    NATIVE_CHAR **const created_path,
                                    struct ov_error *const err);
