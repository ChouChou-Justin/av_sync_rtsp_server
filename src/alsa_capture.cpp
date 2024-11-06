#include "alsa_capture.h"
#include "logger.h"
#include <iostream>
#include <cstring>
#include <chrono>

namespace alsa_rtsp {

alsaCapture::alsaCapture(const char* device, unsigned int sampleRate, 
                        unsigned int channels, unsigned int bitDepth)
    // Member initializer list - initializes class members before constructor body
    : pcm_device(device)                          // Initialize ALSA device name
    , sample_rate(sampleRate)                     // Initialize sampling rate
    , num_channels(channels)                      // Initialize number of channels
    , bit_depth(bitDepth)                         // Initialize bits per sample
    , pcm_handle(nullptr)                         // Initialize PCM handle to null
    , params(nullptr)                             // Initialize params to null
    , frames(NUM_OF_FRAMES_PER_PERIOD)            // Initialize frames per period
    , periods(NUM_OF_PERIODS_IN_BUFFER) {         // Initialize number of periods
    // Calculate total buffer size in bytes:
    // frames * channels * (bytes per sample) * number of periods
    buffer_size = frames * channels * (bitDepth / 8) * periods;
    
    // Resize the buffer to calculated size
    buffer.resize(buffer_size);
}

alsaCapture::~alsaCapture() {
    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
    }
}

bool alsaCapture::initialize() {
    int pcm;
    int dir = 0;  // Force exact rate with dir = 0

    // Open PCM device in blocking mode
    if ((pcm = snd_pcm_open(&pcm_handle, pcm_device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "ERROR: Can't open \"" << pcm_device << "\" PCM device. " << snd_strerror(pcm) << std::endl;
        return false;
    }
    
    // Configure for low latency
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    // Add these lines for explicit configuration
    if ((pcm = snd_pcm_hw_params_set_rate_resample(pcm_handle, params, 1)) < 0) {
        std::cerr << "Cannot set resampling: " << snd_strerror(pcm) << std::endl;
        return false;
    }

    // Set hardware parameters with explicit error checking
    if ((pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        std::cerr << "Error setting access: " << snd_strerror(pcm) << std::endl;
        return false;
    }

    if ((pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_BE)) < 0) {
        std::cerr << "Error setting format: " << snd_strerror(pcm) << std::endl;
        return false;
    }
    
    if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, num_channels)) < 0) {
        std::cerr << "Error setting channels: " << snd_strerror(pcm) << std::endl;
        return false;
    }
    
    // Set sample rate with explicit checking
    unsigned int rateNear = sample_rate;
    if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rateNear, 0)) < 0) {
        std::cerr << "Error setting rate: " << snd_strerror(pcm) << std::endl;
        return false;
    }
    
    if (rateNear != sample_rate) {
        std::cerr << "Warning: Rate " << sample_rate << " Hz not supported, using " 
                << rateNear << " Hz instead" << std::endl;
        return false;
    }

    // After setting hardware parameters, verify what we got
    unsigned int actualRate;
    snd_pcm_format_t actualFormat;
    unsigned int actualChannels;
    snd_pcm_hw_params_get_rate(params, &actualRate, 0);
    snd_pcm_hw_params_get_format(params, &actualFormat);
    snd_pcm_hw_params_get_channels(params, &actualChannels);

    // Verify we got what we requested
    if (actualRate != sample_rate) {
        std::cerr << "WARNING: Sample rate mismatch - requested " 
                << sample_rate << " Hz, got " << actualRate << " Hz\n";
        return false;
    }

    // Set period size (in frames)
    snd_pcm_uframes_t period_size = frames;
    if ((pcm = snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &period_size, &dir)) < 0) {
        std::cerr << "Error setting period size: " << snd_strerror(pcm) << std::endl;
        return false;
    }

    // Set buffer size (in frames)
    snd_pcm_uframes_t buffer_size = frames * periods;
    if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &buffer_size)) < 0) {
        std::cerr << "Error setting buffer size: " << snd_strerror(pcm) << std::endl;
        return false;
    }

    // Apply hardware parameters
    if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0) {
        std::cerr << "ERROR: Can't set hardware parameters. " << snd_strerror(pcm) << std::endl;
        return false;
    }

    logMessage("Successfully set hardware parameters.");

    // Configure software parameters for better buffer management and additional protection against underruns
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(pcm_handle, swparams);
    
    // Start when we have one full period
    snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, frames);

    // Wake up when we have enough frames to process
    snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, frames);
    
    if ((pcm = snd_pcm_sw_params(pcm_handle, swparams)) < 0) {
        std::cerr << "ERROR: Can't set software parameters. " << snd_strerror(pcm) << std::endl;
        return false;
    }

    // Get and print actual configuration
    snd_pcm_uframes_t actual_buffer_size;
    snd_pcm_uframes_t actual_period_size;
    unsigned int actual_rate;
    unsigned int period_time;
    snd_pcm_get_params(pcm_handle, &actual_buffer_size, &actual_period_size);
    snd_pcm_hw_params_get_rate(params, &actual_rate, &dir);
    snd_pcm_hw_params_get_period_time(params, &period_time, &dir);

    std::cout << "========= ALSA Configuration =========\n"
                << "Access: " << snd_pcm_access_name(SND_PCM_ACCESS_RW_INTERLEAVED) << "\n"
                << "Format: " << snd_pcm_format_name(actualFormat) << "\n"
                << "Sample Rate: " << actual_rate << " Hz\n"
                << "Channels: " << num_channels << "\n"
                << "Bit Depth: " << bit_depth << " bits\n"
                << "======================================\n"
                << "Buffer Size: " << actual_buffer_size * num_channels * (bit_depth/8) << " bytes"
                << " (" << actual_buffer_size << " frames)\n"
                << "Number of Periods in Buffer: " << (actual_buffer_size / actual_period_size) << " periods\n"
                << "Number of Frames per Period: " << actual_period_size << " frames\n"                    
                << "Bytes per Frame: " << num_channels * (bit_depth/8) << " bytes\n"
                << "======================================\n";
    return true;
}

bool alsaCapture::startCapture() {
    // Set capture volume to maximum
    snd_mixer_t *mixer;
    snd_mixer_elem_t *elem;
    
    if (snd_mixer_open(&mixer, 0) >= 0) {
        if (snd_mixer_attach(mixer, "hw:2") >= 0) {
            snd_mixer_selem_id_t *sid;
            snd_mixer_selem_id_alloca(&sid);
            snd_mixer_selem_id_set_index(sid, 0);
            snd_mixer_selem_id_set_name(sid, "Mic Capture Volume");
            
            if ((elem = snd_mixer_find_selem(mixer, sid)) != nullptr) {
                // Set to maximum volume
                long min, max;
                snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
                snd_mixer_selem_set_capture_volume_all(elem, max);
            }
        }
        snd_mixer_close(mixer);
    }
    // Turn off Auto Gain Control for consistent volume
    if (snd_mixer_open(&mixer, 0) >= 0) {
        if (snd_mixer_attach(mixer, "hw:2") >= 0) {
            snd_mixer_selem_id_t *sid;
            snd_mixer_selem_id_alloca(&sid);
            snd_mixer_selem_id_set_index(sid, 0);
            snd_mixer_selem_id_set_name(sid, "Auto Gain Control");
            
            if ((elem = snd_mixer_find_selem(mixer, sid)) != nullptr) {
                snd_mixer_selem_set_playback_switch_all(elem, 0);  // Turn off AGC
            }
        }
        snd_mixer_close(mixer);
    }

    logMessage("Successfully start capture.");
    return true;
}

bool alsaCapture::stopCapture() {
    snd_pcm_drain(pcm_handle);
    logMessage("Successfully stop capture.");
    return true;
}

bool alsaCapture::reset() {
    logMessage("Attempting to reset capture device.");

    // Stop capture and close handle
    stopCapture();

    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
        pcm_handle = nullptr;
        params = nullptr;  // params is invalidated when handle is closed
    }

    // Wait for device to settle
    usleep(500000);  // 500ms delay

    // Reinitialize with error checking
    int retries = 3;
    bool init_success = false;
    
    while (retries --> 0 && !init_success) {
        // Try to initialize
        if (initialize()) {
            init_success = true;
            break;
        }
        
        logMessage("Initialization attempt failed, retrying...");
        usleep(100000);  // 100ms between retries
    }
    
    if (!init_success) {
        logMessage("Failed to reinitialize device after multiple attempts");
        return false;
    }

    // Start capture with error checking
    if (!startCapture()) {
        logMessage("Failed to start capture during reset.");
        return false;
    }
    
    // Verify device state
    snd_pcm_state_t state = snd_pcm_state(pcm_handle);
    if (state != SND_PCM_STATE_RUNNING && state != SND_PCM_STATE_PREPARED) {
        logMessage("Device in incorrect state after reset: " + std::to_string(state));
        return false;
    }

    logMessage("Successfully reset capture.");
    return true;
}

int alsaCapture::readFrames(char* outbuffer, int outFrames) {
    static int overrun_count = 0;
    static auto last_overrun = std::chrono::steady_clock::now();
    
    // Check available frames and handle errors
    snd_pcm_sframes_t avail = snd_pcm_avail(pcm_handle);
    
    if (avail < 0) {
        // Handle overrun
        if (avail == -EPIPE) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_overrun);
            std::cerr << "Overrun #" << ++overrun_count 
                      << " occurred after " << duration.count() << "ms" 
                      << std::endl;
            last_overrun = now;
        }
        
        // Try to recover from error
        if ((avail = snd_pcm_recover(pcm_handle, avail, 0)) < 0) {
            std::cerr << "Recovery failed: " << snd_strerror(avail) << std::endl;
            return avail;
        }
        // Re-check available frames after recovery
        avail = snd_pcm_avail(pcm_handle);
    }

    // Read the frames
    int pcm = snd_pcm_readi(pcm_handle, buffer.data(), frames);
    if (pcm < 0) {
        std::cerr << "ERROR. Can't read: " << snd_strerror(pcm) << std::endl;
        return pcm;
    }

    // Calculate bytes to copy based on actual frames read
    size_t bytes_to_copy = pcm * num_channels * (bit_depth / 8);
    // std::cout << "Frames read: " << pcm << ", Bytes to copy: " << bytes_to_copy << std::endl;
    
    if (bytes_to_copy > 0) {
        // Verify output buffer size
        size_t max_copy = outFrames * num_channels * (bit_depth / 8);
        if (bytes_to_copy > max_copy) {
            std::cerr << "WARNING: Truncating output, buffer too small" << std::endl;
            bytes_to_copy = max_copy;
        }
        memcpy(outbuffer, buffer.data(), bytes_to_copy);
    }

    return pcm;
}

} // namespace alsa_rtsp
