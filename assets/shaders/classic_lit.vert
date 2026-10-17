#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vColor;
out float vViewDistance;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vec4 viewPos  = uView * worldPos;
    vWorldPos     = worldPos.xyz;
    vNormal       = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vTexCoord     = aTexCoord;
    vColor        = aColor;
    vViewDistance = length(viewPos.xyz);
    gl_Position   = uProjection * viewPos;
}
