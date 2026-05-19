#version 330

// Fog post-processing fragment shader
// Applied as a screen-space effect using depth information
// Since we lack a depth buffer in standard raylib render textures,
// this uses UV-based distance approximation and time-based animation

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;       // Scene color
uniform float fogDensity;
uniform vec3 fogColor;
uniform float time;
uniform vec2 resolution;

// Simple pseudo-random for fog noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec4 sceneColor = texture(texture0, fragTexCoord);

    // Radial distance from center (simulates depth for fixed camera)
    vec2 centered = fragTexCoord - 0.5;
    float radialDist = length(centered) * 2.0;

    // Depth-based fog using radial distance as approximation
    float fogFactor = 1.0 - exp(-fogDensity * radialDist * radialDist * 10.0);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Add animated fog noise for swirling effect
    vec2 noiseCoord = fragTexCoord * 3.0 + vec2(time * 0.1, time * 0.07);
    float fogNoise = noise(noiseCoord) * 0.3 + 0.7;
    fogFactor *= fogNoise;

    // Subtle time-based pulsing
    float pulse = 1.0 + 0.08 * sin(time * 0.4);
    fogFactor = min(fogFactor * pulse, 1.0);

    // Edges of screen get heavier fog (corridor walls feel closer)
    float edgeFog = smoothstep(0.3, 0.8, radialDist);
    fogFactor = max(fogFactor, edgeFog * 0.4);

    vec3 result = mix(sceneColor.rgb, fogColor, fogFactor);

    finalColor = vec4(result, sceneColor.a);
}
