#ifndef V4L2_H264_FRAMED_SOURCE_H
#define V4L2_H264_FRAMED_SOURCE_H

#include <FramedSource.hh>
#include "v4l2_capture.h"
#include "constants.h"

struct InitialFrameData {
    uint8_t* sps;
    uint8_t* pps;
    uint8_t* idr;
    unsigned spsSize;
    unsigned ppsSize;
    unsigned idrSize;
    struct timeval initialTime;
    
    InitialFrameData() : sps(nullptr), pps(nullptr), idr(nullptr),
                        spsSize(0), ppsSize(0), idrSize(0) {}
    
    ~InitialFrameData() {
        delete[] sps;
        delete[] pps;
        delete[] idr;
    }
};

class v4l2H264FramedSource : public FramedSource {
public:
    static v4l2H264FramedSource* createNew(UsageEnvironment& env, v4l2Capture* capture, InitialFrameData* initData);
    
protected:
    v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture, InitialFrameData* initData);
    virtual ~v4l2H264FramedSource();

private:
    virtual void doGetNextFrame();
    v4l2Capture* fCapture;
    InitialFrameData* fInitData;
    uint32_t fCurTimestamp{0};  // Current RTP timestamp
    static const uint32_t TIMESTAMP_INCREMENT = 90000/FRAME_RATE_DENOMINATOR;  // 90kHz/30fps
    struct timeval fInitialTime;  // Base time for all calculations
    
    enum GopState {
        SENDING_SPS,
        SENDING_PPS,
        SENDING_IDR,
        SENDING_FRAMES
    };
    GopState gopState{SENDING_SPS};

};

#endif // V4L2_H264_FRAMED_SOURCE_H
