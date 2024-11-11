#include "v4l2_h264_framed_source.h"
#include "logger.h"

v4l2H264FramedSource* v4l2H264FramedSource::createNew(UsageEnvironment& env, v4l2Capture* capture, InitialFrameData* initData) {
    return new v4l2H264FramedSource(env, capture, initData);
}

v4l2H264FramedSource::v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture, InitialFrameData* initData)
    : FramedSource(env), 
      fCapture(capture), 
      fInitData(initData),
      fCurTimestamp(0),
      gopState(SENDING_SPS){ // Start in SENDING_SPS state immediately

    // Initialize with the provided data
    fInitialTime = initData->initialTime;
}

v4l2H264FramedSource::~v4l2H264FramedSource() {
    delete fInitData;  // Clean up initial frame data
    logMessage("Successfully destroyed v4l2H264FramedSource.");
}

void v4l2H264FramedSource::doGetNextFrame() {
    if (!isCurrentlyAwaitingData()) return;

    switch (gopState) {
        case SENDING_SPS: {
            if (fInitData->sps && fInitData->spsSize <= fMaxSize) {
                memcpy(fTo, fInitData->sps, fInitData->spsSize);
                fFrameSize = fInitData->spsSize;
                // Use GOP starting timestamp for SPS
                unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
                fPresentationTime = fInitialTime;
                fPresentationTime.tv_sec += elapsedMicros / 1000000;
                fPresentationTime.tv_usec += elapsedMicros % 1000000;
                if (fPresentationTime.tv_usec >= 1000000) {
                    fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                    fPresentationTime.tv_usec %= 1000000;
                }
                fDurationInMicroseconds = 0;
                gopState = SENDING_PPS;
                FramedSource::afterGetting(this);
            }
            break;
        }
        
        case SENDING_PPS: {
            if (fInitData->pps && fInitData->ppsSize <= fMaxSize) {
                memcpy(fTo, fInitData->pps, fInitData->ppsSize);
                fFrameSize = fInitData->ppsSize;
                // Use same timestamp as SPS
                fPresentationTime = fInitialTime;
                unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
                fPresentationTime.tv_sec += elapsedMicros / 1000000;
                fPresentationTime.tv_usec += elapsedMicros % 1000000;
                if (fPresentationTime.tv_usec >= 1000000) {
                    fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                    fPresentationTime.tv_usec %= 1000000;
                }
                fDurationInMicroseconds = 0;
                gopState = SENDING_IDR;
                FramedSource::afterGetting(this);
            }
            break;
        }

        case SENDING_IDR: {
            if (fInitData->idr && fInitData->idrSize <= fMaxSize) {
                memcpy(fTo, fInitData->idr, fInitData->idrSize);
                fFrameSize = fInitData->idrSize;
                // Use same timestamp as SPS/PPS
                fPresentationTime = fInitialTime;
                unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
                fPresentationTime.tv_sec += elapsedMicros / 1000000;
                fPresentationTime.tv_usec += elapsedMicros % 1000000;
                if (fPresentationTime.tv_usec >= 1000000) {
                    fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                    fPresentationTime.tv_usec %= 1000000;
                }
                fDurationInMicroseconds = 33333;  // First frame duration
                gopState = SENDING_FRAMES;
                fCurTimestamp += TIMESTAMP_INCREMENT;  // Start incrementing from next frame
                delete[] fInitData->idr;  // Clear the stored IDR as we'll get new ones
                fInitData->idr = nullptr;
                fInitData->idrSize = 0;
                FramedSource::afterGetting(this);
            }
            break;
        }
        
        case SENDING_FRAMES: {
            // Regular frame delivery
            size_t length;
            unsigned char* frame = fCapture->getFrameWithoutStartCode(length);
            
            if (frame == nullptr || !fCapture->isFrameValid()) {
                handleClosure();
                return;
            }

            // Check for new IDR frame
            if (length > 0 && (frame[0] & 0x1F) == 5) {
                // Store new IDR frame and prepare for new GOP sequence
                fInitData->idr = new uint8_t[length];
                fInitData->idrSize = length;
                memcpy(fInitData->idr, frame, length);
                fCapture->releaseFrame();

                // Start new GOP sequence
                gopState = SENDING_SPS;
                // Don't increment timestamp here as we want same timestamp for SPS/PPS/IDR
                doGetNextFrame();
                return;
            }
            
            if (length <= fMaxSize) {
                memcpy(fTo, frame, length);
                fFrameSize = length;
                fNumTruncatedBytes = 0;
            } else {
                memcpy(fTo, frame, fMaxSize);
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = length - fMaxSize;
            }

            // Calculate presentation time for regular frames
            unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
            fPresentationTime = fInitialTime;
            fPresentationTime.tv_sec += elapsedMicros / 1000000;
            fPresentationTime.tv_usec += elapsedMicros % 1000000;
            if (fPresentationTime.tv_usec >= 1000000) {
                fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                fPresentationTime.tv_usec %= 1000000;
            }
            
            fDurationInMicroseconds = 33333;  // 30fps
            fCurTimestamp += TIMESTAMP_INCREMENT;

            fCapture->releaseFrame();
            FramedSource::afterGetting(this);
            break;
        }
    }
}


