#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
// Tangent opțional
layout(location = 3) in vec3 tangent;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;
out mat3 TBN;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
    FragPos = vec3(modelMatrix * vec4(position, 1.0));
    TexCoords = texCoords;

    vec3 N = normalize(mat3(modelMatrix) * normal);
    vec3 T = normalize(mat3(modelMatrix) * tangent);
    vec3 B = cross(N, T);

    // fallback simplu dacă tangenta e nulă
    if (length(T) < 0.01)
    {
        T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
        B = cross(N, T);
    }

    TBN = mat3(T, B, N);
    Normal = N;

    gl_Position = projectionMatrix * viewMatrix * vec4(FragPos, 1.0);
}
