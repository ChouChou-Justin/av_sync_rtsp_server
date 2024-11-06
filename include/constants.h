#ifndef CONSTANTS_H
#define CONSTANTS_H

// Video settings (V4L2)
#define VIDEO_DEVICE "/dev/video0"
#define VIDEO_BUFFER_COUNT 4
#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480
#define VIDEO_BITRATE 1000000    // 1 Mbps
#define GOP_SIZE 30              // GOP size (1 second at 30 fps)
#define FRAME_RATE_NUMERATOR 1
#define FRAME_RATE_DENOMINATOR 30  // 30 fps
#define ROTATION_DEGREES 180

// Audio settings (ALSA)
#define AUDIO_DEVICE "plughw:2,0"
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_CHANNELS 1
#define AUDIO_BIT_DEPTH 16
#define NUM_OF_PERIODS_IN_BUFFER 64
#define NUM_OF_FRAMES_PER_PERIOD 320

// RTSP server settings
#define DEFAULT_RTSP_PORT 8554

#endif // CONSTANTS_H