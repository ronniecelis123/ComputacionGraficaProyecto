#pragma once

// Std. Includes
#include <vector>
#include <algorithm>

// GL Includes
#define GLEW_STATIC
#include <GL/glew.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// Default camera values
const GLfloat YAW = -90.0f;
const GLfloat PITCH = 0.0f;
const GLfloat SPEED = 2.0f;
const GLfloat SENSITIVTY = 0.25f;
const GLfloat ZOOM = 45.0f;

// An abstract camera class that processes input and calculates the corresponding Eular Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // Constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), GLfloat yaw = YAW, GLfloat pitch = PITCH)
        : front(glm::vec3(0.0f, 0.0f, -1.0f)), movementSpeed(SPEED), mouseSensitivity(SENSITIVTY), zoom(ZOOM)
    {
        this->position = position;
        this->worldUp = up;
        this->yaw = yaw;
        this->pitch = pitch;
        this->updateCameraVectors();
    }

    // Constructor with scalar values
    Camera(GLfloat posX, GLfloat posY, GLfloat posZ, GLfloat upX, GLfloat upY, GLfloat upZ, GLfloat yaw, GLfloat pitch)
        : front(glm::vec3(0.0f, 0.0f, -1.0f)), movementSpeed(SPEED), mouseSensitivity(SENSITIVTY), zoom(ZOOM)
    {
        this->position = glm::vec3(posX, posY, posZ);
        this->worldUp = glm::vec3(upX, upY, upZ);
        this->yaw = yaw;
        this->pitch = pitch;
        this->updateCameraVectors();
    }

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(this->position, this->position + this->front, this->up);
    }

    // Processes input received from any keyboard-like input system.
    void ProcessKeyboard(Camera_Movement direction, GLfloat deltaTime)
    {
        GLfloat velocity = this->movementSpeed * deltaTime;

        if (direction == FORWARD)
            this->position += this->front * velocity;

        if (direction == BACKWARD)
            this->position -= this->front * velocity;

        if (direction == LEFT)
            this->position -= this->right * velocity;

        if (direction == RIGHT)
            this->position += this->right * velocity;
    }

    // Processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(GLfloat xOffset, GLfloat yOffset, GLboolean constrainPitch = true)
    {
        xOffset *= this->mouseSensitivity;
        yOffset *= this->mouseSensitivity;

        this->yaw += xOffset;
        this->pitch += yOffset;

        // Make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (this->pitch > 89.0f)  this->pitch = 89.0f;
            if (this->pitch < -89.0f) this->pitch = -89.0f;
        }

        // Update Front, Right and Up Vectors using the updated Euler angles
        this->updateCameraVectors();
    }

    // Processes input received from a mouse scroll-wheel event (optional zoom handling)
    void ProcessMouseScroll(GLfloat yOffset)
    {
        // Ajusta el zoom interno (si quieres usarlo en tu proyección)
        this->zoom -= yOffset;
        this->zoom = std::max(1.0f, std::min(100.0f, this->zoom));
    }

    // ---- NUEVO: setters/getters ----
    void SetSpeed(GLfloat speed) { this->movementSpeed = speed; }
    GLfloat GetSpeed() const { return this->movementSpeed; }

    void SetMouseSensitivity(GLfloat sens) { this->mouseSensitivity = sens; }
    GLfloat GetMouseSensitivity() const { return this->mouseSensitivity; }

    void SetYawPitch(GLfloat newYaw, GLfloat newPitch, bool constrainPitch = true)
    {
        this->yaw = newYaw;
        this->pitch = newPitch;
        if (constrainPitch)
        {
            if (this->pitch > 89.0f)  this->pitch = 89.0f;
            if (this->pitch < -89.0f) this->pitch = -89.0f;
        }
        this->updateCameraVectors();
    }

    GLfloat GetZoom() const { return this->zoom; }
    glm::vec3 GetPosition() const { return this->position; }
    glm::vec3 GetFront() const { return this->front; }

private:
    // Camera Attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler Angles
    GLfloat yaw;
    GLfloat pitch;

    // Camera options
    GLfloat movementSpeed;
    GLfloat mouseSensitivity;
    GLfloat zoom;

    // Calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // Calculate the new Front vector
        glm::vec3 f;
        f.x = cos(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
        f.y = sin(glm::radians(this->pitch));
        f.z = sin(glm::radians(this->yaw)) * cos(glm::radians(this->pitch));
        this->front = glm::normalize(f);
        // Also re-calculate the Right and Up vector
        this->right = glm::normalize(glm::cross(this->front, this->worldUp));
        this->up = glm::normalize(glm::cross(this->right, this->front));
    }
};
