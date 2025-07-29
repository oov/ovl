cmake_minimum_required(VERSION 3.20)

option(FORMAT_SOURCES "execute clang-format" ON)

project(ovl
        HOMEPAGE_URL https://github.com/oov/ovl
        LANGUAGES C)
if(WIN32)
  enable_language(RC)
endif()
enable_testing()

if(FORMAT_SOURCES)
  file(GLOB sources "${PROJECT_SOURCE_DIR}/src/*.c" "${PROJECT_SOURCE_DIR}/src/*/*.c" "${PROJECT_SOURCE_DIR}/include/ovl/*.h" "${PROJECT_SOURCE_DIR}/include/ovl/*/*.h")
  find_program(CLANG_FORMAT_EXE clang-format)
  add_custom_target(${PROJECT_NAME}-format ALL
    COMMAND ${CLANG_FORMAT_EXE} -style=file -i ${sources}
  )
endif()

add_subdirectory(src)