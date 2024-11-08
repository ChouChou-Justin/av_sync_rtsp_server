# AVS RTSP Server

This project implements an RTSP server that captures video from a V4L2 (Video4Linux2) device and audio from a ALSA (Advanced Linux Sound Architecture) and streams it using the RTSP protocol. It utilizes the Live555 library for RTSP streaming capabilities.

## Features

- Captures video from V4L2 devices
- Encodes video in H.264 format
- Captures audio from ALSA devices
- Streams video over RTSP
- Configurable video parameters
- Configurable audio parameters
- Based on Live555 for robust RTSP implementation

## Dependencies

- CMake (version 3.10 or higher)
- Live555 library
- V4L2 development libraries
- C++ compiler with C++11 support

## Building the Project

1. Clone the repository:  
   ```
   git clone https://github.com/ChouChou-Justin/av_sync_rtsp_server.git
   cd avs_rtsp_server
   ```

2. Create a build directory and navigate to it:  
   ```
   mkdir build
   cd build   
   ```

3. Run CMake and build the project:   
   ```
   cmake ..
   make   
   ```

## Usage

After building the project, you can run the server with:
    ```
    ./avs_rtsp_server
    ```

