#version 330 core
layout (location = 0) in vec3 position; // Usamos 'position' de tu lighting.vs
layout (location = 2) in vec2 texCoords; // Usamos 'texCoords' de tu lighting.vs

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = texCoords;
    mat4 viewNoTranslation = mat4(mat3(view));
    vec4 pos = projection * viewNoTranslation * model * vec4(position, 1.0);
    gl_Position = pos.xyww;
}