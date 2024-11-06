#pragma once

#include <liveMedia.hh>
#include "alsa_capture.h"

namespace alsa_rtsp {

class alsaPcmMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static alsaPcmMediaSubsession* createNew(UsageEnvironment& env, alsaCapture* capture, Boolean reuseFirstSource);

protected:
    alsaPcmMediaSubsession(UsageEnvironment& env, alsaCapture* capture, Boolean reuseFirstSource);

    // Live555 virtual functions for streaming setup
    FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) override;
    RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) override;
    char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) override;
    void deleteStream(unsigned clientSessionId, void*& streamToken) override;
private:
    alsaCapture* fCapture;
};

} // namespace alsa_rtsp