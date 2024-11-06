#include "alsa_pcm_media_subsession.h"
#include "alsa_pcm_framed_source.h"
#include "logger.h"

namespace alsa_rtsp {

alsaPcmMediaSubsession* alsaPcmMediaSubsession::createNew(UsageEnvironment& env, alsaCapture* capture, Boolean reuseFirstSource) {
    return new alsaPcmMediaSubsession(env, capture, reuseFirstSource);
}

alsaPcmMediaSubsession::alsaPcmMediaSubsession(UsageEnvironment& env, alsaCapture* capture, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource), fCapture(capture) {}

FramedSource* alsaPcmMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = fCapture->getSampleRate() * fCapture->getChannels() * fCapture->getBitDepth() / 1000;
    return alsaPcmFramedSource::createNew(envir(), fCapture);
}

RTPSink* alsaPcmMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
                                               unsigned char rtpPayloadTypeIfDynamic,
                                               FramedSource* inputSource) {
    logMessage("Creating new RTP sink with payload type: 97");
    return SimpleRTPSink::createNew(envir(), rtpGroupsock,
                                   97, // payload type
                                   fCapture->getSampleRate(),
                                   "audio", "L16",
                                   fCapture->getChannels(),
                                   False, // Don't set "rtptime" timestamp
                                   True); // Set "marker" bit on last packet
}

void alsaPcmMediaSubsession::deleteStream(unsigned clientSessionId, void*& streamToken) {
    logMessage("Deleting audio stream for client session: 97");
    OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
    if (!fCapture->reset()) {
        envir() << "Failed to reset ALSA capture device. Attempting to continue without reset.\n";
    }
}

char const* alsaPcmMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    // Critical SDP configuration for PCM audio
    // Note: L16 (Linear 16-bit PCM) format must be exactly "L16/<sample-rate>/<channels>"
    const char* fmtpFmt = 
            "a=rtpmap:97 L16/%u/%u\r\n"             // payload type, sample rate, channels
            "a=fmtp:97 channels=%u;byte-order=big-endian\r\n"  // Added byte-order
            "a=ptime:20\r\n"                        // 20ms packets for 16kHz
            "a=maxptime:20\r\n"                     // max packet time
            "a=sendonly\r\n"                        // This is a capture-only stream
            "a=clock-domain:PTP=IEEE1588-2008\r\n"; // Add precise timing info
    // Ensure we have enough space for the formatted string
    unsigned fmtpLineSize = strlen(fmtpFmt) + 100;  // Increased buffer size for safety
    char* fmtpLine = new char[fmtpLineSize];
    
    // Get parameters - ensure they're valid
    unsigned int sampleRate = fCapture->getSampleRate();
    unsigned int channels = fCapture->getChannels();
    
    // Debug output
    envir() << "Creating SDP line with sample rate: " << sampleRate 
            << " Hz, channels: " << channels << "\n";
    
    // Format the SDP line with proper parameter order
    snprintf(fmtpLine, fmtpLineSize, fmtpFmt,
            sampleRate,    // Sample rate first
            channels,      // Number of channels second
            channels);     // Channels again for fmtp line

    return fmtpLine;
}

} // namespace alsa_rtsp