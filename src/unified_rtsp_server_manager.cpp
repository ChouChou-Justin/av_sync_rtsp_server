#include "unified_rtsp_server_manager.h"
#include "v4l2_h264_media_subsession.h"
#include "alsa_pcm_media_subsession.h"
#include "logger.h"

UnifiedRTSPServerManager::UnifiedRTSPServerManager(UsageEnvironment* env, v4l2Capture* videoCapture, alsa_rtsp::alsaCapture* audioCapture, int port)
    : env_(env)
    , port_(port)
    , rtspServer_(nullptr)
    , sms_(nullptr)
    , videoCapture_(videoCapture)
    , audioCapture_(audioCapture) {
}

UnifiedRTSPServerManager::~UnifiedRTSPServerManager() {
    cleanup();
}

bool UnifiedRTSPServerManager::initialize() {
    // Create RTSP server
    rtspServer_ = RTSPServer::createNew(*env_, port_, nullptr);
    if (rtspServer_ == nullptr) {
        logMessage("Failed to create RTSP server: " + std::string(env_->getResultMsg()));
        return false;
    }
    logMessage("Created RTSP server on port " + std::to_string(port_));

    // Create a single session for both streams
    sms_ = ServerMediaSession::createNew(*env_,
        "unified_stream",  // stream name
        "Unified Audio/Video Stream",  // description
        "Session streamed by Unified RTSP Server",
        True);  // reuse first source

    // Add video subsession
    if (videoCapture_) {
        v4l2H264MediaSubsession* videoSubsession = 
            v4l2H264MediaSubsession::createNew(*env_, videoCapture_, True);
        if (videoSubsession == nullptr) {
            logMessage("Failed to create video subsession");
            return false;
        }
        sms_->addSubsession(videoSubsession);
        logMessage("Successfully add video subsession.");
    }

    // Add audio subsession
    if (audioCapture_) {
        alsa_rtsp::alsaPcmMediaSubsession* audioSubsession = 
            alsa_rtsp::alsaPcmMediaSubsession::createNew(*env_, audioCapture_, True);
        if (audioSubsession == nullptr) {
            logMessage("Failed to create audio subsession");
            return false;
        }
        sms_->addSubsession(audioSubsession);
        logMessage("Successfully add audio subsession.");
    }

    // Add session to server
    rtspServer_->addServerMediaSession(sms_);

    // Get stream URL
    char* url = rtspServer_->rtspURL(sms_);
    logMessage("Unified Stream URL: " + std::string(url));
    delete[] url;

    return true;
}

void UnifiedRTSPServerManager::runEventLoop(volatile char* shouldExit) {
    logMessage("Starting unified RTSP server event loop");
    env_->taskScheduler().doEventLoop(const_cast<char*>(shouldExit));  // Safe cast here
}

void UnifiedRTSPServerManager::cleanup() {
    logMessage("Cleaning up unified RTSP server");
    if (rtspServer_) {
        Medium::close(rtspServer_);
        rtspServer_ = nullptr;
    }
    sms_ = nullptr;  // Will be cleaned up by rtspServer_
}
