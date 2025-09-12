#pragma once

#include <ovbase.h>

struct ovl_source;

NODISCARD bool
ovl_source_file_create(NATIVE_CHAR const *const path, struct ovl_source **const sp, struct ov_error *const err);
