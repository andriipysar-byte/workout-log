add_executable(wl_fmt wl_fmt.cpp)
target_link_libraries(wl_fmt PRIVATE wlfs wl_warnings)

add_executable(wl_gen_cycle wl_gen_cycle.cpp)
target_link_libraries(wl_gen_cycle PRIVATE wlfs wl_warnings)

# The archive itself is the golden set: every committed file must already be canonical.
add_test(NAME wl_fmt_check COMMAND wl_fmt --check ${PROJECT_SOURCE_DIR}/data)
