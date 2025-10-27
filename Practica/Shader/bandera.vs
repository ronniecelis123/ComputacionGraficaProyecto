#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform float time;

// --- Parámetros de la Onda ---
const float AMPLITUDE = 0.5;
const float FREQUENCY = 4.0;
const float SPEED = 3.0;

void main()
{
    vec3 newPos = aPos;
    vec3 newNormal = aNormal;
    
    // Asumimos que la bandera está "pegada" al asta en x=0
    // (Esto puede necesitar ajuste si tu modelo no está en el origen)
    if (aPos.x > 0.01) 
    {
        // 1. Calcular la Posición de la Onda (eje Z)
        float wave = AMPLITUDE * sin(aPos.x * FREQUENCY + time * SPEED);
        newPos = aPos + vec3(0.0, 0.0, wave);
        
        // 2. Recalcular la Normal
        float derivative = AMPLITUDE * FREQUENCY * cos(aPos.x * FREQUENCY + time * SPEED);
        
        // === LA LÍNEA CORREGIDA ===
        // La normal correcta (tangente en X cruzada con tangente en Y)
        newNormal = normalize(vec3(-derivative, 0.0, 1.0));
    }

    FragPos = vec3(model * vec4(newPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * newNormal;
    TexCoords = aTexCoords;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}