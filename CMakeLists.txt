cmake_minimum_required(VERSION 2.8.5)

# Set the default CMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE INTERNAL
        "Set the default CMake build type to Release" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

project(QuickSched)

find_package(OpenMP)
if(OPENMP_FOUND)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
else(OPENMP_FOUND)
  message(ERROR "OpenMP not found.")
endif(OPENMP_FOUND)

# add C library
add_subdirectory(quicksched)

# compile test
add_executable(testQuickSched testQuickSched.cpp)
target_link_libraries(testQuickSched quicksched)
