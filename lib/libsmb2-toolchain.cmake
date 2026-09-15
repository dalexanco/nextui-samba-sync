# CMake toolchain file for cross-compiling libsmb2 with the NextUI
# aarch64-nextui-linux-gnu- toolchain (same compiler for tg5040 and tg5050;
# see ghcr.io/loveretro/${PLATFORM}-toolchain). Validated 2026-09-15: with no
# krb5 in this toolchain's sysroot, libsmb2's own CMakeLists.txt detects that
# and disables Kerberos/GSSAPI automatically, so no extra flags are needed
# here for that.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-nextui-linux-gnu-gcc)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
