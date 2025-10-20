#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SOIL2/SOIL2.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.h"
#include "Shader.h"

using namespace std;

static inline string JoinPath(const string& a, const string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    string out = a;
    // Normaliza separadores
    auto norm = [](string s) {
        replace(s.begin(), s.end(), '\\', '/');
        return s;
        };
    string nb = norm(b);
    string na = norm(a);
    if (na.back() == '/') out = na + nb;
    else                  out = na + '/' + nb;
    return out;
}

static inline bool FileEndsWithI(const string& s, const string& suffix) {
    if (suffix.size() > s.size()) return false;
    return equal(suffix.rbegin(), suffix.rend(), s.rbegin(),
        [](char a, char b) { return tolower(a) == tolower(b); });
}

GLuint TextureFromFile(const char* path, const string& directory);

class Model {
public:
    Model(GLchar* path) {
        this->loadModel(path);
    }

    void Draw(Shader shader) {
        for (GLuint i = 0; i < this->meshes.size(); i++) {
            this->meshes[i].Draw(shader);
        }
    }

private:
    vector<Mesh> meshes;
    string directory;
    vector<Texture> textures_loaded;

    void loadModel(string path) {
        // Lee con ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals
        );

        if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }

        // Carpeta base del archivo
        {
            // Normaliza separadores
            replace(path.begin(), path.end(), '\\', '/');
            size_t pos = path.find_last_of('/');
            this->directory = (pos == string::npos) ? string() : path.substr(0, pos);
        }

        this->processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (GLuint i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            this->meshes.push_back(this->processMesh(mesh, scene));
        }
        for (GLuint i = 0; i < node->mNumChildren; i++) {
            this->processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        vector<Vertex> vertices;
        vector<GLuint> indices;
        vector<Texture> textures;

        for (GLuint i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 v;

            v.x = mesh->mVertices[i].x; v.y = mesh->mVertices[i].y; v.z = mesh->mVertices[i].z;
            vertex.Position = v;

            if (mesh->HasNormals()) {
                v.x = mesh->mNormals[i].x; v.y = mesh->mNormals[i].y; v.z = mesh->mNormals[i].z;
                vertex.Normal = v;
            }
            else {
                vertex.Normal = glm::vec3(0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                glm::vec2 t;
                t.x = mesh->mTextureCoords[0][i].x;
                t.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = t;
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        for (GLuint i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (GLuint j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            // Diffuse
            vector<Texture> diffuseMaps = this->loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

            // Specular
            vector<Texture> specularMaps = this->loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

            // Normals (si están como HEIGHT o NORMALS)
            vector<Texture> normalMaps = this->loadMaterialTextures(material, aiTextureType_NORMALS, "texture_normal");
            if (normalMaps.empty()) {
                vector<Texture> heightAsNormal = this->loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
                normalMaps.insert(normalMaps.end(), heightAsNormal.begin(), heightAsNormal.end());
            }
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        }

        return Mesh(vertices, indices, textures);
    }

    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName) {
        vector<Texture> textures;

        for (GLuint i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            // Evita recargar la misma textura
            bool skip = false;
            for (const auto& loaded : textures_loaded) {
                if (loaded.path == str) {
                    textures.push_back(loaded);
                    skip = true;
                    break;
                }
            }

            if (!skip) {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                if (texture.id == 0) {
                    // Fallback: si falla, crea un tex de 1x1 blanco para no crashear el draw
                    GLuint fallback;
                    glGenTextures(1, &fallback);
                    glBindTexture(GL_TEXTURE_2D, fallback);
                    unsigned char white[4] = { 255, 255, 255, 255 };
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    texture.id = fallback;
                    cerr << "[Model] WARNING: usando textura fallback para '" << str.C_Str() << "'\n";
                }
                texture.type = typeName;
                texture.path = str;

                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }

        return textures;
    }
};

// --------- UTILIDAD PRINCIPAL DE CARGA DE TEXTURAS ----------
// Carga una imagen desde disco resolviendo ruta relativa al .obj/.mtl.
// Detecta canales y ajusta formato, hace checks y evita crashes.
GLuint TextureFromFile(const char* path, const string& directory)
{
    // 1) Resuelve ruta
    string rel = path ? string(path) : string();
    // Algunos exporters anteponen "./"
    if (rel.rfind("./", 0) == 0) rel = rel.substr(2);
    string filename = JoinPath(directory, rel);

    // 2) Genera el ID
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        cerr << "[TextureFromFile] glGenTextures fallo\n";
        return 0;
    }

    // 3) Carga con SOIL2 sin forzar número de canales
    int width = 0, height = 0, channels = 0;
    unsigned char* image = SOIL_load_image(filename.c_str(), &width, &height, &channels, 0);

    if (!image) {
        cerr << "[TextureFromFile] SOIL fallo al cargar '" << filename << "': "
            << SOIL_last_result() << "\n";
        return 0;
    }
    if (width <= 0 || height <= 0) {
        cerr << "[TextureFromFile] dimensiones invalidas para '" << filename << "'\n";
        SOIL_free_image_data(image);
        return 0;
    }

    // 4) Selecciona formato según canales
    GLenum srcFmt;
    switch (channels) {
    case 4: srcFmt = GL_RGBA; break;
    case 3: srcFmt = GL_RGB;  break;
    case 2: // no estándar; trátalo como RG
        srcFmt = GL_RG;
        break;
    default: srcFmt = GL_RED; break; // 1 canal u otro
    }

    // 5) Sube textura con alineación segura (evita lecturas fuera de rango)
    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // crítico cuando width*channels no es múltiplo de 4

    // Si quieres espacio de color sRGB, podrías usar GL_SRGB/GL_SRGB_ALPHA como internalFormat
    GLenum internalFmt = (srcFmt == GL_RGB) ? GL_RGB :
        (srcFmt == GL_RGBA) ? GL_RGBA : srcFmt;

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, width, height, 0, srcFmt, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    // 6) Parámetros por defecto razonables
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    SOIL_free_image_data(image);

    // Log útil (puedes comentar si molesta)
    // cerr << "[TextureFromFile] OK: " << filename << " (" << width << "x" << height
    //      << ", channels=" << channels << ")\n";

    return textureID;
}
