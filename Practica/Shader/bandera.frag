#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// ===================================================
// ESTAS ESTRUCTURAS Y UNIFORMS AHORA COINCIDEN CON TU main.cpp
// ===================================================

// --- Estructuras ---
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
}; 

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       
};

// --- Tamaños de los Arreglos (¡DEBEN COINCIDIR CON main.cpp!) ---
// main.cpp usa: NUM_POINT_LIGHTS (29) y NUM_SPOT_LIGHTS (9)
const int MAX_POINT_LIGHTS = 29;
const int MAX_SPOT_LIGHTS = 9;

// --- Uniforms ---
uniform vec3 viewPos;
uniform DirLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform Material material;

// --- Prototipos de Función ---
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec4 texColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 texColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 texColor);

void main()
{    
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // Obtenemos el color de la textura (del logo de la bandera)
    vec4 texColor = texture(material.diffuse, TexCoords);
    
    // --- Cálculos de Luz ---
    vec3 result = CalcDirLight(dirLight, norm, viewDir, texColor);
    
    for(int i = 0; i < MAX_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
        
    for(int i = 0; i < MAX_SPOT_LIGHTS; i++)
        result += CalcSpotLight(spotLights[i], norm, FragPos, viewDir, texColor);
    
    // El color final es el color de la textura multiplicado por toda la luz calculada
    FragColor = vec4(result, 1.0) * texColor;
}


// --- Definiciones de Función ---
// (Modificadas para usar el 'texColor' que les pasamos)

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec4 texColor)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    vec3 ambient = light.ambient * texColor.rgb;
    vec3 diffuse = light.diffuse * diff * texColor.rgb;
    // El especular usa el sampler 'specular' o es blanco si no hay
    vec3 specular = light.specular * spec * texture(material.specular, TexCoords).rgb; 
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 texColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    vec3 ambient = light.ambient * texColor.rgb;
    vec3 diffuse = light.diffuse * diff * texColor.rgb;
    vec3 specular = light.specular * spec * texture(material.specular, TexCoords).rgb;
    
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 texColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    if(theta > light.cutOff) // Si está dentro del cono interno
    {
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        float distance = length(light.position - fragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
        
        vec3 ambient = light.ambient * texColor.rgb;
        vec3 diffuse = light.diffuse * diff * texColor.rgb;
        vec3 specular = light.specular * spec * texture(material.specular, TexCoords).rgb;
        
        return (ambient + diffuse + specular) * attenuation * intensity;
    }
    return vec3(0.0); // Fuera del cono
}