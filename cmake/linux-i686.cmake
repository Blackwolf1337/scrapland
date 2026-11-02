# Native 32 bit Linux build: the system gcc with -m32 (multilib, lib32 libraries), ELF, cdecl default.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_FLAGS_INIT -m32)
set(CMAKE_EXE_LINKER_FLAGS_INIT -m32)
set(CMAKE_SHARED_LINKER_FLAGS_INIT -m32)
set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
set(FIND_LIBRARY_USE_LIB32_PATHS ON)
