#ifndef UNIFIED_RTSP_SERVER_MANAGER_H
#define UNIFIED_RTSP_SERVER_MANAGER_H

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

// Include both capture headers
#include "v4l2_capture.h"
#include "alsa_capture.h"

// Since we're combining both, we'll stay in global namespace for now
class UnifiedRTSPServerManager {
public:
    // Constructor with both captures
    UnifiedRTSPServerManager(
        UsageEnvironment* env, 
        v4l2Capture* videoCapture,
        alsa_rtsp::alsaCapture* audioCapture, 
        int port = 8554);
    
    ~UnifiedRTSPServerManager();

    // Keep existing interface
    bool initialize();
    void runEventLoop(volatile char* shouldExit);  // Changed parameter type
    void cleanup();

private:
    // Environment and server components
    UsageEnvironment* env_;
    int port_;
    RTSPServer* rtspServer_;
    ServerMediaSession* sms_;

    // Both captures
    v4l2Capture* videoCapture_;
    alsa_rtsp::alsaCapture* audioCapture_;
};

#endif // UNIFIED_RTSP_SERVER_MANAGER_H