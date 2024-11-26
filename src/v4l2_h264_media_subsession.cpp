#include "v4l2_h264_media_subsession.h"
#include "v4l2_h264_framed_source.h"
#include "logger.h"
#include <Base64.hh>

v4l2H264MediaSubsession* v4l2H264MediaSubsession::createNew(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource) {
    return new v4l2H264MediaSubsession(env, capture, reuseFirstSource);
}

v4l2H264MediaSubsession::v4l2H264MediaSubsession(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource), 
      fCapture(capture), fAuxSDPLine(NULL) {
}

v4l2H264MediaSubsession::~v4l2H264MediaSubsession() {
    delete[] fAuxSDPLine;
}

FramedSource* v4l2H264MediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = 1000;
    logMessage("===========================================================");
    logMessage("Creating stream source for session: " + std::to_string(clientSessionId));
    
    // Force new SPS/PPS extraction for each session
    fCapture->stopCapture();
    usleep(100000);  // 100ms delay
    fCapture->clearSpsPps();
    usleep(100000);  // 100ms delay
    fCapture->reset();
    usleep(100000);  // 100ms delay
    fCapture->startCapture();

    if (!fCapture->extractSpsPpsImmediate()) {
        logMessage("Immediate SPS/PPS/IDR extraction failed for session " + std::to_string(clientSessionId));
        return nullptr;
    }

    // Prepare initial frame data with timestamps
    InitialFrameData* initData = new InitialFrameData();

    // Get initial timestamp before copying any frames
    gettimeofday(&initData->initialTime, NULL);

    // Copy SPS/PPS
    initData->spsSize = fCapture->getSPSSize();
    initData->ppsSize = fCapture->getPPSSize();
    initData->sps = new uint8_t[initData->spsSize];
    initData->pps = new uint8_t[initData->ppsSize];
    memcpy(initData->sps, fCapture->getSPS(), initData->spsSize);
    memcpy(initData->pps, fCapture->getPPS(), initData->ppsSize);
    
    // Add retry logic for IDR frame acquisition
    const int MAX_IDR_ATTEMPTS = 10;
    const int IDR_RETRY_DELAY_US = 100000; // 100ms delay between attempts

    for (int attempt = 0; attempt < MAX_IDR_ATTEMPTS; attempt++) {
        // Request a keyframe
        struct v4l2_control control;
        control.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
        if (ioctl(fCapture->getFd(), VIDIOC_S_CTRL, &control) != -1) {
            logMessage("Requested keyframe for IDR attempt " + std::to_string(attempt + 1));
        }

        // Wait for the camera to process the keyframe request
        usleep(IDR_RETRY_DELAY_US);

        // Try to get multiple frames to find an IDR
        const int FRAMES_TO_CHECK = 5;
        for (int frame_check = 0; frame_check < FRAMES_TO_CHECK; frame_check++) {
            size_t frameSize;
            unsigned char* frame = fCapture->getFrameWithoutStartCode(frameSize);
            
            if (frame && frameSize > 0) {
                if ((frame[0] & 0x1F) == 5) {  // Found IDR frame
                    initData->idr = new uint8_t[frameSize];
                    initData->idrSize = frameSize;
                    memcpy(initData->idr, frame, frameSize);
                    fCapture->releaseFrame();
                    
                    logMessage("Successfully acquired IDR frame on attempt " + std::to_string(attempt + 1) + ", frame check " + std::to_string(frame_check + 1));
                    
                    // Create source with initial data
                    v4l2H264FramedSource* source = v4l2H264FramedSource::createNew(
                        envir(), fCapture, initData);
                    
                    if (source == nullptr) {
                        delete initData;
                        logMessage("Failed to create source for session " + std::to_string(clientSessionId));
                        return nullptr;
                    }
                    
                    return H264VideoStreamDiscreteFramer::createNew(envir(), source);
                }
                fCapture->releaseFrame();
            }
            usleep(10000);  // 10ms delay between frame checks
        }
        
        logMessage("IDR frame not found in attempt " + std::to_string(attempt + 1) + ", retrying...");
    }

    // If we get here, we failed to get an IDR frame
    logMessage("Failed to get IDR frame after " + std::to_string(MAX_IDR_ATTEMPTS) + 
              " attempts for session " + std::to_string(clientSessionId));
    delete initData;
    return nullptr;
}

RTPSink* v4l2H264MediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) {
    logMessage("Creating new RTP sink with payload type: " + std::to_string(rtpPayloadTypeIfDynamic));
    
    // Ensure we have SPS/PPS
    if (!fCapture->hasSpsPps()) {
        if (!fCapture->extractSpsPps()) {
            envir() << "Failed to extract SPS/PPS. Cannot create RTP sink.\n";
            return nullptr;
        }
    }

    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                    fCapture->getSPS(), fCapture->getSPSSize(),
                                    fCapture->getPPS(), fCapture->getPPSSize());
}

void v4l2H264MediaSubsession::deleteStream(unsigned clientSessionId, void*& streamToken) {
    logMessage("Cleaning up session: " + std::to_string(clientSessionId));

    if (clientSessionId == streamingSessionId) {
        // Stop capture before cleanup
        fCapture->stopCapture();
        // usleep(50000); // 50ms delay
        
        // Reset streaming session ID
        streamingSessionId = 0;
        
        // Reset device after clearing session
        if (!fCapture->reset()) {
            logMessage("Warning: Failed to reset capture device during cleanup");
        }
    }
    
    // usleep(50000); // 50ms delay
    
    OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
}

char const* v4l2H264MediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    if (fAuxSDPLine != NULL) return fAuxSDPLine;

    if (!fCapture->hasSpsPps()) {
        if (!fCapture->extractSpsPps()) {
            envir() << "Failed to extract SPS and PPS. Cannot create aux SDP line.\n";
            return nullptr;
        }
    }
    
    u_int8_t* sps = fCapture->getSPS();
    u_int8_t* pps = fCapture->getPPS();
    unsigned spsSize = fCapture->getSPSSize();
    unsigned ppsSize = fCapture->getPPSSize();

    if (sps == nullptr || pps == nullptr || spsSize == 0 || ppsSize == 0) {
        envir() << "Invalid SPS or PPS. Cannot create aux SDP line.\n";
        return nullptr;
    }

    char* spsBase64 = base64Encode((char*)sps, spsSize);
    char* ppsBase64 = base64Encode((char*)pps, ppsSize);

    char const* fmtpFmt =
        "a=fmtp:%d packetization-mode=1;profile-level-id=%02X%02X%02X;sprop-parameter-sets=%s,%s\r\n";
    unsigned fmtpFmtSize = strlen(fmtpFmt)
        + 3 + 6 + strlen(spsBase64) + strlen(ppsBase64);
    char* fmtp = new char[fmtpFmtSize];
    sprintf(fmtp, fmtpFmt,
            rtpSink->rtpPayloadType(),
            sps[1], sps[2], sps[3],
            spsBase64, ppsBase64);

    delete[] spsBase64;
    delete[] ppsBase64;

    fAuxSDPLine = fmtp;
    return fAuxSDPLine;
}
