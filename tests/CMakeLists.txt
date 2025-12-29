# Tests CMake configuration

find_package(Qt6 REQUIRED COMPONENTS Test)

# Engine tests
add_executable(test_engine
    test_engine.cpp
)

target_link_libraries(test_engine PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_engine COMMAND test_engine)

# Future: Add more test targets
# add_executable(test_persistence ...)
# add_executable(test_segmentation ...)
