#include <iostream>
#include <cmath>
#include <cstdio>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Define STB_IMAGE_IMPLEMENTATION only once in your project (here is fine)

#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assuming SOIL2 is used elsewhere? If not, can be removed.
// #include "SOIL2/SOIL2.h"

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// --- Prototipos ---
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void DoMovement();
GLuint loadTexture(const char* path); // <-- Helper para cargar texturas

// --- Ventana ---
const GLuint WIDTH = 800, HEIGHT = 600;
int SCREEN_WIDTH, SCREEN_HEIGHT;

// --- Cámara ---
Camera  camera(glm::vec3(0.0f, 0.0f, 3.0f));
GLfloat lastX = WIDTH * 0.5f, lastY = HEIGHT * 0.5f;
bool keys[1024]{};
bool firstMouse = true;

// --- Movimiento ---
float g_MoveBoost = 40.0f;
float g_RunBoost = 2.5f;

// --- Brillo global (será animado) ---
// Esta variable AHORA solo controla el SOL (DirLight)
float globalLightBoost = 3.5f;

// --- Tiempo ---
GLfloat deltaTime = 0.0f, lastFrame = 0.0f;

// --- Proyección global ---
glm::mat4 g_Projection(1.0f);


// ===================================
//  VARIABLES - CICLO DÍA/NOCHE
// ===================================
float g_TimeOfDay = 0.0f; // 0.0 = Mediodía, 0.5 = Medianoche, 1.0 = Mediodía
const float CYCLE_SPEED = 0.03f; // Velocidad del ciclo
const float DAY_BOOST = 5.0f;    // Brillo máximo (Sol)
const float NIGHT_BOOST = 1.0f;  // Brillo mínimo (Sol)

// ==========================================================
//  AJUSTE CLAVE:
//  MAX_LOCAL_INTENSITY es ahora 2.0f, igual al
//  globalLightBoost de tu CÓDIGO 1 (el estático).
// ==========================================================
const float MAX_LOCAL_INTENSITY = 2.5f;  // <-- (Era 1.0f)
const float MIN_LOCAL_INTENSITY = 0.1f;  // Las luces locales casi apagadas de día

GLuint g_SkyTextureDay; // Solo necesitamos la textura de día
// ===================================

// =====================
//  BLENDER -> OPENGL
// =====================
// (Tu código de luces de Blender sin cambios)
static inline glm::vec3 FromBlender(const glm::vec3& b) {
    return glm::vec3(b.x, b.z, -b.y);
}
struct BL { glm::vec3 pos; float energy; };
static const BL kBlenderLights[] = { /* ... (tus 29 luces) ... */
    { {  7.506f,  -1.564f, 33.050f }, 1000.0f }, { {  7.506f,  18.210f, 33.050f }, 1000.0f },
    { {  7.506f, -23.389f, 33.050f }, 1000.0f }, { {  7.506f,  39.512f, 33.050f }, 1000.0f },
    { {  7.506f,  53.020f, 33.050f }, 1000.0f }, { { -29.320f, 53.020f,  33.050f }, 1000.0f },
    { {  7.506f,  10.228f,  9.497f }, 1000.0f }, { { 25.063f,  10.228f, 21.128f }, 100.0f  },
    { { -60.455f,   0.000f, 14.382f }, 300.0f  }, { { -29.890f,   0.000f, 14.382f }, 300.0f  },
    { { -60.455f,  32.697f, 14.382f }, 300.0f  }, { { -60.455f, -14.834f, 14.382f }, 300.0f  },
    { { -88.231f,   0.000f, 10.847f }, 1000.0f }, { { -88.231f,  20.268f, 10.847f }, 1000.0f },
    { { -88.231f,  44.557f, 11.160f }, 1000.0f }, { { -88.231f, -21.363f, 10.847f }, 1000.0f },
    { { -88.231f, -42.799f, 10.847f }, 1000.0f }, { { -63.232f, -42.799f, 11.879f }, 500.0f  },
    { { -33.888f, -42.799f, 11.879f }, 300.0f  }, { { -20.423f, -42.799f, 11.879f }, 300.0f  },
    { { -83.964f,  56.018f, 11.160f }, 300.0f  }, { { -64.739f,  56.018f, 11.160f }, 300.0f  },
    { { -44.714f,  56.018f, 11.160f }, 300.0f  }, { { -22.686f,  56.018f, 11.160f }, 300.0f  },
    { { 15.258f, -27.267f, -3.3904f }, 1000.0f }, { { 15.258f,  38.584f, -3.3904f }, 3000.0f },
    { { -66.443f, 10.228f, -6.4713f }, 30000.0f }, { { -154.04f, -140.55f, 69.091f }, 30000.0f },
    { { -7.9162f,  168.05f, 149.2f  }, 2000.0f }
};
static const int NUM_POINT_LIGHTS = static_cast<int>(sizeof(kBlenderLights) / sizeof(kBlenderLights[0]));
glm::vec3 pointLightPositions[NUM_POINT_LIGHTS];
float     pointLightEnergy[NUM_POINT_LIGHTS];
struct BLSpot { glm::vec3 pos; glm::vec3 dir; float innerDeg; float outerDeg; float intensity; glm::vec3 color; };
static const BLSpot kBlenderSpots[] = { 
    { { -107.9f,   60.565f, 18.752f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -83.562f, 60.565f, 20.474f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -60.777f, 64.805f, 18.466f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -36.024f, 62.641f, 18.466f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -31.645f, 33.261f, 17.779f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { {  -46.790f, 33.261f, 17.779f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { { -113.410f, 33.261f, 17.779f }, {  0.0f,       0.0f,       -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { { -107.46f, -36.14f, 23.863f }, { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } },
    { { -112.78f, -36.14f, 23.863f }, { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } }
};
static const int NUM_SPOT_LIGHTS = static_cast<int>(sizeof(kBlenderSpots) / sizeof(kBlenderSpots[0]));
glm::vec3 gSpotPos[NUM_SPOT_LIGHTS];
glm::vec3 gSpotDir[NUM_SPOT_LIGHTS];
float     gSpotInnerCos[NUM_SPOT_LIGHTS];
float     gSpotOuterCos[NUM_SPOT_LIGHTS];
float     gSpotIntensity[NUM_SPOT_LIGHTS];
glm::vec3 gSpotColor[NUM_SPOT_LIGHTS];
static void InitConvertedLights() { /* ... (tu función sin cambios) ... */
    for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
        pointLightPositions[i] = FromBlender(kBlenderLights[i].pos);
        pointLightEnergy[i] = kBlenderLights[i].energy;
    }
    for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
        gSpotPos[i] = FromBlender(kBlenderSpots[i].pos);
        gSpotDir[i] = glm::normalize(FromBlender(kBlenderSpots[i].dir));
        gSpotInnerCos[i] = glm::cos(glm::radians(kBlenderSpots[i].innerDeg));
        gSpotOuterCos[i] = glm::cos(glm::radians(kBlenderSpots[i].outerDeg));
        gSpotIntensity[i] = kBlenderSpots[i].intensity;
        gSpotColor[i] = kBlenderSpots[i].color;
    }
}
// ===================================


int main() {
    // --- Init GLFW (sin cambios) ---
    if (!glfwInit()) { std::cout << "Failed to init GLFW\n"; return EXIT_FAILURE; }
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Previo 9. Ronie Celis", nullptr, nullptr);
    if (!window) { std::cout << "Failed to create GLFW window\n"; glfwTerminate(); return EXIT_FAILURE; }
    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glewExperimental = GL_TRUE;
    if (GLEW_OK != glewInit()) { std::cout << "Failed to initialize GLEW\n"; return EXIT_FAILURE; }

    // --- Info GL (sin cambios) ---
    auto safe_cstr = [](const GLubyte* s) { return s ? reinterpret_cast<const char*>(s) : "(null)"; };
    std::cout << "> Version: " << safe_cstr(glGetString(GL_VERSION)) << '\n';
    std::cout << "> Vendor: " << safe_cstr(glGetString(GL_VENDOR)) << '\n';
    std::cout << "> Renderer: " << safe_cstr(glGetString(GL_RENDERER)) << '\n';
    std::cout << "> SL Ver: " << safe_cstr(glGetString(GL_SHADING_LANGUAGE_VERSION)) << '\n';

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // ===================================
    //  CARGA DE SHADERS (sin cambios)
    // ===================================
    Shader lightingShader("Shader/lighting.vs", "Shader/lighting.frag");
    Shader flagShader("Shader/bandera.vs", "Shader/bandera.frag");
    Shader skyShader("Shader/skybox_esfera.vs", "Shader/skybox_esfera.frag"); // <-- Shader para la esfera


    // ===================================
    //  CARGA DE MODELOS Y TEXTURAS (sin cambios)
    // ===================================
    Model museo((char*)"Models/museo.obj");
    Model astaBandera((char*)"Models/bandera.obj");
    Model bandera((char*)"Models/banderaLogo.obj");
    Model esfera((char*)"Models/esfera.obj"); // <-- Tu esfera

    // Carga SOLO la textura de día
    // stbi_set_flip_vertically_on_load(true); // Descomenta si tu esfera se ve al revés
    g_SkyTextureDay = loadTexture("Models/dia.jpeg");
    // stbi_set_flip_vertically_on_load(false);

    // --- (Resto de Init sin cambios) ---
    InitConvertedLights();
    g_Projection = glm::perspective(
        camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT,
        0.5f,
        600.0f
    );

    // ===================================
    //  GAME LOOP (MODIFICADO)
    // ===================================
    while (!glfwWindowShouldClose(window)) {
        // --- Actualizar Tiempo ---
        GLfloat currentFrame = (GLfloat)glfwGetTime(); // <-- Declaración de currentFrame
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- Input y Movimiento ---
        glfwPollEvents();
        DoMovement();

        // --- Limpiar Pantalla ---
        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ==============================================================
        //  PASO 1: CALCULAR ANIMACIÓN DE DÍA/NOCHE
        // ==============================================================

        g_TimeOfDay = fmod(currentFrame * CYCLE_SPEED, 1.0f);
        float cosTime = cos(g_TimeOfDay * 2.0f * 3.14159f);
        float blendFactor = (cosTime + 1.0f) * 0.5f; // 1.0 = Day, 0.0 = Night

        // globalLightBoost ahora es SOLO para el SOL (DirLight)
        globalLightBoost = NIGHT_BOOST + (DAY_BOOST - NIGHT_BOOST) * blendFactor;

        // --- Calcular Tint Color (sin cambios) ---
        glm::vec3 dayColor(1.0f, 1.0f, 1.0f);      // Blanco
        glm::vec3 sunsetColor(1.0f, 0.7f, 0.4f);   // Naranja
        glm::vec3 nightColor(0.02f, 0.05f, 0.15f); // azul oscuro, visible, no tapa del todo


        glm::vec3 tintColor;
        if (blendFactor > 0.5f) { // Día a Atardecer
            float t = (blendFactor - 0.5f) * 2.0f;
            tintColor = glm::mix(sunsetColor, dayColor, t);
        }
        else { // Atardecer a Noche
            float t = blendFactor * 2.0f;
            tintColor = glm::mix(nightColor, sunsetColor, t);
        }

        // --- Calcular Intensidad de Luces Locales ---
        // ESTA ES LA VARIABLE CLAVE para luces puntuales y spots
        // Anima de MIN (0.1) a MAX (2.0)
        float localLightIntensity = MIN_LOCAL_INTENSITY + (MAX_LOCAL_INTENSITY - MIN_LOCAL_INTENSITY) * (1.0f - blendFactor);


        // ==============================================================
        //  PASO 2: DIBUJAR EL SKYBOX (ESFERA) - (sin cambios)
        // ==============================================================
        glDepthMask(GL_FALSE);

        glm::mat4 view = camera.GetViewMatrix(); // Obtenemos la vista una vez aqui
        glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
        glDepthFunc(GL_LEQUAL);
        GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullWasEnabled) glDisable(GL_CULL_FACE);

        skyShader.Use();

        glUniformMatrix4fv(glGetUniformLocation(skyShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTrans));
        glUniformMatrix4fv(glGetUniformLocation(skyShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(g_Projection));
        glm::mat4 modelSky = glm::mat4(1.0f);
        GLint modelSkyLoc = glGetUniformLocation(skyShader.Program, "model");
        if (modelSkyLoc >= 0) glUniformMatrix4fv(modelSkyLoc, 1, GL_FALSE, glm::value_ptr(modelSky));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_SkyTextureDay);
        glUniform1i(glGetUniformLocation(skyShader.Program, "skyTexture"), 0);

        glUniform3fv(glGetUniformLocation(skyShader.Program, "tintColor"), 1, glm::value_ptr(tintColor));

        esfera.Draw(skyShader);

        if (cullWasEnabled) glEnable(GL_CULL_FACE);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);


        // ==============================================================
        //  PASO 3: OBJETOS ESTÁTICOS (lightingShader)
        // ==============================================================
        lightingShader.Use();

        // Material (sin cambios)
        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.diffuse"), 0);
        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.specular"), 1);
        glUniform1f(glGetUniformLocation(lightingShader.Program, "material.shininess"), 16.0f);

        // Cámara (sin cambios)
        glUniform3f(glGetUniformLocation(lightingShader.Program, "viewPos"),
            camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);

        // Luz direccional (SOL) (Usa globalLightBoost animado, sin cambios)
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.ambient"),
            0.06f * globalLightBoost, 0.06f * globalLightBoost, 0.06f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.diffuse"),
            0.12f * globalLightBoost, 0.12f * globalLightBoost, 0.12f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.specular"),
            0.22f * globalLightBoost, 0.22f * globalLightBoost, 0.22f * globalLightBoost);

        // Helper para setear una puntual (sin cambios, aplica localLightIntensity)
        auto setPoint = [&](int i, glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
            float kc, float kl, float kq) {
                char name[64];
                std::snprintf(name, sizeof(name), "pointLights[%d].position", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name), pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

                // Aplicar intensidad local (que ahora anima de 0.1 a 2.0)
                amb *= localLightIntensity;
                dif *= localLightIntensity;
                spec *= localLightIntensity;

                std::snprintf(name, sizeof(name), "pointLights[%d].ambient", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name), amb.x, amb.y, amb.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].diffuse", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name), dif.x, dif.y, dif.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].specular", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name), spec.x, spec.y, spec.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].constant", i);
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kc);
                std::snprintf(name, sizeof(name), "pointLights[%d].linear", i);
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kl);
                std::snprintf(name, sizeof(name), "pointLights[%d].quadratic", i);
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kq);
            };

        // ==================================================
        //  AJUSTE: Seteo de las puntuales
        // ==================================================
        for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
            // Intensidades base (de Code 1)
            glm::vec3 amb(0.04f), dif(0.90f), spec(0.55f);
            float kc = 1.0f, kl = 0.045f, kq = 0.0075f;

            float energyBoost = pointLightEnergy[i] / 1000.0f;

            // NO multiplicamos por globalLightBoost (sol)
            // El 'setPoint' helper se encargará de multiplicar por 'localLightIntensity'
            amb *= (0.6f * 0.7f * energyBoost);
            dif *= 0.60f * energyBoost;
            spec *= 0.40f * energyBoost;

            // APLICAMOS LAS HEURÍSTICAS DE CODE 1 (que faltaban en Code 2)
            if (energyBoost > 1.5f) { kl = 0.028f; kq = 0.0019f; }

            if (pointLightPositions[i].z < 5.0f) {
                dif *= 0.70f; spec *= 0.50f; kl = 0.060f; kq = 0.0170f;
            }
            else if (pointLightPositions[i].z < 15.0f) {
                dif *= 0.90f;
            }
            // --- Fin de heurísticas ---

            setPoint(i, amb, dif, spec, kc, kl, kq);
        }

        // ==================================================
        //  AJUSTE: Spotlights
        // ==================================================
        for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
            char name[64];

            // NO multiplicamos por globalLightBoost (sol)
            // Multiplicamos por localLightIntensity (que anima de 0.1 a 2.0)
            glm::vec3 amb = 0.02f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 dif = 1.40f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 spe = 0.60f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];

            const float kc = 1.0f, kl = 0.045f, kq = 0.0075f; // (Igual que Code 1)

            std::snprintf(name, sizeof(name), "spotLights[%d].position", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name), gSpotPos[i].x, gSpotPos[i].y, gSpotPos[i].z);
            std::snprintf(name, sizeof(name), "spotLights[%d].direction", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name), gSpotDir[i].x, gSpotDir[i].y, gSpotDir[i].z);
            std::snprintf(name, sizeof(name), "spotLights[%d].cutOff", i);
            glUniform1f(glGetUniformLocation(lightingShader.Program, name), gSpotInnerCos[i]);
            std::snprintf(name, sizeof(name), "spotLights[%d].outerCutOff", i);
            glUniform1f(glGetUniformLocation(lightingShader.Program, name), gSpotOuterCos[i]);
            std::snprintf(name, sizeof(name), "spotLights[%d].constant", i);
            glUniform1f(glGetUniformLocation(lightingShader.Program, name), kc);
            std::snprintf(name, sizeof(name), "spotLights[%d].linear", i);
            glUniform1f(glGetUniformLocation(lightingShader.Program, name), kl);
            std::snprintf(name, sizeof(name), "spotLights[%d].quadratic", i);
            glUniform1f(glGetUniformLocation(lightingShader.Program, name), kq);
            std::snprintf(name, sizeof(name), "spotLights[%d].ambient", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name), amb.x, amb.y, amb.z);
            std::snprintf(name, sizeof(name), "spotLights[%d].diffuse", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name), dif.x, dif.y, dif.z);
            std::snprintf(name, sizeof(name), "spotLights[%d].specular", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name), spe.x, spe.y, spe.z);
        }

        // Matrices (sin cambios)
        GLint modelLoc = glGetUniformLocation(lightingShader.Program, "model");
        GLint viewLoc = glGetUniformLocation(lightingShader.Program, "view");
        GLint projLoc = glGetUniformLocation(lightingShader.Program, "projection");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view)); // Usar 'view' completa
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(g_Projection));

        glm::mat4 model(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Dibujar los estáticos (sin cambios)
        museo.Draw(lightingShader);
        astaBandera.Draw(lightingShader);


        // ==============================================================
        //  PASO 4: DIBUJAR BANDERA (flagShader)
        // ==============================================================
        flagShader.Use();
        glUniform1f(glGetUniformLocation(flagShader.Program, "time"), currentFrame);

        // --- Volver a pasar TODAS las luces (con intensidad local) ---
        glUniform1i(glGetUniformLocation(flagShader.Program, "material.diffuse"), 0);
        glUniform1i(glGetUniformLocation(flagShader.Program, "material.specular"), 1);
        glUniform1f(glGetUniformLocation(flagShader.Program, "material.shininess"), 16.0f);
        glUniform3f(glGetUniformLocation(flagShader.Program, "viewPos"), camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
        glUniformMatrix4fv(glGetUniformLocation(flagShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view)); // Usar 'view' completa
        glUniformMatrix4fv(glGetUniformLocation(flagShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(g_Projection));

        // Dir Light (SOL) (sin cambios)
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.ambient"), 0.06f * globalLightBoost, 0.06f * globalLightBoost, 0.06f * globalLightBoost);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.diffuse"), 0.12f * globalLightBoost, 0.12f * globalLightBoost, 0.12f * globalLightBoost);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.specular"), 0.22f * globalLightBoost, 0.22f * globalLightBoost, 0.22f * globalLightBoost);

        // Helper para flagShader (sin cambios, aplica localLightIntensity)
        auto setPointFlag = [&](int i, glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
            float kc, float kl, float kq) {
                char name[64];
                std::snprintf(name, sizeof(name), "pointLights[%d].position", i);
                glUniform3f(glGetUniformLocation(flagShader.Program, name), pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

                // Aplicar intensidad local (que ahora anima de 0.1 a 2.0)
                amb *= localLightIntensity;
                dif *= localLightIntensity;
                spec *= localLightIntensity;

                std::snprintf(name, sizeof(name), "pointLights[%d].ambient", i);
                glUniform3f(glGetUniformLocation(flagShader.Program, name), amb.x, amb.y, amb.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].diffuse", i);
                glUniform3f(glGetUniformLocation(flagShader.Program, name), dif.x, dif.y, dif.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].specular", i);
                glUniform3f(glGetUniformLocation(flagShader.Program, name), spec.x, spec.y, spec.z);
                std::snprintf(name, sizeof(name), "pointLights[%d].constant", i);
                glUniform1f(glGetUniformLocation(flagShader.Program, name), kc);
                std::snprintf(name, sizeof(name), "pointLights[%d].linear", i);
                glUniform1f(glGetUniformLocation(flagShader.Program, name), kl);
                std::snprintf(name, sizeof(name), "pointLights[%d].quadratic", i);
                glUniform1f(glGetUniformLocation(flagShader.Program, name), kq);
            };

        // ==================================================
        //  AJUSTE: Seteo de puntuales para flagShader
        // ==================================================
        for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
            glm::vec3 amb(0.04f), dif(0.90f), spec(0.55f);
            float kc = 1.0f, kl = 0.045f, kq = 0.0075f;
            float energyBoost = pointLightEnergy[i] / 1000.0f;

            // NO multiplicamos por globalLightBoost (sol)
            amb *= (0.6f * 0.7f * energyBoost);
            dif *= 0.60f * energyBoost;
            spec *= 0.40f * energyBoost;

            // APLICAMOS LAS HEURÍSTICAS DE CODE 1
            if (energyBoost > 1.5f) { kl = 0.028f; kq = 0.0019f; }
            if (pointLightPositions[i].z < 5.0f) {
                dif *= 0.70f; spec *= 0.50f; kl = 0.060f; kq = 0.0170f;
            }
            else if (pointLightPositions[i].z < 15.0f) {
                dif *= 0.90f;
            }
            // --- Fin de heurísticas ---

            setPointFlag(i, amb, dif, spec, kc, kl, kq);
        }

        // ==================================================
        //  AJUSTE: Seteo de Spots para flagShader
        // ==================================================
        for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
            char name[64];

            // NO multiplicamos por globalLightBoost (sol)
            glm::vec3 amb = 0.02f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 dif = 1.40f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 spe = 0.60f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];

            const float kc = 1.0f, kl = 0.045f, kq = 0.0075f;

            std::snprintf(name, sizeof(name), "spotLights[%d].position", i);
            glUniform3f(glGetUniformLocation(flagShader.Program, name), gSpotPos[i].x, gSpotPos[i].y, gSpotPos[i].z);
            std::snprintf(name, sizeof(name), "spotLights[%d].direction", i);
            glUniform3f(glGetUniformLocation(flagShader.Program, name), gSpotDir[i].x, gSpotDir[i].y, gSpotDir[i].z);
            std::snprintf(name, sizeof(name), "spotLights[%d].cutOff", i);
            glUniform1f(glGetUniformLocation(flagShader.Program, name), gSpotInnerCos[i]);
            std::snprintf(name, sizeof(name), "spotLights[%d].outerCutOff", i);
            glUniform1f(glGetUniformLocation(flagShader.Program, name), gSpotOuterCos[i]);
            std::snprintf(name, sizeof(name), "spotLights[%d].constant", i);
            glUniform1f(glGetUniformLocation(flagShader.Program, name), kc);
            std::snprintf(name, sizeof(name), "spotLights[%d].linear", i);
            glUniform1f(glGetUniformLocation(flagShader.Program, name), kl);
            std::snprintf(name, sizeof(name), "spotLights[%d].quadratic", i);
            glUniform1f(glGetUniformLocation(flagShader.Program, name), kq);
            std::snprintf(name, sizeof(name), "spotLights[%d].ambient", i);
            glUniform3f(glGetUniformLocation(flagShader.Program, name), amb.x, amb.y, amb.z);
            std::snprintf(name, sizeof(name), "spotLights[%d].diffuse", i);
            glUniform3f(glGetUniformLocation(flagShader.Program, name), dif.x, dif.y, dif.z);
            std::snprintf(name, sizeof(name), "spotLights[%d].specular", i);
            glUniform3f(glGetUniformLocation(flagShader.Program, name), spe.x, spe.y, spe.z);
        }

        // --- Finalmente, dibujar la bandera ---
        GLint modelLocFlag = glGetUniformLocation(flagShader.Program, "model");
        glUniformMatrix4fv(modelLocFlag, 1, GL_FALSE, glm::value_ptr(model)); // Usa la misma matriz identidad 'model'
        bandera.Draw(flagShader);

        // ==============================================================
        glfwSwapBuffers(window);
    } // --- FIN DEL GAME LOOP ---

    glfwTerminate();
    return 0;
}

// --- Movimiento (sin cambios) ---
void DoMovement() { /* ... (tu función sin cambios) ... */
    const bool  sprint = keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT];
    const float dt = deltaTime * g_MoveBoost * (sprint ? g_RunBoost : 1.0f);
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, dt);
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, dt);
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, dt);
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, dt);
}

// --- Input (sin cambios) ---
void KeyCallback(GLFWwindow* window, int key, int, int action, int) { /* ... (tu función sin cambios) ... */
    if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action) glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}
void MouseCallback(GLFWwindow* window, double xPos, double yPos) { /* ... (tu función sin cambios) ... */
    if (firstMouse) { lastX = (GLfloat)xPos; lastY = (GLfloat)yPos; firstMouse = false; }
    GLfloat xOffset = (GLfloat)xPos - lastX;
    GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos; lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}

// --- Redimensionamiento (sin cambios) ---
void FramebufferSizeCallback(GLFWwindow*, int width, int height) { /* ... (tu función sin cambios) ... */
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;
    glViewport(0, 0, width, height);
    g_Projection = glm::perspective(
        camera.GetZoom(),
        (GLfloat)width / (GLfloat)height,
        0.5f,   // near
        600.0f  // far
    );
}

// ===================================
//  FUNCIÓN - CARGADOR DE TEXTURA (sin cambios)
// ===================================
GLuint loadTexture(const char* path)
{
    GLuint textureID = 0; // Inicializar a 0
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        std::cerr << "Error: glGenTextures falló para " << path << std::endl;
        return 0;
    }
    // std::cout << "Generated Texture ID for " << path << ": " << textureID << std::endl; // Debug

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        GLenum internalFormat; // Usar formato interno específico
        if (nrComponents == 1) {
            format = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3) {
            format = GL_RGB;
            internalFormat = GL_RGB; // Para JPG/BMP sin alfa
        }
        else if (nrComponents == 4) {
            format = GL_RGBA;
            internalFormat = GL_RGBA; // Para PNG con alfa
        }
        else {
            std::cerr << "Formato de textura desconocido con " << nrComponents << " componentes para " << path << std::endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Configuración de texturas
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // <-- CAMBIO: Mejor para skybox
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // <-- CAMBIO: Evita bordes repetidos
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        // std::cout << "Successfully loaded: " << path << " (ID: " << textureID << ")" << std::endl; // Debug
        glBindTexture(GL_TEXTURE_2D, 0); // Desvincular textura
    }
    else
    {
        std::cerr << "*** FAILED to load texture: " << path << " - Reason: " << stbi_failure_reason() << std::endl;
        glDeleteTextures(1, &textureID);
        textureID = 0; // Retornar 0 en caso de fallo
    }

    return textureID;
}