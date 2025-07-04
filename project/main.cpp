#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>

#include "tiny_obj_loader.h"

// ========== UTILS MATRICES 4x4 (col-major) ==========
void loadIdentity(float* mat) {
    for (int i=0; i<16; ++i) mat[i] = 0.0f;
    mat[0]=mat[5]=mat[10]=mat[15]=1.0f;
}

void multiplyMatrix(const float* A, const float* B, float* result) {
    for (int col=0; col<4; ++col) {
        for (int row=0; row<4; ++row) {
            result[col*4+row] = 0.0f;
            for (int i=0; i<4; ++i) {
                result[col*4+row] += A[i*4+row]*B[col*4+i];
            }
        }
    }
}

void translationMatrix(float x, float y, float z, float* mat) {
    loadIdentity(mat);
    mat[12] = x; mat[13] = y; mat[14] = z;
}

void scaleMatrix(float sx, float sy, float sz, float* mat) {
    loadIdentity(mat);
    mat[0] = sx;
    mat[5] = sy;
    mat[10] = sz;
}

void rotationMatrixX(float angleDeg, float* mat) {
    float rad = angleDeg * (3.14159265f/180.0f);
    float c = cosf(rad), s = sinf(rad);
    loadIdentity(mat);
    mat[5] = c; mat[6] = s;
    mat[9] = -s; mat[10] = c;
}

void rotationMatrixY(float angleDeg, float* mat) {
    float rad = angleDeg * (3.14159265f/180.0f);
    float c = cosf(rad), s = sinf(rad);
    loadIdentity(mat);
    mat[0] = c; mat[2] = -s;
    mat[8] = s; mat[10] = c;
}

// Normalisation d’un vecteur 3D
void normalize(float* v) {
    float len = sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(len > 0.00001f) {
        v[0]/=len; v[1]/=len; v[2]/=len;
    }
}

// Produit vectoriel v1 x v2 -> result
void cross(const float* v1, const float* v2, float* result) {
    result[0] = v1[1]*v2[2]-v1[2]*v2[1];
    result[1] = v1[2]*v2[0]-v1[0]*v2[2];
    result[2] = v1[0]*v2[1]-v1[1]*v2[0];
}

// Produit scalaire v1 . v2
float dot(const float* v1, const float* v2) {
    return v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2];
}

// Construction de la View Matrix “LookAt”
void lookAt(const float* eye, const float* target, const float* up, float* view) {
    float f[3] = {target[0]-eye[0], target[1]-eye[1], target[2]-eye[2]};
    normalize(f);
    float forward[3] = {-f[0], -f[1], -f[2]};
    float right[3];
    cross(up, forward, right);
    normalize(right);
    float upCorrected[3];
    cross(forward, right, upCorrected);

    loadIdentity(view);
    view[0] = right[0];    view[4] = right[1];    view[8] = right[2];
    view[1] = upCorrected[0]; view[5] = upCorrected[1]; view[9] = upCorrected[2];
    view[2] = forward[0];  view[6] = forward[1];  view[10] = forward[2];
    view[12] = -dot(right, eye);
    view[13] = -dot(upCorrected, eye);
    view[14] = -dot(forward, eye);
}

// Projection perspective
void perspective(float fov, float ratio, float near, float far, float* proj) {
    float f = 1.0f / tanf((fov*3.14159265f/180.0f)/2.0f);
    loadIdentity(proj);
    proj[0] = f/ratio;
    proj[5] = f;
    proj[10] = (far+near)/(near-far);
    proj[11] = -1.0f;
    proj[14] = (2*far*near)/(near-far);
    proj[15] = 0.0f;
}

// ========== CAMERA VARIABLES ==========
bool fpsMode = false; // Orbite par défaut
float eye[3] = {0.0f, 0.0f, 5.0f}; // Position initiale
float yaw = -90.0f; // Rotation Y (horizontal)
float pitch = 0.0f; // Rotation X (vertical)
float fov = 45.0f; // Champ de vision
float rotX = 0.0f, rotY = 0.0f; // Pour mode orbite
float radius = 5.0f; // Rayon orbite
double lastX = 400.0, lastY = 300.0; // Centre de la fenêtre
bool firstMouse = true;

// ========== GESTION SOURIS ==========
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Inversé pour mouvement naturel
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    if (fpsMode) {
        yaw += (float)xoffset;
        pitch += (float)yoffset;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    } else {
        rotY += (float)xoffset;
        rotX += (float)yoffset;
        if (rotX > 90.0f) rotX = 90.0f;
        if (rotX < -90.0f) rotX = -90.0f;
        if (rotY > 180.0f) rotY -= 360.0f;
        if (rotY < -180.0f) rotY += 360.0f;
    }
}

// ========== GESTION MOLETTE ==========
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (fpsMode) {
        fov -= (float)yoffset;
        if (fov < 10.0f) fov = 10.0f;
        if (fov > 90.0f) fov = 90.0f;
    } else {
        radius -= (float)yoffset * 0.5f;
        if (radius < 1.0f) radius = 1.0f;
        if (radius > 10.0f) radius = 10.0f;
    }
}

// ========== GESTION CLAVIER ==========
void processInput(GLFWwindow* window) {
    static double lastToggleTime = 0.0;
    double currentTime = glfwGetTime();
    float deltaTime = 0.016f; // ~60 FPS

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Bascule FPS/Orbite avec touche 'C'
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && currentTime - lastToggleTime > 0.2) {
        fpsMode = !fpsMode;
        firstMouse = true; // Réinitialiser la souris
        lastToggleTime = currentTime;
    }

    if (fpsMode) {
        float speed = 5.0f * deltaTime;
        float front[3], right[3], up[3] = {0.0f, 1.0f, 0.0f};
        float radYaw = yaw * (3.14159265f/180.0f);
        float radPitch = pitch * (3.14159265f/180.0f);
        front[0] = cos(radYaw) * cos(radPitch);
        front[1] = sin(radPitch);
        front[2] = sin(radYaw) * cos(radPitch);
        normalize(front);
        cross(front, up, right);
        normalize(right);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            eye[0] += speed * front[0];
            eye[1] += speed * front[1];
            eye[2] += speed * front[2];
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            eye[0] -= speed * front[0];
            eye[1] -= speed * front[1];
            eye[2] -= speed * front[2];
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            eye[0] += speed * right[0];
            eye[1] += speed * right[1];
            eye[2] += speed * right[2];
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            eye[0] -= speed * right[0];
            eye[1] -= speed * right[1];
            eye[2] -= speed * right[2];
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            eye[1] += speed;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
            eye[1] -= speed;
        }
    } else {
        float sensitivity = 0.05f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            rotX -= sensitivity;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            rotX += sensitivity;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            rotY -= sensitivity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            rotY += sensitivity;
        if (rotX > 90.0f) rotX = 90.0f;
        if (rotX < -90.0f) rotX = -90.0f;
        if (rotY > 180.0f) rotY -= 360.0f;
        if (rotY < -180.0f) rotY += 360.0f;
    }

    // Zoom avec +/- (optionnel)
    if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
        if (fpsMode) {
            fov -= 1.0f;
            if (fov < 10.0f) fov = 10.0f;
        } else {
            radius -= 0.5f;
            if (radius < 1.0f) radius = 1.0f;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
        if (fpsMode) {
            fov += 1.0f;
            if (fov > 90.0f) fov = 90.0f;
        } else {
            radius += 0.5f;
            if (radius > 10.0f) radius = 10.0f;
        }
    }
}

// ========== SHADERS ==========
const char* vertexShaderSource = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;

layout(std140) uniform Matrices {
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

uniform mat4 WorldMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    mat4 WV = ViewMatrix * WorldMatrix;
    FragPos = vec3(WV * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(WV))) * aNormal;
    TexCoord = aTexCoord;
    gl_Position = ProjectionMatrix * WV * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderLambertSource = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 SkyColor;
uniform vec3 GroundColor;
uniform vec3 SkyDirection;
uniform float Ia;

void main() {
    vec3 ka = pow(Ka, vec3(2.2));
    vec3 kd = pow(Kd, vec3(2.2));
    vec3 color = kd;

    float HemisphereFactor = dot(normalize(Normal), SkyDirection) * 0.5 + 0.5;
    vec3 AmbientColor = Ia * mix(GroundColor, SkyColor, HemisphereFactor);
    vec3 ambient = ka * AmbientColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;

    vec3 result = ambient + diffuse;
    FragColor = vec4(result, 1.0);
}
)glsl";

const char* fragmentShaderPhongSource = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform float Ns;
uniform vec3 SkyColor;
uniform vec3 GroundColor;
uniform vec3 SkyDirection;
uniform float Ia;

void main() {
    vec3 ka = pow(Ka, vec3(2.2));
    vec3 kd = pow(Kd, vec3(2.2));
    vec3 ks = pow(Ks, vec3(2.2));
    vec3 color = kd;

    float HemisphereFactor = dot(normalize(Normal), SkyDirection) * 0.5 + 0.5;
    vec3 AmbientColor = Ia * mix(GroundColor, SkyColor, HemisphereFactor);
    vec3 ambient = ka * AmbientColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), Ns);
    vec3 specular = ks * spec;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
)glsl";

const char* fragmentShaderColorSource = R"glsl(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 SkyColor;
uniform vec3 GroundColor;
uniform vec3 SkyDirection;
uniform float Ia;

void main() {
    vec3 ka = pow(Ka, vec3(2.2));
    vec3 kd = pow(Kd, vec3(2.2));
    vec3 color = kd;

    float HemisphereFactor = dot(normalize(Normal), SkyDirection) * 0.5 + 0.5;
    vec3 AmbientColor = Ia * mix(GroundColor, SkyColor, HemisphereFactor);
    vec3 ambient = ka * AmbientColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;

    vec3 result = ambient + diffuse;
    FragColor = vec4(result, 1.0);
}
)glsl";

// ========== FONCTION AIDE COMPIL SHADERS ==========
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compilation error:\n" << log << std::endl;
    }
    return shader;
}

GLuint createProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint success; glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return prog;
}

// Structure pour stocker les données du mesh
struct Mesh {
    std::vector<float> vertices;
    std::vector<GLuint> indices;
    GLuint VAO, VBO, EBO;
    float WorldMatrix[16];
    GLuint shaderProgram;
    float Ka[3];
    float Kd[3];
    float Ks[3];
    float Ns;
};

// Fonction pour charger un OBJ avec TinyOBJLoader
bool loadOBJ(const std::string& path, Mesh& mesh) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string base_dir = path.substr(0, path.find_last_of("/\\"));
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), base_dir.c_str(), true);

    if (!warn.empty()) std::cerr << "Warning: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Error: " << err << std::endl;
    if (!ret) return false;

    std::vector<float> vertexData;
    std::vector<GLuint> indexData;

    for (const auto& shape : shapes) {
        for (size_t i = 0; i < shape.mesh.indices.size(); ++i) {
            const auto& index = shape.mesh.indices[i];
            vertexData.push_back(attrib.vertices[3 * index.vertex_index + 0]);
            vertexData.push_back(attrib.vertices[3 * index.vertex_index + 1]);
            vertexData.push_back(attrib.vertices[3 * index.vertex_index + 2]);

            if (index.normal_index >= 0) {
                vertexData.push_back(attrib.normals[3 * index.normal_index + 0]);
                vertexData.push_back(attrib.normals[3 * index.normal_index + 1]);
                vertexData.push_back(attrib.normals[3 * index.normal_index + 2]);
            } else {
                float normal[3] = {0.0f, 0.0f, 0.0f};
                if (i >= 2 && i % 3 == 2) {
                    size_t idx0 = shape.mesh.indices[i - 2].vertex_index;
                    size_t idx1 = shape.mesh.indices[i - 1].vertex_index;
                    size_t idx2 = shape.mesh.indices[i].vertex_index;
                    float v0[3] = {attrib.vertices[3 * idx0], attrib.vertices[3 * idx0 + 1], attrib.vertices[3 * idx0 + 2]};
                    float v1[3] = {attrib.vertices[3 * idx1], attrib.vertices[3 * idx1 + 1], attrib.vertices[3 * idx1 + 2]};
                    float v2[3] = {attrib.vertices[3 * idx2], attrib.vertices[3 * idx2 + 1], attrib.vertices[3 * idx2 + 2]};
                    float e1[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
                    float e2[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
                    cross(e1, e2, normal);
                    normalize(normal);
                    vertexData[vertexData.size() - 9 + 3] = normal[0];
                    vertexData[vertexData.size() - 9 + 4] = normal[1];
                    vertexData[vertexData.size() - 9 + 5] = normal[2];
                    vertexData[vertexData.size() - 6 + 3] = normal[0];
                    vertexData[vertexData.size() - 6 + 4] = normal[1];
                    vertexData[vertexData.size() - 6 + 5] = normal[2];
                    vertexData[vertexData.size() - 3 + 3] = normal[0];
                    vertexData[vertexData.size() - 3 + 4] = normal[1];
                    vertexData[vertexData.size() - 3 + 5] = normal[2];
                } else {
                    vertexData.push_back(0.0f);
                    vertexData.push_back(0.0f);
                    vertexData.push_back(1.0f);
                }
            }

            if (index.texcoord_index >= 0) {
                vertexData.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                vertexData.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
            } else {
                vertexData.push_back(0.0f);
                vertexData.push_back(0.0f);
            }

            indexData.push_back(static_cast<GLuint>(indexData.size()));
        }

        if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids[0] >= 0) {
            const auto& mat = materials[shape.mesh.material_ids[0]];
            mesh.Ka[0] = mat.ambient[0];  mesh.Ka[1] = mat.ambient[1];  mesh.Ka[2] = mat.ambient[2];
            mesh.Kd[0] = mat.diffuse[0];  mesh.Kd[1] = mat.diffuse[1];  mesh.Kd[2] = mat.diffuse[2];
            mesh.Ks[0] = mat.specular[0]; mesh.Ks[1] = mat.specular[1]; mesh.Ks[2] = mat.specular[2];
            mesh.Ns = mat.shininess;
        } else {
            mesh.Ka[0] = 0.1f; mesh.Ka[1] = 0.1f; mesh.Ka[2] = 0.1f;
            mesh.Kd[0] = 0.8f; mesh.Kd[1] = 0.8f; mesh.Kd[2] = 0.8f;
            mesh.Ks[0] = 0.5f; mesh.Ks[1] = 0.5f; mesh.Ks[2] = 0.5f;
            mesh.Ns = 32.0f;
        }
    }

    mesh.vertices = vertexData;
    mesh.indices = indexData;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(GLuint), mesh.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return true;
}

// ========== MAIN ==========
int main() {
    if(!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800,600,"Scene 3D avec OBJ", nullptr, nullptr);
    if(!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        std::cerr << "Failed to init GLEW\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    GLuint lambertProgram = createProgram(vertexShaderSource, fragmentShaderLambertSource);
    GLuint phongProgram = createProgram(vertexShaderSource, fragmentShaderPhongSource);
    GLuint colorProgram = createProgram(vertexShaderSource, fragmentShaderColorSource);

    // Configurer UBO pour ViewMatrix et ProjectionMatrix
    GLuint uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * 16 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Lier UBO au point de liaison 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);
    GLuint uboIndex = glGetUniformBlockIndex(lambertProgram, "Matrices");
    glUniformBlockBinding(lambertProgram, uboIndex, 0);
    uboIndex = glGetUniformBlockIndex(phongProgram, "Matrices");
    glUniformBlockBinding(phongProgram, uboIndex, 0);
    uboIndex = glGetUniformBlockIndex(colorProgram, "Matrices");
    glUniformBlockBinding(colorProgram, uboIndex, 0);

    std::vector<Mesh> meshes(3);

    // Cube
    if (!loadOBJ("models/cube.obj", meshes[0])) {
        std::cerr << "Failed to load cube.obj\n";
        glfwTerminate();
        return -1;
    }
    float transCube[16], scaleCube[16], worldCube[16];
    translationMatrix(-2.0f, 0.0f, 0.0f, transCube);
    scaleMatrix(1.0f, 1.0f, 1.0f, scaleCube);
    multiplyMatrix(transCube, scaleCube, worldCube);
    for (int i = 0; i < 16; ++i) meshes[0].WorldMatrix[i] = worldCube[i];
    meshes[0].shaderProgram = colorProgram;

    // Cylindre
    if (!loadOBJ("models/cylinder.obj", meshes[1])) {
        std::cerr << "Failed to load cylinder.obj\n";
        glfwTerminate();
        return -1;
    }
    float transCylinder[16], scaleCylinder[16], worldCylinder[16];
    translationMatrix(0.0f, 0.0f, 0.0f, transCylinder);
    scaleMatrix(1.0f, 1.0f, 1.0f, scaleCylinder);
    multiplyMatrix(transCylinder, scaleCylinder, worldCylinder);
    for (int i = 0; i < 16; ++i) meshes[1].WorldMatrix[i] = worldCylinder[i];
    meshes[1].shaderProgram = phongProgram;

    // Pyramide
    if (!loadOBJ("models/pyramid.obj", meshes[2])) {
        std::cerr << "Failed to load pyramid.obj\n";
        glfwTerminate();
        return -1;
    }
    float transPyramid[16], rotPyramid[16], scalePyramid[16], tempPyramid[16], worldPyramid[16];
    translationMatrix(2.0f, 0.0f, 0.0f, transPyramid);
    rotationMatrixY(45.0f, rotPyramid);
    scaleMatrix(1.0f, 1.5f, 1.0f, scalePyramid);
    multiplyMatrix(transPyramid, rotPyramid, tempPyramid);
    multiplyMatrix(tempPyramid, scalePyramid, worldPyramid);
    for (int i = 0; i < 16; ++i) meshes[2].WorldMatrix[i] = worldPyramid[i];
    meshes[2].shaderProgram = lambertProgram;

    float projection[16];
    perspective(fov, 800.0f / 600.0f, 0.1f, 100.0f, projection);

    float view[16];
    float target[3] = {0.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};

    while(!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        processInput(window);

        // Calculer la view matrix
        if (fpsMode) {
            float front[3];
            float radYaw = yaw * (3.14159265f/180.0f);
            float radPitch = pitch * (3.14159265f/180.0f);
            front[0] = cos(radYaw) * cos(radPitch);
            front[1] = sin(radPitch);
            front[2] = sin(radYaw) * cos(radPitch);
            target[0] = eye[0] + front[0];
            target[1] = eye[1] + front[1];
            target[2] = eye[2] + front[2];
            lookAt(eye, target, up, view);
        } else {
            float camX = radius * cos(rotX * 3.14159265f / 180.0f) * sin(rotY * 3.14159265f / 180.0f);
            float camY = radius * sin(rotX * 3.14159265f / 180.0f);
            float camZ = radius * cos(rotX * 3.14159265f / 180.0f) * cos(rotY * 3.14159265f / 180.0f);
            eye[0] = camX;
            eye[1] = camY;
            eye[2] = camZ;
            lookAt(eye, target, up, view);
        }

        // Projection FOV
        perspective(fov, 800.0f / 600.0f, 0.1f, 100.0f, projection);

        // UBO
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, 16 * sizeof(float), view);
        glBufferSubData(GL_UNIFORM_BUFFER, 16 * sizeof(float), 16 * sizeof(float), projection);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        for (const auto& mesh : meshes) {
            glUseProgram(mesh.shaderProgram);
            GLint locWorld = glGetUniformLocation(mesh.shaderProgram, "WorldMatrix");
            GLint locLight = glGetUniformLocation(mesh.shaderProgram, "lightPos");
            GLint locViewPos = glGetUniformLocation(mesh.shaderProgram, "viewPos");
            GLint locKa = glGetUniformLocation(mesh.shaderProgram, "Ka");
            GLint locKd = glGetUniformLocation(mesh.shaderProgram, "Kd");
            GLint locKs = glGetUniformLocation(mesh.shaderProgram, "Ks");
            GLint locNs = glGetUniformLocation(mesh.shaderProgram, "Ns");
            GLint locSkyColor = glGetUniformLocation(mesh.shaderProgram, "SkyColor");
            GLint locGroundColor = glGetUniformLocation(mesh.shaderProgram, "GroundColor");
            GLint locSkyDirection = glGetUniformLocation(mesh.shaderProgram, "SkyDirection");
            GLint locIa = glGetUniformLocation(mesh.shaderProgram, "Ia");

            glUniformMatrix4fv(locWorld, 1, GL_FALSE, mesh.WorldMatrix);
            glUniform3f(locLight, 1.0f, 1.0f, 2.0f);
            glUniform3f(locViewPos, eye[0], eye[1], eye[2]);
            glUniform3fv(locKa, 1, mesh.Ka);
            glUniform3fv(locKd, 1, mesh.Kd);
            glUniform3f(locSkyColor, 0.5f, 0.7f, 1.0f); // Bleu ciel
            glUniform3f(locGroundColor, 0.3f, 0.5f, 0.2f); // Vert sol
            glUniform3f(locSkyDirection, 0.0f, 1.0f, 0.0f); // Vers le haut
            glUniform1f(locIa, 0.2f); // Intensité ambiante
            if (mesh.shaderProgram == phongProgram) {
                glUniform3fv(locKs, 1, mesh.Ks);
                glUniform1f(locNs, mesh.Ns);
            }

            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& mesh : meshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO);
        glDeleteBuffers(1, &mesh.EBO);
    }
    glDeleteBuffers(1, &uboMatrices);
    glDeleteProgram(lambertProgram);
    glDeleteProgram(phongProgram);
    glDeleteProgram(colorProgram);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}