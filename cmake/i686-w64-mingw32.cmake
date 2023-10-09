set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
add_definitions(-D_WIN32_WINNT=0x0600)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_C_COMPILER_TARGET i686-w64-mingw32)
set(CMAKE_ASM_COMPILER i686-w64-mingw32-as)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_CXX_COMPILER_TARGET i686-w64-mingw32)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
# CMAKE_AR is required to be a CACHE variable
set(CMAKE_AR
    i686-w64-mingw32-ar
    CACHE FILEPATH "Archiver"
)
set(CMAKE_SYSROOT /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
