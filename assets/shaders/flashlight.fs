#version 330

// Flashlight fragment shader - spotlight cone + distance attenuation + fog
// Applied per-object during 3D rendering

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
    // Base object color from vertex color
    vec4 baseColor = fragColor;

    // ---- Flashlight Spotlight ----
    vec3 toFrag = fragPosition - playerPos;
    float dist = length(toFrag);
    vec3 dirToPlayer = normalize(toFrag);

    // Spotlight cone: cosine of angle between light direction and fragment direction
    float cosAngle = dot(-dirToPlayer, normalize(playerDir));

    // Cone parameters (inner = sharper, outer = softer edge)
    float innerCutoff = 0.82;   // ~35 degrees half-angle
    float outerCutoff = 0.55;   // ~57 degrees half-angle
    float spotlight = smoothstep(outerCutoff, innerCutoff, cosAngle);

    // Distance attenuation (quadratic falloff)
    float attenuation = 1.0 / (1.0 + 0.07 * dist + 0.017 * dist * dist);

    // Combine: ambient + spotlight contribution
    float light = ambientStrength + spotlight * attenuation * flickerIntensity;

    // Slight warm tint for flashlight
    vec3 lightColor = mix(vec3(1.0), vec3(1.0, 0.95, 0.85), spotlight * 0.5);

    vec3 litColor = baseColor.rgb * light * lightColor;

    // ---- Distance Fog ----
    float fogFactor = 1.0 - exp(-fogDensity * dist * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Animate fog slightly for eerie movement
    float fogAnim = 1.0 + 0.15 * sin(time * 0.5 + dist * 0.3);
    fogFactor = min(fogFactor * fogAnim, 1.0);

    litColor = mix(litColor, fogColor, fogFactor);

    finalColor = vec4(litColor, baseColor.a);
}
