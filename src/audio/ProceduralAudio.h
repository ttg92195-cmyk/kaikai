#pragma once
#include "raylib.h"
#include <vector>

namespace ProceduralAudio {
    // Generate audio waves programmatically
    Wave generateHeartbeat(float bpm);
    Wave generateFootstep();
    Wave generateDoorCreak();
    Wave generateWhisper(float duration);
    Wave generateStatic(float duration);
    Wave generateScream(); // for jumpscare
    Wave generateWind();
    Wave generateDripping();
    
    // Helper
    std::vector<float> generateSineWave(float frequency, float duration, float sampleRate = 44100.0f);
    std::vector<float> generateWhiteNoise(float duration, float sampleRate = 44100.0f);
    std::vector<float> mixWaves(const std::vector<float>& a, const std::vector<float>& b, float mix);
}
