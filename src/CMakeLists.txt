option(USE_ADDRESS_SANITIZER "use address sanitizer" OFF)
option(USE_COMPILER_RT "use compiler-rt runtime" OFF)
option(USE_NO_PTHREAD "add -no-pthread" OFF)
option(USE_LTO "use LTO" ON)
option(TARGET_EMSCRIPTEN "target emscripten" OFF)
option(TARGET_WASI_SDK "target wasi-sdk" OFF)
set(LDNAME "lld" CACHE STRING "ld name")

# 3rd/ogg
set(INSTALL_DOCS OFF CACHE INTERNAL "")
set(INSTALL_PKGCONFIG_MODULES OFF CACHE INTERNAL "")
set(INSTALL_CMAKE_CONFIG_MODULE OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(BUILD_FRAMEWORK OFF CACHE INTERNAL "")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE INTERNAL "")
add_subdirectory(3rd/ogg)
target_compile_options(ogg PRIVATE -O3)
set(WITH_OGG ON CACHE INTERNAL "")

# 3rd/vorbis
set(INSTALL_CMAKE_CONFIG_MODULE OFF CACHE INTERNAL "")
add_subdirectory(3rd/vorbis)

# 3rd/opus
set(BUILD_TESTING OFF CACHE INTERNAL "")
add_subdirectory(3rd/opus)
target_compile_options(opus PRIVATE -O3)

# 3rd/opusfile
set(OP_DISABLE_HTTP ON CACHE INTERNAL "")
set(OP_DISABLE_EXAMPLES ON CACHE INTERNAL "")
set(OP_DISABLE_DOCS ON CACHE INTERNAL "")
add_subdirectory(3rd/opusfile)

# 3rd/flac
set(BUILD_CXXLIBS OFF CACHE INTERNAL "")
set(BUILD_PROGRAMS OFF CACHE INTERNAL "")
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_DOCS OFF CACHE INTERNAL "")
set(WITH_FORTIFY_SOURCE OFF CACHE INTERNAL "")
set(WITH_STACK_PROTECTOR OFF CACHE INTERNAL "")
set(INSTALL_MANPAGES OFF CACHE INTERNAL "")
add_subdirectory(3rd/flac)
target_compile_options(FLAC PRIVATE -O3)

# 3rd/minimp3
add_library(minimp3 INTERFACE)
target_include_directories(minimp3 INTERFACE 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rd/minimp3>
)

set(is_clang "$<C_COMPILER_ID:Clang>")
set(v16_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,16>")
set(v18_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,18>")
set(v19_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,19>")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
if (NOT TARGET ovbase) # if ovbase is not added in the parent project
  find_package(ovbase QUIET)
  if (NOT ovbase_FOUND) # If ovbase is not installed
    FetchContent_Declare(
      ovbase
      GIT_REPOSITORY https://github.com/oov/ovbase
      GIT_TAG        caace762bea6a59bfe4f9fc51a703099e3f65e9e
    )
    FetchContent_MakeAvailable(ovbase)
  endif()
endif()

add_library(ovl_intf INTERFACE)
target_compile_options(ovl_intf
INTERFACE
  -mstackrealign
  -Wall
  -Wextra
  -Werror
  -Weverything
  -Wshadow
  -Werror=return-type
  -pedantic-errors
  -Wno-declaration-after-statement
  -Wno-padded
  $<$<AND:${is_clang},${v19_or_later}>:-Wno-pre-c11-compat>
  $<$<AND:${is_clang},${v18_or_later}>:-Wno-switch-default>
  $<$<AND:${is_clang},${v16_or_later}>:-Wno-unsafe-buffer-usage>
  -ffunction-sections
  -fdata-sections
  $<$<BOOL:${USE_ADDRESS_SANITIZER}>:-fsanitize=address>
  $<$<CONFIG:Debug>:-O0>
  $<$<CONFIG:Release>:-O2>
  $<$<BOOL:${USE_LTO}>:-flto>
)
target_link_options(ovl_intf
INTERFACE
  $<$<NOT:$<STREQUAL:${LDNAME},>>:-fuse-ld=${LDNAME}>
  -Wl,--gc-sections
  # -Wl,--print-gc-sections
  -static
  $<$<BOOL:${TARGET_WASI_SDK}>:-Wl,--import-memory,--export-memory,--max-memory=67108864>
)
target_link_libraries(ovl_intf INTERFACE
  comctl32
  ovbase

  ogg
  vorbis
  vorbisfile
  opus
  opusfile
  FLAC
  minimp3
)

# Copy headers to build directory
set(SOURCE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(DESTINATION_INCLUDE_DIR ${PROJECT_BINARY_DIR}/include)
file(GLOB_RECURSE headers "${SOURCE_INCLUDE_DIR}/ovl/*.h")
foreach(header ${headers})
  file(RELATIVE_PATH rel_path ${SOURCE_INCLUDE_DIR} ${header})
  configure_file(${header} ${DESTINATION_INCLUDE_DIR}/${rel_path} COPYONLY)
endforeach()

add_library(ovl STATIC
  # Core files
  file.c
  path.c
  source/file.c
  source/memory.c
  
  # Tag handling
  audio/tag.c
  audio/tag/id3v2.c
  audio/tag/vorbis_comment.c
  
  # Decoders
  audio/decoder/bidi.c
  audio/decoder/flac.c
  audio/decoder/mp3.c
  audio/decoder/ogg.c
  audio/decoder/opus.c
  audio/decoder/wav.c

  # Test Utility
  test_util.c
)

set_target_properties(ovl PROPERTIES
RUNTIME_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/bin
ARCHIVE_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/lib
LIBRARY_OUTPUT_DIRECTORY
  ${PROJECT_BINARY_DIR}/lib
)

target_include_directories(ovl
PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ovl
PRIVATE
  $<$<BOOL:${WIN32}>:_WIN32_WINNT=0x0601>
  $<$<CONFIG:Release>:NDEBUG>
)

target_compile_options(ovl
PUBLIC
  $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${USE_COMPILER_RT}>>:--rtlib=compiler-rt>
  $<$<BOOL:${TARGET_EMSCRIPTEN}>:-sUSE_PTHREADS>
)

target_link_options(ovl
PUBLIC
  $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${USE_COMPILER_RT}>>:--rtlib=compiler-rt>
  $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${USE_NO_PTHREAD}>>:-no-pthread>
  $<$<NOT:$<BOOL:${WIN32}>>:-pthread>
  $<$<BOOL:${USE_ADDRESS_SANITIZER}>:-fsanitize=address>
  $<$<BOOL:${TARGET_EMSCRIPTEN}>:-sUSE_PTHREADS -sSTANDALONE_WASM>
PRIVATE
  -static
)

# External libraries are now properly configured and linked

# Link against required libraries
target_link_libraries(ovl
PRIVATE
  ovl_intf
)

# Install targets
install(TARGETS ovl
        EXPORT ovl-export
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ovl DESTINATION include)

# Test executables
add_executable(test_path path_test.c)
list(APPEND tests test_path)

add_executable(test_decoder_flac audio/decoder/flac_test.c)
list(APPEND tests test_decoder_flac)

add_executable(test_decoder_mp3 audio/decoder/mp3_test.c)
list(APPEND tests test_decoder_mp3)

add_executable(test_decoder_ogg audio/decoder/ogg_test.c)
list(APPEND tests test_decoder_ogg)

add_executable(test_decoder_opus audio/decoder/opus_test.c)
list(APPEND tests test_decoder_opus)

add_executable(test_decoder_wav audio/decoder/wav_test.c)
list(APPEND tests test_decoder_wav)

add_executable(test_decoder_bidi audio/decoder/bidi_test.c)
list(APPEND tests test_decoder_bidi)

add_executable(test_tag_id3v2 audio/tag/id3v2_test.c)
list(APPEND tests test_tag_id3v2)

foreach(target ${tests})
  add_test(NAME ${target} COMMAND ${target})
  target_compile_definitions(${target}
  PRIVATE
    SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    "TESTDATADIR=NSTR(\"${CMAKE_CURRENT_SOURCE_DIR}/../testdata\")"
  )
  target_link_libraries(${target} PRIVATE
    ovbase
    ovl
    $<$<BOOL:${WIN32}>:shlwapi>
    ovl_intf

    ogg
    vorbis
    vorbisfile
    opus
    opusfile
    FLAC
    minimp3
  )
endforeach(target)