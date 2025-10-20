#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 WorldPos;
    vec2 TexCoords;
    mat3 TBN;
} fs_in;

struct PointLight {
    vec3 position;
    vec3 color;     // energía de la luz (linear)
    float intensity; // multiplicador opcional
};

#define MAX_POINT_LIGHTS 4
uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

uniform vec3 uCamPos;

// Texturas PBR
uniform sampler2D albedoMap;    // sRGB
uniform sampler2D normalMap;    // en espacio tangente (XYZ en [0..1])
uniform sampler2D metallicMap;  // R
uniform sampler2D roughnessMap; // R
uniform sampler2D aoMap;        // R

// === Utilidades ===
const float PI = 3.14159265359;

// Convierte sRGB a linear para la textura de albedo.
vec3 SRGBtoLINEAR(vec3 c) {
    // aprox. rápida
    return pow(c, vec3(2.2));
}

// Toma normal del normalMap y lo lleva a espacio mundo usando TBN.
vec3 getNormal()
{
    vec3 n = texture(normalMap, fs_in.TexCoords).xyz;
    n = n * 2.0 - 1.0; // de [0,1] a [-1,1]
    return normalize(fs_in.TBN * n);
}

// GGX/Trowbridge-Reitz NDF
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + 1e-7);
}

// Schlick-GGX para una dirección (micro-shadowing)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r*r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k + 1e-7);
}

// Smith (combina vistas y luz)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel de Schlick
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Fresnel Schlick con rugosidad (mejor para N graze)
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    // Lee parámetros del material
    vec3  albedo    = SRGBtoLINEAR(texture(albedoMap,    fs_in.TexCoords).rgb);
    float metallic  = clamp(texture(metallicMap,  fs_in.TexCoords).r, 0.0, 1.0);
    float roughness = clamp(texture(roughnessMap, fs_in.TexCoords).r, 0.04, 1.0); // evita 0
    float ao        = clamp(texture(aoMap,        fs_in.TexCoords).r, 0.0, 1.0);

    vec3 N = getNormal();
    vec3 V = normalize(uCamPos - fs_in.WorldPos);

    // F0: reflectancia base en normal incidence
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Iluminación directa (suma de point lights)
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < uNumPointLights; ++i)
    {
        vec3 L = normalize(uPointLights[i].position - fs_in.WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(uPointLights[i].position - fs_in.WorldPos);
        float attenuation = 1.0 / (distance * distance); // inverse square
        vec3 radiance     = uPointLights[i].color * uPointLights[i].intensity * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denom       = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-7;
        vec3  specular    = numerator / denom;

        // kS = F; kD = (1 - kS) * (1 - metallic)
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Luz ambiente (AO simple; para IBL usar irradiance + prefiltered env + LUT)
    vec3 ambient = ao * albedo * 0.03;

    vec3 color = ambient + Lo;

    // HDR -> tone map (Reinhard simple) y gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); // gamma 2.2

    FragColor = vec4(color, 1.0);
}
