#include "alsa_pcm_framed_source.h"
#include "logger.h"

namespace alsa_rtsp {

alsaPcmFramedSource* alsaPcmFramedSource::createNew(UsageEnvironment& env, alsaCapture* capture) {
    return new alsaPcmFramedSource(env, capture);
}

alsaPcmFramedSource::alsaPcmFramedSource(UsageEnvironment& env, alsaCapture* capture)
    : FramedSource(env), fCapture(capture), fCurTimestamp(0) {  // Start at 1 second
    fFrameSize = fCapture->getBufferSize();
    if (fFrameSize > (1024 * 1024 * 10)) { // 10MB limit
        handleClosure();
        return;
    }
    fBuffer = new char[fFrameSize];
    gettimeofday(&fInitialTime, NULL);
    

    logMessage("Audio timing: " + std::to_string(TIMESTAMP_INCREMENT) + " ticks per packet");
}

alsaPcmFramedSource::~alsaPcmFramedSource() {
    delete[] fBuffer;
    logMessage("Successfully destroyed alsaPcmFramedSource.");
}

void alsaPcmFramedSource::doGetNextFrame() {
    if (!isCurrentlyAwaitingData()) return;

    // Read exactly 320 samples (one packet)
    int frames = fCapture->readFrames(fBuffer, NUM_OF_FRAMES_PER_PERIOD);
    if (frames < 0) {
        handleClosure();
        return;
    }

    // Calculate size in bytes (320 samples * channels * bytes_per_sample)
    fFrameSize = frames * fCapture->getChannels() * (fCapture->getBitDepth() / 8);
    
    // Calculate presentation time from start
    unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;  // Convert from 90kHz to microseconds
    fPresentationTime = fInitialTime;
    fPresentationTime.tv_sec += elapsedMicros / 1000000;
    fPresentationTime.tv_usec += elapsedMicros % 1000000;
    if (fPresentationTime.tv_usec >= 1000000) {
        fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
        fPresentationTime.tv_usec %= 1000000;
    }

    // Each packet is 20ms (320/16000 seconds)
    fDurationInMicroseconds = 20000;

    if (fFrameSize > fMaxSize) {
        fNumTruncatedBytes = fFrameSize - fMaxSize;
        fFrameSize = fMaxSize;
    } else {
        fNumTruncatedBytes = 0;
    }

    memcpy(fTo, fBuffer, fFrameSize);
    
    // Increment by 1800 ticks (90000/50 or 320 samples * (90000/16000))
    fCurTimestamp += TIMESTAMP_INCREMENT;

    FramedSource::afterGetting(this);
}

} // namespace alsa_rtsp