#version 330 core

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in mat3 TBN;

out vec4 FragColor;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;

uniform vec3 albedoColor; // fallback
uniform vec3 lightPositions[1];
uniform vec3 lightColors[1];
uniform float lightIntensity;

void main()
{
    vec3 albedo = texture(albedoMap, TexCoords).rgb;
    if (length(albedo) < 0.01)
        albedo = albedoColor;

    float roughness = texture(roughnessMap, TexCoords).r;
    float metallic = texture(metallicMap, TexCoords).r;

    vec3 N = texture(normalMap, TexCoords).rgb;
    N = normalize(N * 2.0 - 1.0);
    N = normalize(TBN * N);

    vec3 L = normalize(lightPositions[0] - FragPos);
    float diff = max(dot(N, L), 0.0);
    vec3 color = albedo * diff * lightIntensity;

    FragColor = vec4(color, 1.0);
}
