#pragma once

// Contains GLSL shader code as C++ string literals
// Used by the Renderer to load shaders from memory at runtime

namespace ShaderCode {

    // ---- Flashlight Vertex Shader ----
    // Transforms vertices and passes world-space position for spotlight calculations
    const char* getFlashlightVertexShader();

    // ---- Flashlight Fragment Shader ----
    // Spotlight cone from player position, distance attenuation, fog integration
    const char* getFlashlightFragmentShader();

    // ---- Fog Vertex Shader ----
    // Simple pass-through for post-processing fullscreen quad
    const char* getFogVertexShader();

    // ---- Fog Fragment Shader ----
    // Screen-space fog with noise animation and edge emphasis
    const char* getFogFragmentShader();

    // ---- Sanity Vertex Shader ----
    // Simple pass-through for post-processing fullscreen quad
    const char* getSanityVertexShader();

    // ---- Sanity Fragment Shader ----
    // Vignette, chromatic aberration, noise, color distortion, screen shake
    const char* getSanityFragmentShader();

} // namespace ShaderCode
