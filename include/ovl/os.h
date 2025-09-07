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
 * @return void* HINSTANCE handle of the module, or NULL on error
 */
void *ovl_os_get_hinstance_from_fnptr(void *fn);
#endif
