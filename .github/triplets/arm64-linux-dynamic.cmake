set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Disable platform-specific assembly optimizations in mpg123
# to avoid linker and compilation errors on ARM64.
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DHAVE_ARCH_IS_ARM64=OFF")
