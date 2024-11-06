cmake_minimum_required(VERSION 3.10)
project(avs_rtsp_server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option for building tests (default to OFF)
option(BUILD_TESTS "Build test suite" OFF)

# Set the path to Live555
set(LIVE555_DIR "/home/pi/Desktop/live")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIVE555_DIR}/UsageEnvironment/include
    ${LIVE555_DIR}/groupsock/include
    ${LIVE555_DIR}/liveMedia/include
    ${LIVE555_DIR}/BasicUsageEnvironment/include
    ${ALSA_INCLUDE_DIRS}
)

# Find required packages
find_library(USAGE_ENVIRONMENT_LIB UsageEnvironment PATHS ${LIVE555_DIR}/UsageEnvironment)
find_library(BASIC_USAGE_ENVIRONMENT_LIB BasicUsageEnvironment PATHS ${LIVE555_DIR}/BasicUsageEnvironment)
find_library(GROUPSOCK_LIB groupsock PATHS ${LIVE555_DIR}/groupsock)
find_library(LIVEMEDIA_LIB liveMedia PATHS ${LIVE555_DIR}/liveMedia)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(ALSA REQUIRED)

# Main application source files
set(SOURCES
    src/main.cpp
    src/v4l2_capture.cpp
    src/v4l2_h264_framed_source.cpp
    src/v4l2_h264_media_subsession.cpp
    src/alsa_capture.cpp
    src/alsa_pcm_framed_source.cpp
    src/alsa_pcm_media_subsession.cpp
    src/unified_rtsp_server_manager.cpp
    src/logger.cpp
)

# Create main executable
add_executable(avs_rtsp_server ${SOURCES})

# Link libraries for main executable
target_link_libraries(avs_rtsp_server
    ${LIVEMEDIA_LIB}
    ${GROUPSOCK_LIB}
    ${BASIC_USAGE_ENVIRONMENT_LIB}
    ${USAGE_ENVIRONMENT_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    ${ALSA_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Testing configuration only if BUILD_TESTS is ON and GTest is found
if(BUILD_TESTS)
    # Try to find GTest
    find_package(GTest QUIET)
    
    if(GTEST_FOUND)
        enable_testing()
        
        # Test source files
        set(TEST_SOURCES
            tests/test_main.cpp
            tests/test_v4l2_capture.cpp
            tests/test_alsa_capture.cpp
            tests/test_sync_buffer.cpp
            tests/test_rtsp_server.cpp
            tests/test_system_integration.cpp
            # Add source files needed by tests (excluding main.cpp)
            src/v4l2_capture.cpp
            src/v4l2_h264_framed_source.cpp
            src/v4l2_h264_media_subsession.cpp
            src/alsa_capture.cpp
            src/alsa_pcm_framed_source.cpp
            src/alsa_pcm_media_subsession.cpp
            src/live555_rtsp_server_manager.cpp
            src/logger.cpp
        )

        # Create test executable
        add_executable(run_tests ${TEST_SOURCES})

        # Link test libraries
        target_link_libraries(run_tests
            GTest::GTest
            GTest::Main
            ${LIVEMEDIA_LIB}
            ${GROUPSOCK_LIB}
            ${BASIC_USAGE_ENVIRONMENT_LIB}
            ${USAGE_ENVIRONMENT_LIB}
            ${CMAKE_THREAD_LIBS_INIT}
            ${ALSA_LIBRARIES}
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Add test to CTest
        add_test(NAME unit_tests COMMAND run_tests)
        
        # Install test executable
        install(TARGETS run_tests DESTINATION bin)
    else()
        message(STATUS "GTest not found - tests will not be built")
    endif()
endif()

# Install main executable
install(TARGETS avs_rtsp_server DESTINATION bin)