add_library(wl_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wl_warnings INTERFACE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(wl_warnings INTERFACE /W4)
endif()
