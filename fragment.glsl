#version 330 core
in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

layout (location = 0) out vec4 FragPosition;
layout (location = 1) out vec4 FragNormal;
layout (location = 2) out vec4 FragAlbedo;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.5) discard;
    
    FragPosition = vec4(vFragPos, 1.0);
    FragNormal = vec4(normalize(vNormal), 1.0);
    FragAlbedo = texColor;
}
