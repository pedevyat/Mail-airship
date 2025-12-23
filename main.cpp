#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

// Для OpenGL
#include <GL/glew.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>



// Структуры для хранения данных
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 baseColor;
    bool hasIndices;
};

struct Cloud {
    glm::vec3 position;
    glm::vec3 velocity;
    float flashTimer;
    float flashDuration;
    bool isFlashing;
    float oscillation;
};

struct Balloon {
    glm::vec3 position;
    glm::vec3 color;
    float oscillation;
};

// Глобальные переменные
int width = 1200, height = 800;
glm::mat4 projection, view;
glm::vec3 airshipPos = glm::vec3(0.0f, 15.0f, 0.0f);
float airshipYaw = 0.0f;
float airshipSpeed = 15.0f;

// Шейдерные программы
GLuint shaderProgram;
GLuint cloudShaderProgram;

// Модели
Model groundModel, treeModel, airshipModel, cloudModel, balloonModel;
std::vector<Cloud> clouds;
std::vector<Balloon> balloons;

bool spotlightOn = false;
float timeElapsed = 0.0f;

// Прототипы функций
Model createGroundModel();
Model createTreeModel();
Model createAirshipModel();
Model createCloudModel();
Model createBalloonModel();
void initClouds();
void initBalloons();
void renderModel(const Model& model, const glm::mat4& modelMatrix, const glm::vec3& color);
void renderCloud(const Cloud& cloud);
void renderBalloon(const Balloon& balloon);
void processInput(sf::Window& window, float deltaTime);
void updateClouds(float deltaTime);
void updateBalloons(float deltaTime);
GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);

// Функция создания шейдерной программы
GLuint createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    // Вершинный шейдер
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vertexSourcePtr = vertexSource.c_str();
    glShaderSource(vertexShader, 1, &vertexSourcePtr, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    // Фрагментный шейдер
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragmentSourcePtr = fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &fragmentSourcePtr, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    // Шейдерная программа
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

// Шейдеры
std::string mainVertexShader = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;
    layout(location = 2) in vec3 aColor;

    out vec3 FragPos;
    out vec3 Normal;
    out vec3 Color;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = aNormal;
        Color = aColor;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

std::string mainFragmentShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Normal;
    in vec3 Color;

    out vec4 FragColor;

    uniform vec3 lightDir;
    uniform vec3 lightColor;
    uniform vec3 viewPos;
    uniform bool useSpotlight;

    void main() {
        // Основное освещение
        vec3 norm = normalize(Normal);
        vec3 lightDirection = normalize(-lightDir);
        float diff = max(dot(norm, lightDirection), 0.0);
        vec3 diffuse = diff * lightColor;

        // Фоновое освещение
        vec3 ambient = 0.3 * lightColor;

        // Финальный цвет
        vec3 result = (ambient + diffuse) * Color;
        FragColor = vec4(result, 1.0);
    }
)";

std::string cloudVertexShader = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;
    layout(location = 2) in vec3 aColor;

    out vec3 FragPos;
    out vec3 Color;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform float time;

    void main() {
        // Легкое покачивание туч
        vec3 pos = aPos;
        pos.y += sin(time * 2.0 + aPos.x * 0.1) * 0.2;

        FragPos = vec3(model * vec4(pos, 1.0));
        Color = aColor;
        gl_Position = projection * view * model * vec4(pos, 1.0);
    }
)";

std::string cloudFragmentShader = R"(
    #version 330 core
    in vec3 FragPos;
    in vec3 Color;

    out vec4 FragColor;

    uniform float time;
    uniform bool isFlashing;

    void main() {
        vec3 color = Color;

        // Мерцание для туч
        if (isFlashing) {
            float flash = sin(time * 30.0) * 0.5 + 0.5;
            color = mix(color, vec3(1.0, 1.0, 0.8), flash * 0.5);
        }

        // Прозрачность для туч
        FragColor = vec4(color, 0.7);
    }
)";

// Создание моделей
Model createGroundModel() {
    Model model;
    model.baseColor = glm::vec3(0.2f, 0.6f, 0.3f);
    model.hasIndices = true;

    float groundSize = 100.0f;

    // Создаем простой квадрат для земли
    Vertex vertices[] = {
        {{-groundSize, 0.0f, -groundSize}, {0.0f, 1.0f, 0.0f}, {0.2f, 0.6f, 0.3f}},
        {{groundSize, 0.0f, -groundSize}, {0.0f, 1.0f, 0.0f}, {0.2f, 0.6f, 0.3f}},
        {{groundSize, 0.0f, groundSize}, {0.0f, 1.0f, 0.0f}, {0.2f, 0.6f, 0.3f}},
        {{-groundSize, 0.0f, groundSize}, {0.0f, 1.0f, 0.0f}, {0.2f, 0.6f, 0.3f}}
    };

    unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    model.vertices = std::vector<Vertex>(vertices, vertices + 4);
    model.indices = std::vector<unsigned int>(indices, indices + 6);

    return model;
}

Model createTreeModel() {
    Model model;
    model.baseColor = glm::vec3(0.0f, 0.5f, 0.0f);
    model.hasIndices = true;

    // Простая пирамида для ёлки
    float height = 8.0f;
    float base = 3.0f;

    // Вершины для пирамиды
    Vertex vertices[] = {
        // Основание
        {{-base, 0.0f, -base}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.3f, 0.0f}},
        {{base, 0.0f, -base}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.3f, 0.0f}},
        {{base, 0.0f, base}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.3f, 0.0f}},
        {{-base, 0.0f, base}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.3f, 0.0f}},

        // Вершина
        {{0.0f, height, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.7f, 0.0f}}
    };

    unsigned int indices[] = {
        // Боковые грани
        0, 1, 4,
        1, 2, 4,
        2, 3, 4,
        3, 0, 4,

        // Основание
        0, 1, 2,
        0, 2, 3
    };

    model.vertices = std::vector<Vertex>(vertices, vertices + 5);
    model.indices = std::vector<unsigned int>(indices, indices + 18);

    return model;
}

Model createAirshipModel() {
    Model model;
    model.baseColor = glm::vec3(0.8f, 0.2f, 0.2f);
    model.hasIndices = true;

    // Простой эллипсоид для дирижабля
    float radiusX = 3.0f;
    float radiusY = 1.5f;
    float radiusZ = 6.0f;
    int slices = 16;
    int stacks = 8;

    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)i / stacks * glm::pi<float>();

        for (int j = 0; j <= slices; ++j) {
            float theta = (float)j / slices * 2.0f * glm::pi<float>();

            Vertex v;
            v.position = glm::vec3(
                radiusX * sin(phi) * cos(theta),
                radiusY * cos(phi),
                radiusZ * sin(phi) * sin(theta)
            );

            v.normal = glm::normalize(v.position);
            v.color = glm::vec3(0.8f, 0.2f, 0.2f);

            model.vertices.push_back(v);
        }
    }

    // Индексы
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;

            model.indices.push_back(first);
            model.indices.push_back(second);
            model.indices.push_back(first + 1);

            model.indices.push_back(second);
            model.indices.push_back(second + 1);
            model.indices.push_back(first + 1);
        }
    }

    return model;
}

Model createCloudModel() {
    Model model;
    model.baseColor = glm::vec3(0.9f, 0.9f, 0.9f);
    model.hasIndices = true;

    // Простая сфера для тучи
    float radius = 2.0f;
    int slices = 8;
    int stacks = 8;

    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)i / stacks * glm::pi<float>();

        for (int j = 0; j <= slices; ++j) {
            float theta = (float)j / slices * 2.0f * glm::pi<float>();

            Vertex v;
            v.position = glm::vec3(
                radius * sin(phi) * cos(theta),
                radius * cos(phi),
                radius * sin(phi) * sin(theta)
            );

            v.normal = glm::normalize(v.position);
            v.color = glm::vec3(0.9f, 0.9f, 0.9f);

            model.vertices.push_back(v);
        }
    }

    // Индексы
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;

            model.indices.push_back(first);
            model.indices.push_back(second);
            model.indices.push_back(first + 1);

            model.indices.push_back(second);
            model.indices.push_back(second + 1);
            model.indices.push_back(first + 1);
        }
    }

    return model;
}

Model createBalloonModel() {
    Model model;
    model.baseColor = glm::vec3(1.0f, 0.0f, 0.0f);
    model.hasIndices = true;

    // Простая сфера для воздушного шара
    float radius = 1.5f;
    int slices = 12;
    int stacks = 12;

    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)i / stacks * glm::pi<float>();

        for (int j = 0; j <= slices; ++j) {
            float theta = (float)j / slices * 2.0f * glm::pi<float>();

            Vertex v;
            v.position = glm::vec3(
                radius * sin(phi) * cos(theta),
                radius * cos(phi) + radius,
                radius * sin(phi) * sin(theta)
            );

            v.normal = glm::normalize(v.position);
            v.color = glm::vec3(1.0f, 0.0f, 0.0f);

            model.vertices.push_back(v);
        }
    }

    // Индексы
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;

            model.indices.push_back(first);
            model.indices.push_back(second);
            model.indices.push_back(first + 1);

            model.indices.push_back(second);
            model.indices.push_back(second + 1);
            model.indices.push_back(first + 1);
        }
    }

    return model;
}

void initClouds() {
    srand(time(nullptr));

    for (int i = 0; i < 8; ++i) {
        Cloud cloud;
        cloud.position = glm::vec3(
            (rand() % 200 - 100),
            30.0f + (rand() % 20),
            (rand() % 200 - 100)
        );
        cloud.velocity = glm::vec3(
            (rand() % 100 - 50) * 0.01f,
            0.0f,
            (rand() % 100 - 50) * 0.01f
        );
        cloud.flashTimer = 0.0f;
        cloud.flashDuration = 2.0f + (rand() % 100) * 0.01f;
        cloud.isFlashing = false;
        cloud.oscillation = (rand() % 100) * 0.01f * glm::pi<float>();

        clouds.push_back(cloud);
    }
}

void initBalloons() {
    srand(time(nullptr));

    for (int i = 0; i < 10; ++i) {
        Balloon balloon;
        balloon.position = glm::vec3(
            (rand() % 180 - 90),
            10.0f + (rand() % 20),
            (rand() % 180 - 90)
        );
        balloon.color = glm::vec3(
            (rand() % 100) * 0.01f,
            (rand() % 100) * 0.01f,
            (rand() % 100) * 0.01f
        );
        balloon.oscillation = 0.0f;

        balloons.push_back(balloon);
    }
}

void renderModel(const Model& model, const glm::mat4& modelMatrix, const glm::vec3& color) {
    glUseProgram(shaderProgram);

    // Установка uniform-переменных
    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLuint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");
    GLuint lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    GLuint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLuint spotlightLoc = glGetUniformLocation(shaderProgram, "useSpotlight");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(lightDirLoc, -0.5f, -1.0f, -0.3f);
    glUniform3f(lightColorLoc, 1.0f, 1.0f, 0.95f);
    glUniform3f(viewPosLoc, airshipPos.x, airshipPos.y, airshipPos.z);
    glUniform1i(spotlightLoc, spotlightOn ? 1 : 0);

    // Создание VBO и VAO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(Vertex), model.vertices.data(), GL_STATIC_DRAW);

    // Позиция
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Нормаль
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // Цвет
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);

    if (model.hasIndices && !model.indices.empty()) {
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

        glDrawElements(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT, 0);

        glDeleteBuffers(1, &EBO);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, model.vertices.size());
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void renderCloud(const Cloud& cloud) {
    glUseProgram(cloudShaderProgram);

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, cloud.position);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(2.0f, 1.0f, 2.0f));

    // Установка uniform-переменных
    GLuint modelLoc = glGetUniformLocation(cloudShaderProgram, "model");
    GLuint viewLoc = glGetUniformLocation(cloudShaderProgram, "view");
    GLuint projLoc = glGetUniformLocation(cloudShaderProgram, "projection");
    GLuint timeLoc = glGetUniformLocation(cloudShaderProgram, "time");
    GLuint flashLoc = glGetUniformLocation(cloudShaderProgram, "isFlashing");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(timeLoc, timeElapsed);
    glUniform1i(flashLoc, cloud.isFlashing ? 1 : 0);

    // Создание VBO и VAO для тучи
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, cloudModel.vertices.size() * sizeof(Vertex), cloudModel.vertices.data(), GL_STATIC_DRAW);

    // Атрибуты
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glEnableVertexAttribArray(2);

    if (cloudModel.hasIndices && !cloudModel.indices.empty()) {
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cloudModel.indices.size() * sizeof(unsigned int), cloudModel.indices.data(), GL_STATIC_DRAW);

        glDrawElements(GL_TRIANGLES, cloudModel.indices.size(), GL_UNSIGNED_INT, 0);

        glDeleteBuffers(1, &EBO);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, cloudModel.vertices.size());
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void renderBalloon(const Balloon& balloon) {
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 pos = balloon.position;
    pos.y += sin(balloon.oscillation) * 0.5f; // Легкое покачивание
    modelMatrix = glm::translate(modelMatrix, pos);

    renderModel(balloonModel, modelMatrix, balloon.color);
}

void processInput(sf::Window& window, float deltaTime) {
    // Проверяем события
    while (auto event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }

        if (auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
            if (keyEvent->scancode == sf::Keyboard::Scan::Escape) {
                window.close();
            }

            if (keyEvent->scancode == sf::Keyboard::Scan::F) {
                spotlightOn = !spotlightOn;
                std::cout << "Прожектор: " << (spotlightOn ? "ВКЛ" : "ВЫКЛ") << std::endl;
            }
        }
    }

    // Управление дирижаблем
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W))
        airshipPos.z -= airshipSpeed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S))
        airshipPos.z += airshipSpeed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A))
        airshipPos.x -= airshipSpeed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D))
        airshipPos.x += airshipSpeed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))
        airshipPos.y += airshipSpeed * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift))
        airshipPos.y -= airshipSpeed * deltaTime;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Left))
        airshipYaw += 1.5f * deltaTime;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Right))
        airshipYaw -= 1.5f * deltaTime;
}

void updateClouds(float deltaTime) {
    for (auto& cloud : clouds) {
        // Движение по тригонометрической траектории
        cloud.position.x += sin(timeElapsed + cloud.oscillation) * 0.5f * deltaTime;
        cloud.position.z += cos(timeElapsed + cloud.oscillation) * 0.5f * deltaTime;
        cloud.position.y += sin(timeElapsed * 0.7f + cloud.oscillation * 2.0f) * 0.2f * deltaTime;

        cloud.oscillation += 0.1f * deltaTime;

        // Мерцание
        cloud.flashTimer += deltaTime;
        if (cloud.flashTimer >= cloud.flashDuration) {
            cloud.isFlashing = true;
            cloud.flashTimer = 0.0f;
            cloud.flashDuration = 0.1f + (rand() % 100) * 0.001f;
        } else if (cloud.isFlashing && cloud.flashTimer > 0.05f) {
            cloud.isFlashing = false;
            cloud.flashDuration = 3.0f + (rand() % 100) * 0.1f;
        }
    }
}

void updateBalloons(float deltaTime) {
    for (auto& balloon : balloons) {
        balloon.oscillation += deltaTime;
    }
}

int main() {
	setlocale(LC_ALL, "ru_RU.UTF-8");
    // Настройки OpenGL
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antiAliasingLevel = 4;
    settings.majorVersion = 3;
    settings.minorVersion = 3;

    // Создание окна
    sf::Window window(sf::VideoMode({static_cast<unsigned int>(width), static_cast<unsigned int>(height)}),
                 "Mail-Airship - Доставка посылок",
                 sf::State::Windowed,  // ← ИЗМЕНИТЕ НА ЭТО
                 settings);

    window.setVerticalSyncEnabled(true);
	
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return -1;
}

    // Инициализация OpenGL
   /*if (!gladLoadGL()) {
        std::cerr << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }*/

    // Настройка OpenGL
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    std::cout << "OpenGL версия: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Управление:" << std::endl;
    std::cout << "  W/A/S/D - движение" << std::endl;
    std::cout << "  SPACE/SHIFT - вверх/вниз" << std::endl;
    std::cout << "  Стрелки влево/вправо - поворот" << std::endl;
    std::cout << "  F - включить/выключить прожектор" << std::endl;
    std::cout << "  ESC - выход" << std::endl;

    // Создание шейдеров
    shaderProgram = createShaderProgram(mainVertexShader, mainFragmentShader);
    cloudShaderProgram = createShaderProgram(cloudVertexShader, cloudFragmentShader);

    // Создание моделей
    groundModel = createGroundModel();
    treeModel = createTreeModel();
    airshipModel = createAirshipModel();
    cloudModel = createCloudModel();
    balloonModel = createBalloonModel();

    // Инициализация объектов
    initClouds();
    initBalloons();

    // Основной цикл
    sf::Clock clock;
    float lastFrame = 0.0f;

    while (window.isOpen()) {
        float currentFrame = clock.getElapsedTime().asSeconds();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        timeElapsed += deltaTime;

        // Обработка ввода
        processInput(window, deltaTime);

        // Обновление
        updateClouds(deltaTime);
        updateBalloons(deltaTime);

        // Очистка экрана
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Настройка проекции
        projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 500.0f);

        // Настройка камеры (сзади сверху дирижабля)
        glm::vec3 cameraPos = airshipPos + glm::vec3(0.0f, 2.0f, 0.0f); // Немного выше центра
glm::vec3 cameraTarget = cameraPos + glm::vec3(
    sin(airshipYaw), 
    0.0f, 
    cos(airshipYaw)
);
        view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

        // Рендеринг поля
        glm::mat4 groundMatrix = glm::mat4(1.0f);
        renderModel(groundModel, groundMatrix, groundModel.baseColor);

        // Рендеринг ёлки
        glm::mat4 treeMatrix = glm::mat4(1.0f);
        treeMatrix = glm::translate(treeMatrix, glm::vec3(0.0f, 0.0f, 0.0f));
        renderModel(treeModel, treeMatrix, treeModel.baseColor);

        // Рендеринг туч
        for (const auto& cloud : clouds) {
            renderCloud(cloud);
        }

        // Рендеринг воздушных шаров
        for (const auto& balloon : balloons) {
            renderBalloon(balloon);
        }

        // Рендеринг дирижабля
        glm::mat4 airshipMatrix = glm::mat4(1.0f);
        airshipMatrix = glm::translate(airshipMatrix, airshipPos);
        airshipMatrix = glm::rotate(airshipMatrix, airshipYaw, glm::vec3(0.0f, 1.0f, 0.0f));
        // Легкое покачивание
        airshipMatrix = glm::rotate(airshipMatrix, float(sin(timeElapsed) * 0.02f), glm::vec3(1.0f, 0.0f, 0.0f));
		airshipMatrix = glm::rotate(airshipMatrix, float(cos(timeElapsed * 1.3f) * 0.02f), glm::vec3(0.0f, 0.0f, 1.0f));
        renderModel(airshipModel, airshipMatrix, airshipModel.baseColor);

        // Отображение
        window.display();
    }

    // Очистка
    glDeleteProgram(shaderProgram);
    glDeleteProgram(cloudShaderProgram);

    return 0;
}
