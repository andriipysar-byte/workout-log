add_library(wl_warnings INTERFACE)

option(WL_WERROR "Treat compiler warnings as errors (CI turns this on)" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Designated initializers that leave optional members out are the idiom for
    # building models, and GCC's -Wextra flags every one of them.
    target_compile_options(wl_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers
        $<$<BOOL:${WL_WERROR}>:-Werror>)
endif()
