cmake_minimum_required(VERSION 3.20)
project(MailAirship)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ручное указание путей
set(SFML_DIR "C:/GitHub/SFML-3.0.0")
set(VCPKG_DIR "C:/GitHub/vcpkg/installed/x64-windows")

# Проверка что файлы существуют
if(NOT EXISTS "${SFML_DIR}")
    message(FATAL_ERROR "SFML not found at: ${SFML_DIR}")
endif()

# Исполняемый файл
add_executable(${PROJECT_NAME} main.cpp)

# Включаем пути к заголовочным файлам
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SFML_DIR}/include
    ${VCPKG_DIR}/include
)

# Пути к библиотекам
target_link_directories(${PROJECT_NAME} PRIVATE
    ${SFML_DIR}/lib
    ${VCPKG_DIR}/lib
)

# Линковка библиотек - для динамической линковки используем импортные библиотеки
# Ищем библиотеки в таком порядке:
find_library(SFML_GRAPHICS 
    NAMES sfml-graphics-3 sfml-graphics 
    PATHS ${SFML_DIR}/lib
    REQUIRED
)

find_library(SFML_WINDOW 
    NAMES sfml-window-3 sfml-window 
    PATHS ${SFML_DIR}/lib
    REQUIRED
)

find_library(SFML_SYSTEM 
    NAMES sfml-system-3 sfml-system 
    PATHS ${SFML_DIR}/lib
    REQUIRED
)

find_library(GLEW_LIB 
    NAMES glew32 glew32s
    PATHS ${VCPKG_DIR}/lib
    REQUIRED
)

# Линковка
target_link_libraries(${PROJECT_NAME}
    ${SFML_GRAPHICS}
    ${SFML_WINDOW}
    ${SFML_SYSTEM}
    ${GLEW_LIB}
    opengl32
    gdi32
    winmm
)

# Копирование DLL
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_DIR}/bin/sfml-graphics-3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_DIR}/bin/sfml-window-3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SFML_DIR}/bin/sfml-system-3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_DIR}/bin/glew32.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)