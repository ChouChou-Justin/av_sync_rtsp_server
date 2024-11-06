#pragma once // Preventing multiple inclusions of header files

#include <alsa/asoundlib.h>
#include <vector>
#include "constants.h"

namespace alsa_rtsp {

class alsaCapture {
public:
    // Parameterized constructor for flexibility
    alsaCapture(const char* device, unsigned int sampleRate, 
                unsigned int channels, unsigned int bitDepth);
    ~alsaCapture();

    bool initialize();
    bool startCapture();
    bool stopCapture();
    bool reset();
    int readFrames(char* outbuffer, int outFrames);

    // Getters for audio parameters
    unsigned int getSampleRate() const { return AUDIO_SAMPLE_RATE; }
    unsigned int getChannels() const { return AUDIO_CHANNELS; }
    unsigned int getBitDepth() const { return AUDIO_BIT_DEPTH; }
    size_t getBufferSize() const { return buffer_size; }

private:
    const char* pcm_device;
    unsigned int sample_rate;
    unsigned int num_channels;
    unsigned int bit_depth;
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* params;
    snd_pcm_uframes_t frames;
    snd_pcm_uframes_t periods;
    std::vector<char> buffer;
    size_t buffer_size;
};

} // namespace alsa_rtsp