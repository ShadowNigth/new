#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vColor;
in float vViewDistance;

out vec4 FragColor;

uniform sampler2D uAlbedoTex;
uniform vec3 uTintColor;

// Directional & ambient light
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;
uniform float uDirLightIntensity;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;

// Up to 4 classic point lights
const int MAX_POINT_LIGHTS = 4;
uniform int uPointLightCount;
uniform vec3 uPointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColors[MAX_POINT_LIGHTS];
uniform float uPointLightIntensities[MAX_POINT_LIGHTS];
uniform float uPointLightRadii[MAX_POINT_LIGHTS];

// Classic linear distance fog
uniform int uFogEnabled;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

void main() {
    vec4 texSample = texture(uAlbedoTex, vTexCoord);
    vec3 baseAlbedo = texSample.rgb * vColor * uTintColor;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDirLightDirection);
    float ndotl = max(dot(N, L), 0.0);

    vec3 lighting = uAmbientColor * uAmbientIntensity +
                    uDirLightColor * (uDirLightIntensity * ndotl);

    for (int i = 0; i < uPointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 toLight = uPointLightPositions[i] - vWorldPos;
        float dist = length(toLight);
        float radius = max(uPointLightRadii[i], 0.001);
        if (dist < radius) {
            vec3 pDir = toLight / max(dist, 0.0001);
            float pNdotL = max(dot(N, pDir), 0.0);
            float atten = 1.0 - (dist / radius);
            atten = atten * atten;
            lighting += uPointLightColors[i] * (uPointLightIntensities[i] * pNdotL * atten);
        }
    }

    vec3 litColor = baseAlbedo * lighting;

    if (uFogEnabled != 0) {
        float fogSpan = max(uFogEnd - uFogStart, 0.001);
        float fogFactor = clamp((uFogEnd - vViewDistance) / fogSpan, 0.0, 1.0);
        litColor = mix(uFogColor, litColor, fogFactor);
    }

    FragColor = vec4(litColor, texSample.a);
}
