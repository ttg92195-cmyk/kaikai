#include "ShaderCode.h"

namespace ShaderCode {

// ============================================================================
// FLASHLIGHT VERTEX SHADER
// ============================================================================
const char* getFlashlightVertexShader() {
    return R"GLSL(
#version 330

// Vertex attributes provided by raylib
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Outputs to fragment shader
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;

// Uniforms provided by raylib
uniform mat4 mvp;
uniform mat4 matModel;

void main() {
    // World-space position for lighting calculations
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragNormal = mat3(matModel) * vertexNormal;
    fragColor = vertexColor;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";
}

// ============================================================================
// FLASHLIGHT FRAGMENT SHADER
// ============================================================================
const char* getFlashlightFragmentShader() {
    return R"GLSL(
#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

out vec4 finalColor;

uniform vec3 playerPos;
uniform vec3 playerDir;
uniform float flickerIntensity;
uniform float ambientStrength;
uniform float fogDensity;
uniform vec3 fogColor;
uniform float time;

void main() {
    vec4 baseColor = fragColor;

    // ---- Flashlight Spotlight ----
    vec3 toFrag = fragPosition - playerPos;
    float dist = length(toFrag);
    vec3 dirToPlayer = normalize(toFrag);

    float cosAngle = dot(-dirToPlayer, normalize(playerDir));

    float innerCutoff = 0.82;
    float outerCutoff = 0.55;
    float spotlight = smoothstep(outerCutoff, innerCutoff, cosAngle);

    // Distance attenuation (quadratic falloff)
    float attenuation = 1.0 / (1.0 + 0.07 * dist + 0.017 * dist * dist);

    // Combined lighting
    float light = ambientStrength + spotlight * attenuation * flickerIntensity;

    // Warm tint for flashlight
    vec3 lightColor = mix(vec3(1.0), vec3(1.0, 0.95, 0.85), spotlight * 0.5);
    vec3 litColor = baseColor.rgb * light * lightColor;

    // ---- Distance Fog ----
    float fogFactor = 1.0 - exp(-fogDensity * dist * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    float fogAnim = 1.0 + 0.15 * sin(time * 0.5 + dist * 0.3);
    fogFactor = min(fogFactor * fogAnim, 1.0);

    litColor = mix(litColor, fogColor, fogFactor);

    finalColor = vec4(litColor, baseColor.a);
}
)GLSL";
}

// ============================================================================
// FOG VERTEX SHADER (post-processing)
// ============================================================================
const char* getFogVertexShader() {
    return R"GLSL(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragTexCoord;

uniform mat4 mvp;

void main() {
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";
}

// ============================================================================
// FOG FRAGMENT SHADER (post-processing)
// ============================================================================
const char* getFogFragmentShader() {
    return R"GLSL(
#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float fogDensity;
uniform vec3 fogColor;
uniform float time;
uniform vec2 resolution;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec4 sceneColor = texture(texture0, fragTexCoord);

    vec2 centered = fragTexCoord - 0.5;
    float radialDist = length(centered) * 2.0;

    float fogFactor = 1.0 - exp(-fogDensity * radialDist * radialDist * 10.0);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec2 noiseCoord = fragTexCoord * 3.0 + vec2(time * 0.1, time * 0.07);
    float fogNoise = noise(noiseCoord) * 0.3 + 0.7;
    fogFactor *= fogNoise;

    float pulse = 1.0 + 0.08 * sin(time * 0.4);
    fogFactor = min(fogFactor * pulse, 1.0);

    float edgeFog = smoothstep(0.3, 0.8, radialDist);
    fogFactor = max(fogFactor, edgeFog * 0.4);

    vec3 result = mix(sceneColor.rgb, fogColor, fogFactor);

    finalColor = vec4(result, sceneColor.a);
}
)GLSL";
}

// ============================================================================
// SANITY VERTEX SHADER (post-processing)
// ============================================================================
const char* getSanityVertexShader() {
    return R"GLSL(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragTexCoord;

uniform mat4 mvp;

void main() {
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";
}

// ============================================================================
// SANITY FRAGMENT SHADER (post-processing)
// ============================================================================
const char* getSanityFragmentShader() {
    return R"GLSL(
#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float sanity;
uniform float time;
uniform vec2 resolution;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float vignette(vec2 uv, float intensity) {
    vec2 center = uv - 0.5;
    float dist = length(center);
    float radius = mix(0.9, 0.35, intensity);
    float softness = mix(0.6, 0.15, intensity);
    return smoothstep(radius, radius - softness, dist);
}

vec3 chromaticAberration(sampler2D tex, vec2 uv, float amount) {
    vec2 dir = uv - 0.5;
    float dist = length(dir);
    vec2 offset = normalize(dir + 0.001) * amount * dist;

    float r = texture(tex, uv + offset * 1.5).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - offset * 1.5).b;

    return vec3(r, g, b);
}

vec2 screenShake(vec2 uv, float intensity, float t) {
    float shakeX = sin(t * 23.7) * cos(t * 17.3) * intensity;
    float shakeY = cos(t * 19.1) * sin(t * 31.4) * intensity;
    shakeX += sin(t * 47.0) * step(0.97, hash(vec2(floor(t * 3.0), 0.0))) * intensity * 5.0;
    shakeY += cos(t * 53.0) * step(0.95, hash(vec2(floor(t * 3.0), 1.0))) * intensity * 5.0;
    return uv + vec2(shakeX, shakeY) * 0.01;
}

void main() {
    float insanity = 1.0 - clamp(sanity / 100.0, 0.0, 1.0);

    if (insanity < 0.01) {
        finalColor = texture(texture0, fragTexCoord);
        return;
    }

    vec2 uv = fragTexCoord;
    if (insanity > 0.3) {
        uv = screenShake(uv, insanity * 0.5, time);
        uv = clamp(uv, 0.0, 1.0);
    }

    vec3 color;
    if (insanity > 0.4) {
        float aberrationAmount = (insanity - 0.4) * 0.025;
        color = chromaticAberration(texture0, uv, aberrationAmount);
    } else {
        color = texture(texture0, uv).rgb;
    }

    if (insanity > 0.2) {
        float redShift = (insanity - 0.2) * 0.4;
        color.r = mix(color.r, color.r * 1.5 + 0.05, redShift);
        color.g = mix(color.g, color.g * 0.8, redShift * 0.5);
        color.b = mix(color.b, color.b * 0.7, redShift * 0.5);
    }

    float vig = vignette(fragTexCoord, insanity);
    color *= mix(1.0, vig, 0.3 + insanity * 0.7);

    if (insanity > 0.5) {
        float noiseIntensity = (insanity - 0.5) * 2.0;

        float scanline = sin(fragTexCoord.y * resolution.y * 1.5 + time * 2.0) * 0.5 + 0.5;
        scanline = pow(scanline, 8.0);
        color = mix(color, color * 0.6, scanline * noiseIntensity * 0.5);

        float grain = hash(fragTexCoord * resolution + vec2(time * 100.0, time * 77.0));
        float grainIntensity = noiseIntensity * 0.15;
        color = mix(color, vec3(grain), grainIntensity);

        float staticTrigger = hash(vec2(floor(time * 4.0), 0.0));
        if (staticTrigger > (1.0 - noiseIntensity * 0.05)) {
            float staticNoise = hash(fragTexCoord * resolution + vec2(time * 1000.0));
            color = mix(color, vec3(staticNoise), noiseIntensity * 0.6);
        }
    }

    if (insanity > 0.8) {
        float flashIntensity = (insanity - 0.8) * 5.0;
        float flash = sin(time * 3.0) * 0.5 + 0.5;
        flash *= step(0.7, hash(vec2(floor(time * 2.0), 42.0)));
        color = mix(color, vec3(0.6, 0.0, 0.0), flash * flashIntensity * 0.3);
    }

    if (insanity > 0.6) {
        float pulseIntensity = (insanity - 0.6) * 0.3;
        float pulse = sin(time * 1.5) * 0.5 + 0.5;
        color *= 1.0 - pulseIntensity * pulse * 0.2;
    }

    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)GLSL";
}

} // namespace ShaderCode
