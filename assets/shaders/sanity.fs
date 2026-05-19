#version 330

// Sanity post-processing fragment shader
// Vignette, color distortion, noise/static, chromatic aberration
// All effects intensify as sanity decreases (sanity: 100=full, 0=insane)

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;       // Scene color
uniform float sanity;             // 0.0 to 100.0
uniform float time;
uniform vec2 resolution;

// ---- Utility functions ----

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(93.9898, 67.345))) * 23421.631);
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

// ---- Vignette ----
float vignette(vec2 uv, float intensity) {
    vec2 center = uv - 0.5;
    float dist = length(center);
    // Stronger vignette at low sanity
    float radius = mix(0.9, 0.35, intensity);
    float softness = mix(0.6, 0.15, intensity);
    return smoothstep(radius, radius - softness, dist);
}

// ---- Chromatic Aberration ----
vec3 chromaticAberration(sampler2D tex, vec2 uv, float amount) {
    vec2 dir = uv - 0.5;
    float dist = length(dir);
    vec2 offset = normalize(dir + 0.001) * amount * dist;

    float r = texture(tex, uv + offset * 1.5).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - offset * 1.5).b;

    return vec3(r, g, b);
}

// ---- Screen Shake UV Offset ----
vec2 screenShake(vec2 uv, float intensity, float t) {
    float shakeX = sin(t * 23.7) * cos(t * 17.3) * intensity;
    float shakeY = cos(t * 19.1) * sin(t * 31.4) * intensity;
    // Sudden jerks
    shakeX += sin(t * 47.0) * step(0.97, hash(vec2(floor(t * 3.0), 0.0))) * intensity * 5.0;
    shakeY += cos(t * 53.0) * step(0.95, hash(vec2(floor(t * 3.0), 1.0))) * intensity * 5.0;
    return uv + vec2(shakeX, shakeY) * 0.01;
}

void main() {
    // Insanity factor: 0 when sane (100), 1 when insane (0)
    float insanity = 1.0 - clamp(sanity / 100.0, 0.0, 1.0);

    // Early out if sanity is high - minimal effects
    if (insanity < 0.01) {
        finalColor = texture(texture0, fragTexCoord);
        return;
    }

    // ---- UV with screen shake ----
    vec2 uv = fragTexCoord;
    if (insanity > 0.3) {
        uv = screenShake(uv, insanity * 0.5, time);
        uv = clamp(uv, 0.0, 1.0);
    }

    // ---- Chromatic Aberration (starts at sanity ~60) ----
    vec3 color;
    if (insanity > 0.4) {
        float aberrationAmount = (insanity - 0.4) * 0.025;
        color = chromaticAberration(texture0, uv, aberrationAmount);
    } else {
        color = texture(texture0, uv).rgb;
    }

    // ---- Color Distortion: Red shift at low sanity ----
    if (insanity > 0.2) {
        float redShift = (insanity - 0.2) * 0.4;
        color.r = mix(color.r, color.r * 1.5 + 0.05, redShift);
        color.g = mix(color.g, color.g * 0.8, redShift * 0.5);
        color.b = mix(color.b, color.b * 0.7, redShift * 0.5);
    }

    // ---- Vignette (always present, intensifies with insanity) ----
    float vig = vignette(fragTexCoord, insanity);
    color *= mix(1.0, vig, 0.3 + insanity * 0.7);

    // ---- Visual Noise / Static (at sanity < 50) ----
    if (insanity > 0.5) {
        float noiseIntensity = (insanity - 0.5) * 2.0; // 0 to 1

        // Scan lines
        float scanline = sin(fragTexCoord.y * resolution.y * 1.5 + time * 2.0) * 0.5 + 0.5;
        scanline = pow(scanline, 8.0);
        color = mix(color, color * 0.6, scanline * noiseIntensity * 0.5);

        // Random noise grain
        float grain = hash(fragTexCoord * resolution + vec2(time * 100.0, time * 77.0));
        float grainIntensity = noiseIntensity * 0.15;
        color = mix(color, vec3(grain), grainIntensity);

        // Occasional full-screen static flash
        float staticTrigger = hash(vec2(floor(time * 4.0), 0.0));
        if (staticTrigger > (1.0 - noiseIntensity * 0.05)) {
            float staticNoise = hash(fragTexCoord * resolution + vec2(time * 1000.0));
            color = mix(color, vec3(staticNoise), noiseIntensity * 0.6);
        }
    }

    // ---- Hallucination color flashes (at very low sanity < 20) ----
    if (insanity > 0.8) {
        float flashIntensity = (insanity - 0.8) * 5.0;
        float flash = sin(time * 3.0) * 0.5 + 0.5;
        flash *= step(0.7, hash(vec2(floor(time * 2.0), 42.0)));
        color = mix(color, vec3(0.6, 0.0, 0.0), flash * flashIntensity * 0.3);
    }

    // ---- Brightness pulsing (subtle, at low sanity) ----
    if (insanity > 0.6) {
        float pulseIntensity = (insanity - 0.6) * 0.3;
        float pulse = sin(time * 1.5) * 0.5 + 0.5;
        color *= 1.0 - pulseIntensity * pulse * 0.2;
    }

    // Final output
    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
