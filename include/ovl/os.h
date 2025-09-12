#pragma once

#include <ovbase.h>

#ifdef _WIN32
/**
 * @brief Gets HINSTANCE handle from a function pointer
 *
 * Retrieves the HINSTANCE handle of the module containing the specified function pointer.
 * This is a Windows-specific function used to identify the module handle of a DLL or EXE.
 *
 * @param fn Function pointer to get the HINSTANCE for
 * @param hinstance Output parameter for the HINSTANCE handle
 * @param err Error information output
 * @return bool true on success, false on failure
 */
bool ovl_os_get_hinstance_from_fnptr(void *fn, void **hinstance, struct ov_error *const err);
#endif
