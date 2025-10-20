
#include <iostream>
#include <cmath>
#include <cstdio>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SOIL2/SOIL2.h"

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// --- Prototipos ---
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void DoMovement();

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

// --- Brillo global de TODAS las luces ---
float globalLightBoost = 2.0f;   // 1.0 = normal

// --- Tiempo ---
GLfloat deltaTime = 0.0f, lastFrame = 0.0f;

// --- Proyección global (para poder recalcular al redimensionar) ---
glm::mat4 g_Projection(1.0f);

// =====================
//  BLENDER -> OPENGL
// =====================
// Conversión de posiciones/direcciones Blender(Z-up) -> OpenGL(Y-up): (x,y,z) -> (x,z,-y)
static inline glm::vec3 FromBlender(const glm::vec3& b) {
    return glm::vec3(b.x, b.z, -b.y);
}

// ===============================
//  PUNTUALES (desde Blender)
// ===============================
struct BL {
    glm::vec3 pos; // (x,y,z) en Blender
    float     energy;
};

static const BL kBlenderLights[] = {
    // 8 primeras, energy = 1000
    { {  7.506f,  -1.564f, 33.050f }, 1000.0f }, // Light
    { {  7.506f,  18.210f, 33.050f }, 1000.0f }, // Light.001
    { {  7.506f, -23.389f, 33.050f }, 1000.0f }, // Light.002
    { {  7.506f,  39.512f, 33.050f }, 1000.0f }, // Light.003
    { {  7.506f,  53.020f, 33.050f }, 1000.0f }, // Light.004
    { { -29.320f, 53.020f,  33.050f }, 1000.0f }, // Light.005
    { {  7.506f,  10.228f,  9.497f }, 1000.0f }, // Light.006
    { { 25.063f,  10.228f, 21.128f }, 100.0f  }, // Light.007
    // Resto
    { { -60.455f,   0.000f, 14.382f }, 300.0f  }, // Point
    { { -29.890f,   0.000f, 14.382f }, 300.0f  }, // Point.001
    { { -60.455f,  32.697f, 14.382f }, 300.0f  }, // Point.002
    { { -60.455f, -14.834f, 14.382f }, 300.0f  }, // Point.003
    { { -88.231f,   0.000f, 10.847f }, 1000.0f }, // Point.004
    { { -88.231f,  20.268f, 10.847f }, 1000.0f }, // Point.005
    { { -88.231f,  44.557f, 11.160f }, 1000.0f }, // Point.006
    { { -88.231f, -21.363f, 10.847f }, 1000.0f }, // Point.007
    { { -88.231f, -42.799f, 10.847f }, 1000.0f }, // Point.008
    { { -63.232f, -42.799f, 11.879f }, 500.0f  }, // Point.009
    { { -33.888f, -42.799f, 11.879f }, 300.0f  }, // Point.010
    { { -20.423f, -42.799f, 11.879f }, 300.0f  }, // Point.011
    { { -83.964f,  56.018f, 11.160f }, 300.0f  }, // Point.012
    { { -64.739f,  56.018f, 11.160f }, 300.0f  }, // Point.013
    { { -44.714f,  56.018f, 11.160f }, 300.0f  }, // Point.014
    { { -22.686f,  56.018f, 11.160f }, 300.0f  }, // Point.015
    { { 15.258f, -27.267f, -3.3904f }, 1000.0f }, // Luz solar simulada
    { { 15.258f,  38.584f, -3.3904f }, 2000.0f },
    { { -66.443f, 10.228f, -6.4713f }, 20000.0f }, // Luz solar simulada 1
    { { -154.04f, -140.55f, 69.091f }, 20000.0f },
    { { -7.9162f,  168.05f, 149.2f  }, 2000.0f }
};

static const int NUM_POINT_LIGHTS =
static_cast<int>(sizeof(kBlenderLights) / sizeof(kBlenderLights[0]));

glm::vec3 pointLightPositions[NUM_POINT_LIGHTS];
float     pointLightEnergy[NUM_POINT_LIGHTS];

// ====================================
//  SPOTLIGHTS (arreglo)
// ====================================
struct BLSpot {
    glm::vec3 pos;      // posición en Blender (Z-up)
    glm::vec3 dir;      // dirección en Blender (hacia donde apunta)
    float innerDeg;     // ángulo interno (grados)
    float outerDeg;     // ángulo externo (grados)
    float intensity;    // factor de intensidad
    glm::vec3 color;    // color RGB [0..1]
};



static const BLSpot kBlenderSpots[] = {
    // —— Los 4 que ya tenías ——
    { { -107.9f,   60.565f, 18.752f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -83.562f, 60.565f, 20.474f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -60.777f, 64.805f, 18.466f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },
    { {  -36.024f, 62.641f, 18.466f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 0.0f, 1.0f, 1.0f } },

    // —— Los 3 que añadiste hace poco ——
    { {  -31.645f, 33.261f, 17.779f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { {  -46.790f, 33.261f, 17.779f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },
    { { -113.410f, 33.261f, 17.779f }, {  0.0f,      0.0f,     -1.0f }, 15.0f, 22.0f, 2.0f, { 1.0f, 1.0f, 1.0f } },

       { { -107.46f, -36.14f, 23.863f }, { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } },
    { { -112.78f, -36.14f, 23.863f }, { 0.0f, 0.0f, -1.0f }, 15.0f, 22.0f, 3.0f, { 1.0f, 1.0f, 1.0f } }
};


static const int NUM_SPOT_LIGHTS =
static_cast<int>(sizeof(kBlenderSpots) / sizeof(kBlenderSpots[0]));

// Buffers convertidos a OpenGL
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
        gSpotDir[i] = glm::normalize(FromBlender(kBlenderSpots[i].dir));
        gSpotInnerCos[i] = glm::cos(glm::radians(kBlenderSpots[i].innerDeg));
        gSpotOuterCos[i] = glm::cos(glm::radians(kBlenderSpots[i].outerDeg));
        gSpotIntensity[i] = kBlenderSpots[i].intensity;
        gSpotColor[i] = kBlenderSpots[i].color;
    }
}

int main() {
    // Init GLFW
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

    // Info GL (opcional)
    auto safe_cstr = [](const GLubyte* s) { return s ? reinterpret_cast<const char*>(s) : "(null)"; };
    std::cout << "> Version: " << safe_cstr(glGetString(GL_VERSION)) << '\n';
    std::cout << "> Vendor: " << safe_cstr(glGetString(GL_VENDOR)) << '\n';
    std::cout << "> Renderer: " << safe_cstr(glGetString(GL_RENDERER)) << '\n';
    std::cout << "> SL Ver: " << safe_cstr(glGetString(GL_SHADING_LANGUAGE_VERSION)) << '\n';

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // Shaders
    Shader lightingShader("Shader/lighting.vs", "Shader/lighting.frag");

    // Modelo
    Model museo((char*)"Models/PruebaMuseo.obj");

    // Convertir posiciones de luces una vez
    InitConvertedLights();

    // Proyección (AUMENTO DE RANGO: near=0.5f, far=600.0f)
    g_Projection = glm::perspective(
        camera.GetZoom(),
        (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT,
        0.5f,     // near mayor = mejor precisión
        600.0f    // far más lejos = mayor distancia de visión
    );

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;

        glfwPollEvents();
        DoMovement();

        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightingShader.Use();

        // Material (coincide con tu lighting.frag → "material")
        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.diffuse"), 0);
        glUniform1i(glGetUniformLocation(lightingShader.Program, "material.specular"), 1);
        glUniform1f(glGetUniformLocation(lightingShader.Program, "material.shininess"), 16.0f);

        // Cámara
        glm::mat4 view = camera.GetViewMatrix();
        glUniform3f(glGetUniformLocation(lightingShader.Program, "viewPos"),
            camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);

        // Luz direccional suave (relleno global) con boost
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.direction"), -0.2f, -1.0f, -0.3f);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.ambient"),
            0.06f * globalLightBoost, 0.06f * globalLightBoost, 0.06f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.diffuse"),
            0.12f * globalLightBoost, 0.12f * globalLightBoost, 0.12f * globalLightBoost);
        glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.specular"),
            0.22f * globalLightBoost, 0.22f * globalLightBoost, 0.22f * globalLightBoost);

        // Helper para setear una puntual
        auto setPoint = [&](int i, glm::vec3 amb, glm::vec3 dif, glm::vec3 spec,
            float kc, float kl, float kq)
            {
                char name[64];

                std::snprintf(name, sizeof(name), "pointLights[%d].position", i);
                glUniform3f(glGetUniformLocation(lightingShader.Program, name),
                    pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

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

        // Seteo de las puntuales
        for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
            // Intensidades base
            glm::vec3 amb(0.04f), dif(0.90f), spec(0.55f);
            float kc = 1.0f, kl = 0.045f, kq = 0.0075f;

            // Escala por energía de Blender (1000 -> 1.0, 3000 -> 3.0)
            float energyBoost = pointLightEnergy[i] / 1000.0f;
            // Después de calcular energyBoost...
            amb *= globalLightBoost * (0.6f * 0.7f * energyBoost); // antes 0.7
            dif *= globalLightBoost * 0.60f * energyBoost;         // antes 1.0
            spec *= globalLightBoost * 0.40f * energyBoost;         // antes 1.0

            // Menos atenuación para las luces "fuertes"
            if (energyBoost > 1.5f) { kl = 0.028f; kq = 0.0019f; }

            // Heurística por profundidad
            if (pointLightPositions[i].z < 5.0f) {
                dif *= 0.70f; spec *= 0.50f; kl = 0.060f; kq = 0.0170f;
            }
            else if (pointLightPositions[i].z < 15.0f) {
                dif *= 0.90f;
            }

            setPoint(i, amb, dif, spec, kc, kl, kq);
        }

        // ===== Spotlights (arreglo) =====
        for (int i = 0; i < NUM_SPOT_LIGHTS; ++i) {
            char name[64];

            // color por luz * intensidad * boost global
            glm::vec3 amb = 0.02f * gSpotIntensity[i] * globalLightBoost * gSpotColor[i];
            glm::vec3 dif = 1.40f * gSpotIntensity[i] * globalLightBoost * gSpotColor[i];
            glm::vec3 spe = 0.60f * gSpotIntensity[i] * globalLightBoost * gSpotColor[i];

            // Atenuación (igual que el primero)
            const float kc = 1.0f, kl = 0.045f, kq = 0.0075f;

            std::snprintf(name, sizeof(name), "spotLights[%d].position", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name),
                gSpotPos[i].x, gSpotPos[i].y, gSpotPos[i].z);

            std::snprintf(name, sizeof(name), "spotLights[%d].direction", i);
            glUniform3f(glGetUniformLocation(lightingShader.Program, name),
                gSpotDir[i].x, gSpotDir[i].y, gSpotDir[i].z);

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


        // Matrices
        GLint modelLoc = glGetUniformLocation(lightingShader.Program, "model");
        GLint viewLoc = glGetUniformLocation(lightingShader.Program, "view");
        GLint projLoc = glGetUniformLocation(lightingShader.Program, "projection");
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(g_Projection));

        glm::mat4 model(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        museo.Draw(lightingShader);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
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

// --- Input ---
void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action) glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }
}

void MouseCallback(GLFWwindow* window, double xPos, double yPos) {
    if (firstMouse) { lastX = (GLfloat)xPos; lastY = (GLfloat)yPos; firstMouse = false; }
    GLfloat xOffset = (GLfloat)xPos - lastX;
    GLfloat yOffset = lastY - (GLfloat)yPos;
    lastX = (GLfloat)xPos; lastY = (GLfloat)yPos;
    camera.ProcessMouseMovement(xOffset, yOffset);
}

// --- Redimensionamiento: actualiza viewport y proyección ---
void FramebufferSizeCallback(GLFWwindow*, int width, int height) {
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
