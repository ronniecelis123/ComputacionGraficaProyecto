#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 WorldPos;
    vec2 TexCoords;
    mat3 TBN;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    vec3 N = normalize(mat3(model) * aNormal);

    // Ortonormaliza T respecto a N por estabilidad
    T = normalize(T - dot(T, N) * N);
    B = normalize(cross(N, T));

    vs_out.TBN = mat3(T, B, N);
    vec4 worldPos = model * vec4(aPos, 1.0);
    vs_out.WorldPos = worldPos.xyz;
    vs_out.TexCoords = aTexCoords;

    gl_Position = projection * view * worldPos;
}
