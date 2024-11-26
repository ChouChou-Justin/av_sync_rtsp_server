#include <csignal>
#include <iostream>
#include "unified_rtsp_server_manager.h"
#include "constants.h"
#include "logger.h"

// Global flag for clean shutdown
static char volatile shouldExit = 0;

// Signal handler
static void sigintHandler(int sig) {
    shouldExit = 1;
}

int main(int argc, char** argv) {
    // Set up signal handling
    signal(SIGINT, sigintHandler);
    signal(SIGTERM, sigintHandler);

    // Create basic usage environment
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    try {
        // Initialize video capture
        v4l2Capture* videoCapture = new v4l2Capture(VIDEO_DEVICE);
        if (!videoCapture->initialize()) {
            logMessage("Failed to initialize video capture");
            delete videoCapture;
            env->reclaim();
            delete scheduler;
            return -1;
        }
        logMessage("Successfully initialize video capture.");

        // Initialize audio capture
        alsa_rtsp::alsaCapture* audioCapture = new alsa_rtsp::alsaCapture(
            AUDIO_DEVICE,
            AUDIO_SAMPLE_RATE,
            AUDIO_CHANNELS,
            AUDIO_BIT_DEPTH
        );
        if (!audioCapture->initialize()) {
            logMessage("Failed to initialize audio capture");
            delete videoCapture;
            delete audioCapture;
            env->reclaim();
            delete scheduler;
            return -1;
        }
        logMessage("Successfully initialize audio capture.");

        // Start captures
        if (!videoCapture->startCapture()) {
            logMessage("Failed to start video capture");
            delete videoCapture;
            delete audioCapture;
            env->reclaim();
            delete scheduler;
            return -1;
        }
        if (!audioCapture->startCapture()) {
            logMessage("Failed to start audio capture");
            videoCapture->stopCapture();
            delete videoCapture;
            delete audioCapture;
            env->reclaim();
            delete scheduler;
            return -1;
        }

        // Create and initialize RTSP server
        UnifiedRTSPServerManager* serverManager = new UnifiedRTSPServerManager(
            env, videoCapture, audioCapture, DEFAULT_RTSP_PORT
        );

        if (!serverManager->initialize()) {
            logMessage("Failed to initialize server manager");
            videoCapture->stopCapture();
            audioCapture->stopCapture();
            delete serverManager;
            delete videoCapture;
            delete audioCapture;
            env->reclaim();
            delete scheduler;
            return -1;
        }

        logMessage("Successfully initialize RTSP server.");
        logMessage("Use Ctrl-C to exit.");
        logMessage("===========================================================");

        // Run the event loop
        serverManager->runEventLoop(&shouldExit);

        // Cleanup
        serverManager->cleanup();
        videoCapture->stopCapture();
        audioCapture->stopCapture();
        delete serverManager;
        delete videoCapture;
        delete audioCapture;
        logMessage("Successfully clean up resources.");

    } catch (const std::exception& e) {
        logMessage("Exception occurred: " + std::string(e.what()));
    }

    // Final cleanup
    env->reclaim();
    delete scheduler;

    logMessage("Successfully shutdown server.");
    return 0;
}
