cmake_minimum_required(VERSION 3.0.0)
project(mqp_tests VERSION 0.1.0)

set(SOURCE_FILES tests.cpp MQProcessor.h semaphore.h)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} ${GTEST_LIBRARIES})
