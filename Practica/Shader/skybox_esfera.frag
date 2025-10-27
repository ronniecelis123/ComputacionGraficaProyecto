#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D skyTexture; // Solo una textura
uniform vec3 tintColor;      // Color de tinte

void main()
{
    vec4 baseColor = texture(skyTexture, TexCoords);
    FragColor = vec4(baseColor.rgb * tintColor, baseColor.a); // Aplicar tinte
}