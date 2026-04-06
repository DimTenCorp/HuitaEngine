#version 330 core
in vec3 vNormal;
in vec3 vPos;
in vec2 vTexCoord;
in vec4 vPosLightSpace;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform sampler2D shadowMap;
uniform vec3 viewPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform bool shadowsEnabled;

void main() {
    // Просто выводим текстуру без освещения для отладки
    vec4 texColor = texture(uTexture, vTexCoord);
    if(texColor.a < 0.5) discard;
    
    FragColor = vec4(texColor.rgb, 1.0);
}
