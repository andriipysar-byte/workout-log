# The filesystem side of SessionStorage, shared by the tools and the app. The
# core never links this; it only knows the interface.
add_library(wlfs STATIC src/directory_storage.cpp)
target_include_directories(wlfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wlfs PUBLIC wlcore PRIVATE wl_warnings)

add_executable(wlfs_tests tests/directory_storage_test.cpp)
target_link_libraries(wlfs_tests PRIVATE wlfs doctest::doctest_with_main wl_warnings)
add_test(NAME wlfs COMMAND wlfs_tests)
