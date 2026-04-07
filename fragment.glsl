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

float calcShadow(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0) return 0.0;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.0005 * (1.0 - dot(norm, lightDirNorm)), 0.0001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    
    return shadow / 9.0;
}

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if(texColor.a < 0.5) discard;
    
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(-sunDir);
    
    // Ambient
    vec3 ambient = texColor.rgb * 0.15;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Shadow
    float shadow = shadowsEnabled ? calcShadow(vPosLightSpace) : 0.0;
    
    // Result
    vec3 diffuse = texColor.rgb * sunColor * diff * sunIntensity * (1.0 - shadow);
    vec3 result = ambient + diffuse;
    
    // Fog
    float dist = length(viewPos - vPos);
    float fog = clamp(exp(-dist * 0.03), 0.0, 1.0);
    result = mix(vec3(0.05, 0.05, 0.1), result, fog);
    
    // Gamma
    result = pow(result, vec3(1.0/2.2));
    
    FragColor = vec4(result, texColor.a);
}