#!/usr/bin/env bash
set -eu

CUR_DIR="${PWD}"
cd "$(dirname "${BASH_SOURCE:-$0}")"

INSTALL_TOOLS=1
REBUILD=0
SKIP_TESTS=0
CMAKE_PRESET=release
FORMAT_SOURCES=ON
USE_ADDRESS_SANITIZER=OFF
while [[ $# -gt 0 ]]; do
  case $1 in
    --no-install-tools)
      INSTALL_TOOLS=0
      shift
      ;;
    -d|--debug)
      CMAKE_PRESET=debug
      shift
      ;;
    --no-format)
      FORMAT_SOURCES=OFF
      shift
      ;;
    -r|--rebuild)
      REBUILD=1
      shift
      ;;
    -s|--skip-tests)
      SKIP_TESTS=1
      shift
      ;;
    --asan)
      USE_ADDRESS_SANITIZER=ON
      shift
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      shift
      ;;
  esac
done

if [ "${INSTALL_TOOLS}" -eq 1 ]; then
  mkdir -p "build/tools"
  if [ ! -e "${PWD}/build/tools/setup-llvm-mingw.sh" ]; then
    curl -o "${PWD}/build/tools/setup-llvm-mingw.sh" -sOL https://raw.githubusercontent.com/oov/ovbase/219351252f1689c99b0dfdaeb4eed038a5fd58e0/setup-llvm-mingw.sh
  fi
  . "${PWD}/build/tools/setup-llvm-mingw.sh" --dir "${PWD}/build/tools"
fi

ARCHDIR=${ARCHDIR:-`uname -m`}
destdir="${PWD}/build/${CMAKE_PRESET}/${ARCHDIR}"
CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX:-"${destdir}/local"}
CMAKE_C_COMPILER=${CMAKE_C_COMPILER:-clang}
CMAKE_TOOL_CHANIN_FILE=${CMAKE_TOOL_CHANIN_FILE:-"${PWD}/cmake/llvm-mingw.cmake"}

if [ "${REBUILD}" -eq 1 ] || [ ! -e "${destdir}/CMakeCache.txt" ]; then
  rm -rf "${destdir}"
  cmake -S . -B "${destdir}" --preset "${CMAKE_PRESET}" \
    -DFORMAT_SOURCES="${FORMAT_SOURCES}" \
    -DUSE_ADDRESS_SANITIZER="${USE_ADDRESS_SANITIZER}" \
    -DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}" \
    -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER}" \
    -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOL_CHANIN_FILE}"
fi
cmake --build "${destdir}"
if [ "${SKIP_TESTS}" -eq 0 ]; then
  ctest --test-dir "${destdir}" --output-on-failure --output-junit testlog.xml
fi
cmake --install "${destdir}"

cd "${CUR_DIR}"