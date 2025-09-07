#include <ovl/os.h>

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

void *ovl_os_get_hinstance_from_fnptr(void *fn) {
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery((LPCVOID)fn, &mbi, sizeof(mbi))) {
    return (HINSTANCE)mbi.AllocationBase;
  }
  return NULL;
}

#endif
