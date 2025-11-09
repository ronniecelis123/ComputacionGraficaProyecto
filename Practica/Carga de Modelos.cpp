#include <iostream>
#include <cmath>
#include <cstdio>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// --- Prototipos ---
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void DoMovement();
void UpdateCharacter(); // Animación y estado del mono
void UpdateCaballero(); // Animación del caballero (de momento casi vacía)
GLuint loadTexture(const char* path);

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

// --- Brillo global (SOL) ---
float globalLightBoost = 3.5f;

// --- Tiempo ---
GLfloat deltaTime = 0.0f, lastFrame = 0.0f;

// --- Proyección global ---
glm::mat4 g_Projection(1.0f);

// ===================================
//  VARIABLES - CICLO DÍA/NOCHE
// ===================================
float g_TimeOfDay = 0.0f;     // 0.0 = Mediodía, 0.5 = Medianoche, 1.0 = Mediodía
const float CYCLE_SPEED = 0.03f;
const float DAY_BOOST = 5.0f;
const float NIGHT_BOOST = 1.0f;

const float MAX_LOCAL_INTENSITY = 2.5f;
const float MIN_LOCAL_INTENSITY = 0.1f;

GLuint g_SkyTextureDay;

// =====================
//  BLENDER -> OPENGL
// =====================
static inline glm::vec3 FromBlender(const glm::vec3& b) {
    // Blender (X, Y, Z) -> OpenGL (X, Z, -Y)
    return glm::vec3(b.x, b.z, -b.y);
}

struct BL { glm::vec3 pos; float energy; };
static const BL kBlenderLights[] = {
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
    { { -7.9162f,  168.05f, 149.2f   }, 2000.0f }
};
static const int NUM_POINT_LIGHTS = static_cast<int>(sizeof(kBlenderLights) / sizeof(kBlenderLights[0]));
glm::vec3 pointLightPositions[NUM_POINT_LIGHTS];
float     pointLightEnergy[NUM_POINT_LIGHTS];

struct BLSpot {
    glm::vec3 pos; glm::vec3 dir; float innerDeg; float outerDeg; float intensity; glm::vec3 color;
};
static const BLSpot kBlenderSpots[] = {
    { { -107.9f,   60.565f, 18.752f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -83.562f, 60.565f, 20.474f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -60.777f, 64.805f, 18.466f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -36.024f, 62.641f, 18.466f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -31.645f, 33.261f, 17.779f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { {  -46.790f, 33.261f, 17.779f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { { -113.410f, 33.261f, 17.779f }, {  0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { { -107.46f, -36.14f, 23.863f },  { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } },
    { { -112.78f, -36.14f, 23.863f },  { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } }
};
static const int NUM_SPOT_LIGHTS = static_cast<int>(sizeof(kBlenderSpots) / sizeof(kBlenderSpots[0]));
glm::vec3 gSpotPos[NUM_SPOT_LIGHTS];
glm::vec3 gSpotDir[NUM_SPOT_LIGHTS];
float     gSpotInnerCos[NUM_SPOT_LIGHTS];
float     gSpotOuterCos[NUM_SPOT_LIGHTS];
float     gSpotIntensity[NUM_SPOT_LIGHTS];
glm::vec3 gSpotColor[NUM_SPOT_LIGHTS];

static void InitConvertedLights() {
    for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
        pointLightPositions[i] = FromBlender(kBlenderLights[i].pos);
        pointLightEnergy[i] = kBlenderLights[i].energy;
    }
    for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
        gSpotPos[i] = FromBlender(kBlenderSpots[i].pos);
        gSpotDir[i] = glm::normalize(FromBlender(kBlenderSpots[i].dir)); // dirección también convertida
        gSpotInnerCos[i] = glm::cos(glm::radians(kBlenderSpots[i].innerDeg));
        gSpotOuterCos[i] = glm::cos(glm::radians(kBlenderSpots[i].outerDeg));
        gSpotIntensity[i] = kBlenderSpots[i].intensity;
        gSpotColor[i] = kBlenderSpots[i].color;
    }
}

// ========================================================
//  PERSONAJE JERÁRQUICO (Mono Caminante)
// ========================================================

// Escala global del personaje
const float CHARACTER_SCALE = 4.569f;

// Posición base del mono (torso en Blender)
const glm::vec3 torsoOffset = glm::vec3(
    34.6179f,
    3.85286f,
    0.0f
);

// 'characterPos' representa la posición MUNDIAL actual del personaje
glm::vec3 characterPos = torsoOffset;

// ---------- BRAZO IZQUIERDO (Hombro1 / Brazo1) ----------
const glm::vec3 hombroIzqOffset = glm::vec3(
    1.91730f,
    1.12384f,
    0.0f
); // hombro1 - torso

const glm::vec3 brazoIzqOffset = glm::vec3(
    0.88150f,
    -2.48061f,
    0.313965f
); // brazo1 - hombro1

// ---------- BRAZO DERECHO (Hombro2 / Brazo2) ----------
const glm::vec3 hombroDerOffset = glm::vec3(
    -1.83660f,
    0.74994f,
    0.0f
); // hombro2 - torso

const glm::vec3 brazoDerOffset = glm::vec3(
    -1.13450f,
    -2.33765f,
    0.090295f
); // brazo2 - hombro2

// ---------- PIERNA IZQUIERDA (Muslo1 / Pie1) ----------
const glm::vec3 musloIzqOffset = glm::vec3(
    0.60530f,
    -4.16120f,
    0.0f
); // muslo1 - torso

const glm::vec3 pieIzqOffset = glm::vec3(
    -0.13580f,
    -2.60476f,
    0.0f
); // pierna1 - muslo1

// ---------- PIERNA DERECHA (Muslo2 / Pie2) ----------
const glm::vec3 musloDerOffset = glm::vec3(
    -0.54790f,
    -4.15132f,
    0.0f
); // muslo2 - torso

const glm::vec3 pieDerOffset = glm::vec3(
    -0.03500f,
    -2.53817f,
    -0.083658f
); // pierna2 - muslo2

// Ángulos de animación (Mono)
float legBaseAngle = 0.0f;
float musloIzqAngle = 0.0f;
float pieIzqAngle = 0.0f;
float musloDerAngle = 0.0f;
float pieDerAngle = 0.0f;
float hombroIzqAngle = 0.0f;
float brazoIzqAngle = 0.0f;
float hombroDerAngle = 0.0f;
float brazoDerAngle = 0.0f;

// Máquina de estados y movimiento (Mono)
enum CharacterState {
    WALKING_FORWARD,
    TURNING_AT_END,
    WALKING_BACK,
    TURNING_AT_START
};

CharacterState g_CharacterState = WALKING_FORWARD; // Estado inicial
float characterRotationY = 0.0f;                   // Rotación actual del personaje en grados

const float WALK_DISTANCE = 15.0f; // Distancia que caminará hacia adelante
const float WALK_SPEED = 8.0f;     // Velocidad de movimiento
const float TURN_SPEED = 180.0f;   // Velocidad de rotación en grados/segundo

const glm::vec3 g_CharacterStartPos = torsoOffset;
const glm::vec3 g_CharacterEndPos = g_CharacterStartPos + glm::vec3(0.0f, 0.0f, WALK_DISTANCE);

// ===== CABALLERO =====

// Escala (ya ajustada)
const float g_CaballeroScale = 5.540f;

// Posiciones en Blender
const glm::vec3 kKnightBodyPosBlender = glm::vec3(73.9528f, 7.8842f, 4.41893f);
const glm::vec3 kKnightShieldPosBlender = glm::vec3(74.4190f, 11.3946f, 6.62876f);
const glm::vec3 kKnightSwordPosBlender = glm::vec3(72.2018f, 3.98538f, 8.97841f);

// Convertidas a OpenGL
const glm::vec3 g_KnightBodyPosGL = FromBlender(kKnightBodyPosBlender);
const glm::vec3 g_KnightShieldPosGL = FromBlender(kKnightShieldPosBlender);
const glm::vec3 g_KnightSwordPosGL = FromBlender(kKnightSwordPosBlender);

// Offsets respecto al cuerpo (en espacio OpenGL)
const glm::vec3 g_KnightShieldOffsetGL = g_KnightShieldPosGL - g_KnightBodyPosGL;
const glm::vec3 g_KnightSwordOffsetGL = g_KnightSwordPosGL - g_KnightBodyPosGL;

// Ángulos de brazos
float g_KnightShieldAngle = 0.0f;
float g_KnightSwordAngle = 0.0f;

// Rotación del caballero completo alrededor del eje "vertical" (Y de OpenGL)
float g_KnightRootAngle = 0.0f;
float g_KnightTargetAngle = 0.0f;

// Tiempo interno para la animación de brazos
float g_KnightAnimTime = 0.0f;

// Máquina de estados del caballero
enum KnightState {
    KNIGHT_ANIMATING, // mueve brazos
    KNIGHT_TURNING    // gira 45°
};

KnightState g_KnightState = KNIGHT_ANIMATING;

// Parámetros de la animación
const float KNIGHT_ANIM_DURATION = 3.0f;   // segundos moviendo brazos antes de girar
const float KNIGHT_TURN_STEP = 90.0f;  // grados por giro
const float KNIGHT_TURN_SPEED = 90.0f;  // grados/segundo al girar





// ================= MAIN =================
int main() {
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

    auto safe_cstr = [](const GLubyte* s) { return s ? reinterpret_cast<const char*>(s) : "(null)"; };
    std::cout << "> Version: " << safe_cstr(glGetString(GL_VERSION)) << '\n';
    std::cout << "> Vendor: " << safe_cstr(glGetString(GL_VENDOR)) << '\n';
    std::cout << "> Renderer: " << safe_cstr(glGetString(GL_RENDERER)) << '\n';
    std::cout << "> SL Ver: " << safe_cstr(glGetString(GL_SHADING_LANGUAGE_VERSION)) << '\n';

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader lightingShader("Shader/lighting.vs", "Shader/lighting.frag");
    Shader flagShader("Shader/bandera.vs", "Shader/bandera.frag");
    Shader skyShader("Shader/skybox_esfera.vs", "Shader/skybox_esfera.frag");

    // Modelos
    Model museo((char*)"Models/museo.obj");
    Model astaBandera((char*)"Models/bandera.obj");
    Model bandera((char*)"Models/banderaLogo.obj");
    Model esfera((char*)"Models/esfera.obj");

    // Personaje (Mono)
    Model Torso((char*)"Models/person/torso.obj");
    Model Muslo1((char*)"Models/person/muslo1.obj");
    Model Pie1((char*)"Models/person/pie1.obj");
    Model Muslo2((char*)"Models/person/muslo2.obj");
    Model Pie2((char*)"Models/person/pie2.obj");
    Model Hombro1((char*)"Models/person/hombro1.obj");
    Model Brazo1((char*)"Models/person/brazo1.obj");
    Model Hombro2((char*)"Models/person/hombro2.obj");
    Model Brazo2((char*)"Models/person/brazo2.obj");

    // Modelos del Caballero
    Model CuerpoCaballero((char*)"Models/caballero/cuerpo.obj");
    Model BrazoEscudo((char*)"Models/caballero/brazoEscudo.obj");
    Model BrazoEspada((char*)"Models/caballero/brazoEspada.obj");

    g_SkyTextureDay = loadTexture("Models/dia.jpeg");

    InitConvertedLights();
    g_Projection = glm::perspective(
        camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT,
        0.5f,
        600.0f
    );

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        DoMovement();
        UpdateCharacter();
        UpdateCaballero(); // de momento solo prepara ángulos

        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ================= CICLO DÍA/NOCHE =================
        g_TimeOfDay = fmod(currentFrame * CYCLE_SPEED, 1.0f);
        float cosTime = cos(g_TimeOfDay * 2.0f * 3.14159f);
        float blendFactor = (cosTime + 1.0f) * 0.5f; // 1 = día, 0 = noche

        globalLightBoost = NIGHT_BOOST + (DAY_BOOST - NIGHT_BOOST) * blendFactor;

        glm::vec3 dayColor(1.0f, 1.0f, 1.0f);
        glm::vec3 sunsetColor(1.0f, 0.7f, 0.4f);
        glm::vec3 nightColor(0.02f, 0.05f, 0.15f);

        glm::vec3 tintColor;
        if (blendFactor > 0.5f) {
            float t = (blendFactor - 0.5f) * 2.0f;
            tintColor = glm::mix(sunsetColor, dayColor, t);
        }
        else {
            float t = blendFactor * 2.0f;
            tintColor = glm::mix(nightColor, sunsetColor, t);
        }

        float localLightIntensity =
            MIN_LOCAL_INTENSITY +
            (MAX_LOCAL_INTENSITY - MIN_LOCAL_INTENSITY) * (1.0f - blendFactor);

        // ================= SKYBOX (ESFERA) =================
        glDepthMask(GL_FALSE);
        glm::mat4 view = camera.GetViewMatrix();
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

        // ================= ESCENA ESTÁTICA =================
        lightingShader.Use();

        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.diffuse"), 0);
        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.specular"), 1);
        glUniform1f(glGetUniformLocation(lightingShader.Program, "material.shininess"), 16.0f);

        glUniform3f(glGetUniformLocation(lightingShader.Program, "viewPos"),
            camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);

        // DirLight (sol)
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.ambient"),
            0.06f * globalLightBoost, 0.06f * globalLightBoost, 0.06f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.diffuse"),
            0.12f * globalLightBoost, 0.12f * globalLightBoost, 0.12f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.specular"),
            0.22f * globalLightBoost, 0.22f * globalLightBoost, 0.22f * globalLightBoost);

        auto setPoint = [&](int i, glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
            float kc, float kl, float kq) {
                char name[64];
                std::snprintf(name, sizeof(name), "pointLights[%d].position", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name),
                    pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

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

        for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
            glm::vec3 amb(0.04f), dif(0.90f), spec(0.55f);
            float kc = 1.0f, kl = 0.045f, kq = 0.0075f;
            float energyBoost = pointLightEnergy[i] / 1000.0f;

            amb *= (0.6f * 0.7f * energyBoost);
            dif *= 0.60f * energyBoost;
            spec *= 0.40f * energyBoost;

            if (energyBoost > 1.5f) { kl = 0.028f; kq = 0.0019f; }

            if (pointLightPositions[i].z < 5.0f) {
                dif *= 0.70f; spec *= 0.50f; kl = 0.060f; kq = 0.0170f;
            }
            else if (pointLightPositions[i].z < 15.0f) {
                dif *= 0.90f;
            }
            setPoint(i, amb, dif, spec, kc, kl, kq);
        }

        for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
            char name[64];

            glm::vec3 amb = 0.02f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 dif = 1.40f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];
            glm::vec3 spe = 0.60f * gSpotIntensity[i] * localLightIntensity * gSpotColor[i];

            const float kc = 1.0f, kl = 0.045f, kq = 0.0075f;

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

        GLint modelLoc = glGetUniformLocation(lightingShader.Program, "model");
        GLint viewLoc = glGetUniformLocation(lightingShader.Program, "view");
        GLint projLoc = glGetUniformLocation(lightingShader.Program, "projection");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(g_Projection));

        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // ---- Museo y asta ----
        museo.Draw(lightingShader);
        astaBandera.Draw(lightingShader);

        // ================= MONO =================
        {
            glm::mat4 base = glm::mat4(1.0f);
            base = glm::translate(base, characterPos);
            base = glm::rotate(base, glm::radians(characterRotationY), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::mat4 torsoBase = base;

            // Torso
            glm::mat4 torsoModel = torsoBase;
            torsoModel = glm::scale(torsoModel, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(torsoModel));
            Torso.Draw(lightingShader);

            // Pierna izquierda
            glm::mat4 musloLBase = torsoBase;
            musloLBase = glm::translate(musloLBase, musloIzqOffset);
            musloLBase = glm::rotate(musloLBase, glm::radians(musloIzqAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 musloLModel = glm::scale(musloLBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(musloLModel));
            Muslo1.Draw(lightingShader);

            glm::mat4 pieLBase = musloLBase;
            pieLBase = glm::translate(pieLBase, pieIzqOffset);
            pieLBase = glm::rotate(pieLBase, glm::radians(pieIzqAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 pieLModel = glm::scale(pieLBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pieLModel));
            Pie1.Draw(lightingShader);

            // Pierna derecha
            glm::mat4 musloRBase = torsoBase;
            musloRBase = glm::translate(musloRBase, musloDerOffset);
            musloRBase = glm::rotate(musloRBase, glm::radians(musloDerAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 musloRModel = glm::scale(musloRBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(musloRModel));
            Muslo2.Draw(lightingShader);

            glm::mat4 pieRBase = musloRBase;
            pieRBase = glm::translate(pieRBase, pieDerOffset);
            pieRBase = glm::rotate(pieRBase, glm::radians(pieDerAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 pieRModel = glm::scale(pieRBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(pieRModel));
            Pie2.Draw(lightingShader);

            // Brazo izquierdo
            glm::mat4 hombroLBase = torsoBase;
            hombroLBase = glm::translate(hombroLBase, hombroIzqOffset);
            hombroLBase = glm::rotate(hombroLBase, glm::radians(hombroIzqAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 hombroLModel = glm::scale(hombroLBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(hombroLModel));
            Hombro1.Draw(lightingShader);

            glm::mat4 brazoLBase = hombroLBase;
            brazoLBase = glm::translate(brazoLBase, brazoIzqOffset);
            brazoLBase = glm::rotate(brazoLBase, glm::radians(brazoIzqAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 brazoLModel = glm::scale(brazoLBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(brazoLModel));
            Brazo1.Draw(lightingShader);

            // Brazo derecho
            glm::mat4 hombroRBase = torsoBase;
            hombroRBase = glm::translate(hombroRBase, hombroDerOffset);
            hombroRBase = glm::rotate(hombroRBase, glm::radians(hombroDerAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 hombroRModel = glm::scale(hombroRBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(hombroRModel));
            Hombro2.Draw(lightingShader);

            glm::mat4 brazoRBase = hombroRBase;
            brazoRBase = glm::translate(brazoRBase, brazoDerOffset);
            brazoRBase = glm::rotate(brazoRBase, glm::radians(brazoDerAngle), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 brazoRModel = glm::scale(brazoRBase, glm::vec3(CHARACTER_SCALE));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(brazoRModel));
            Brazo2.Draw(lightingShader);
        }
        // ================= CABALLERO =================
        {
            glm::mat4 base = glm::mat4(1.0f);

            // 1) Primero trasladar al cuerpo (posición en mundo)
            base = glm::translate(base, g_KnightBodyPosGL);

            // 2) Luego rotar sobre su eje Y (gira sobre su propio pivote)
            base = glm::rotate(base,
                glm::radians(g_KnightRootAngle),
                glm::vec3(0.0f, 1.0f, 0.0f));

            // ----- CUERPO -----
            glm::mat4 cuerpoModel = base;
            cuerpoModel = glm::scale(cuerpoModel, glm::vec3(g_CaballeroScale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cuerpoModel));
            CuerpoCaballero.Draw(lightingShader);

            // Eje Y de Blender → aquí lo usamos como (0,0,-1)
            const glm::vec3 blenderYinGL(0.0f, 0.0f, -1.0f);

            // ----- BRAZO ESCUDO -----
            glm::mat4 escudoModel = base;
            escudoModel = glm::translate(escudoModel, g_KnightShieldOffsetGL);
            escudoModel = glm::rotate(
                escudoModel,
                glm::radians(g_KnightShieldAngle),
                blenderYinGL
            );
            escudoModel = glm::scale(escudoModel, glm::vec3(g_CaballeroScale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(escudoModel));
            BrazoEscudo.Draw(lightingShader);

            // ----- BRAZO ESPADA -----
            glm::mat4 espadaModel = base;
            espadaModel = glm::translate(espadaModel, g_KnightSwordOffsetGL);
            espadaModel = glm::rotate(
                espadaModel,
                glm::radians(g_KnightSwordAngle),
                blenderYinGL
            );
            espadaModel = glm::scale(espadaModel, glm::vec3(g_CaballeroScale));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(espadaModel));
            BrazoEspada.Draw(lightingShader);
        }






        // ================= BANDERA =================
        flagShader.Use();
        glUniform1f(glGetUniformLocation(flagShader.Program, "time"), currentFrame);

        glUniform1i(glGetUniformLocation(flagShader.Program, "material.diffuse"), 0);
        glUniform1i(glGetUniformLocation(flagShader.Program, "material.specular"), 1);
        glUniform1f(glGetUniformLocation(flagShader.Program, "material.shininess"), 16.0f);
        glUniform3f(glGetUniformLocation(flagShader.Program, "viewPos"),
            camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);
        glUniformMatrix4fv(glGetUniformLocation(flagShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(flagShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(g_Projection));

        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.ambient"), 0.06f * globalLightBoost, 0.06f * globalLightBoost, 0.06f * globalLightBoost);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.diffuse"), 0.12f * globalLightBoost, 0.12f * globalLightBoost, 0.12f * globalLightBoost);
        glUniform3f(glGetUniformLocation(flagShader.Program, "dirLight.specular"), 0.22f * globalLightBoost, 0.22f * globalLightBoost, 0.22f * globalLightBoost);

        auto setPointFlag = [&](int i, glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
            float kc, float kl, float kq) {
                char name[64];
                std::snprintf(name, sizeof(name), "pointLights[%d].position", i);
                glUniform3f(glGetUniformLocation(flagShader.Program, name),
                    pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

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
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kc);
                std::snprintf(name, sizeof(name), "pointLights[%d].linear", i);
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kl);
                std::snprintf(name, sizeof(name), "pointLights[%d].quadratic", i);
                glUniform1f(glGetUniformLocation(lightingShader.Program, name), kq);
            };

        for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
            glm::vec3 amb(0.04f), dif(0.90f), spec(0.55f);
            float kc = 1.0f, kl = 0.045f, kq = 0.0075f;
            float energyBoost = pointLightEnergy[i] / 1000.0f;

            amb *= (0.6f * 0.7f * energyBoost);
            dif *= 0.60f * energyBoost;
            spec *= 0.40f * energyBoost;

            if (energyBoost > 1.5f) { kl = 0.028f; kq = 0.0019f; }
            if (pointLightPositions[i].z < 5.0f) {
                dif *= 0.70f; spec *= 0.50f; kl = 0.060f; kq = 0.0170f;
            }
            else if (pointLightPositions[i].z < 15.0f) {
                dif *= 0.90f;
            }
            setPointFlag(i, amb, dif, spec, kc, kl, kq);
        }

        for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
            char name[64];

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

        GLint modelLocFlag = glGetUniformLocation(flagShader.Program, "model");
        glm::mat4 modelFlag = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLocFlag, 1, GL_FALSE, glm::value_ptr(modelFlag));
        bandera.Draw(flagShader);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void UpdateCaballero() {
    // Movimiento de brazos
    const float speed = 1.5f;
    const float maxSword = 40.0f;
    const float maxShield = 25.0f;

    switch (g_KnightState) {
    case KNIGHT_ANIMATING:
    {
        g_KnightAnimTime += deltaTime;
        float t = g_KnightAnimTime * speed;

        // Misma anim que tenías
        g_KnightSwordAngle = std::sin(t) * maxSword;
        g_KnightShieldAngle = std::sin(t + 0.4f) * maxShield;

        // Cuando termina el “combo”, toca girar
        if (g_KnightAnimTime >= KNIGHT_ANIM_DURATION) {
            g_KnightAnimTime = 0.0f;
            g_KnightTargetAngle = g_KnightRootAngle + KNIGHT_TURN_STEP;
            g_KnightState = KNIGHT_TURNING;
        }
    }
    break;

    case KNIGHT_TURNING:
    {
        // Brazos se quedan como están, solo gira el cuerpo
        float step = KNIGHT_TURN_SPEED * deltaTime;
        g_KnightRootAngle += step;

        if (g_KnightRootAngle >= g_KnightTargetAngle) {
            g_KnightRootAngle = g_KnightTargetAngle; // clamp
            g_KnightState = KNIGHT_ANIMATING;        // siguiente combo
        }

        // (opcional) evita que el ángulo crezca infinito
        if (g_KnightRootAngle >= 360.0f) {
            g_KnightRootAngle -= 360.0f;
            g_KnightTargetAngle -= 360.0f;
        }
    }
    break;
    }
}



// --- Movimiento ---
void DoMovement() {
    const bool  sprint = keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT];
    const float dt = deltaTime * g_MoveBoost * (sprint ? g_RunBoost : 1.0f);
    if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, dt);
    if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, dt);
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, dt);
    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, dt);
}

// --- Animación del Mono ---
void AnimateWalkCycle() {
    static bool forward = true;
    const float maxAngle = 25.0f;
    const float speed = 50.0f; // Velocidad de la animación de las piernas

    if (forward) {
        legBaseAngle += speed * deltaTime;
        if (legBaseAngle > maxAngle) {
            legBaseAngle = maxAngle;
            forward = false;
        }
    }
    else {
        legBaseAngle -= speed * deltaTime;
        if (legBaseAngle < -maxAngle) {
            legBaseAngle = -maxAngle;
            forward = true;
        }
    }

    // Piernas y brazos se mueven en oposición
    musloIzqAngle = legBaseAngle;
    pieIzqAngle = legBaseAngle * 0.5f;
    musloDerAngle = -legBaseAngle;
    pieDerAngle = -legBaseAngle * 0.5f;

    hombroIzqAngle = -legBaseAngle;
    brazoIzqAngle = -legBaseAngle * 0.4f;
    hombroDerAngle = legBaseAngle;
    brazoDerAngle = legBaseAngle * 0.2f;
}

void UpdateCharacter() {
    switch (g_CharacterState) {
    case WALKING_FORWARD:
        AnimateWalkCycle();
        characterPos.z += WALK_SPEED * deltaTime;
        if (characterPos.z >= g_CharacterEndPos.z) {
            characterPos.z = g_CharacterEndPos.z;
            g_CharacterState = TURNING_AT_END;
        }
        break;

    case TURNING_AT_END:
        musloIzqAngle = pieIzqAngle = musloDerAngle = pieDerAngle = 0.0f;
        hombroIzqAngle = brazoIzqAngle = hombroDerAngle = brazoDerAngle = 0.0f;

        characterRotationY += TURN_SPEED * deltaTime;
        if (characterRotationY >= 180.0f) {
            characterRotationY = 180.0f;
            g_CharacterState = WALKING_BACK;
        }
        break;

    case WALKING_BACK:
        AnimateWalkCycle();
        characterPos.z -= WALK_SPEED * deltaTime;
        if (characterPos.z <= g_CharacterStartPos.z) {
            characterPos.z = g_CharacterStartPos.z;
            g_CharacterState = TURNING_AT_START;
        }
        break;

    case TURNING_AT_START:
        musloIzqAngle = pieIzqAngle = musloDerAngle = pieDerAngle = 0.0f;
        hombroIzqAngle = brazoIzqAngle = hombroDerAngle = brazoDerAngle = 0.0f;

        characterRotationY += TURN_SPEED * deltaTime;
        if (characterRotationY >= 360.0f) {
            characterRotationY = 0.0f;
            g_CharacterState = WALKING_FORWARD;
        }
        break;
    }
}


// --- Input ---
void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}

void MouseCallback(GLFWwindow*, double xPos, double yPos) {
    if (firstMouse) {
        lastX = (GLfloat)xPos;
        lastY = (GLfloat)yPos;
        firstMouse = false;
    }
    GLfloat xOffset = (GLfloat)xPos - lastX;
    GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos;
    lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}

// --- Redimensionamiento ---
void FramebufferSizeCallback(GLFWwindow*, int width, int height) {
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;
    glViewport(0, 0, width, height);
    g_Projection = glm::perspective(
        camera.GetZoom(),
        (GLfloat)width / (GLfloat)height,
        0.5f,
        600.0f
    );
}

// --- Carga de textura ---
GLuint loadTexture(const char* path) {
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        std::cerr << "Error: glGenTextures fallo para " << path << std::endl;
        return 0;
    }

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        GLenum internalFormat;
        if (nrComponents == 1) {
            format = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3) {
            format = GL_RGB;
            internalFormat = GL_RGB;
        }
        else if (nrComponents == 4) {
            format = GL_RGBA;
            internalFormat = GL_RGBA;
        }
        else {
            std::cerr << "Formato de textura desconocido con " << nrComponents
                << " componentes para " << path << std::endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
            format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        std::cerr << "*** FAILED to load texture: " << path
            << " - Reason: " << stbi_failure_reason() << std::endl;
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    return textureID;
}
