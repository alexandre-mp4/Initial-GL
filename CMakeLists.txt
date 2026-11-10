cmake_minimum_required(VERSION 3.16)
project(DriftGame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release par défaut : avec 48 000 arbres, un build Debug serait très lent
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(DriftGame
	src/DriftGame.cpp
	src/core/Game.cpp src/core/GameUi.cpp src/core/Session.cpp src/core/AssetManager.cpp
	src/render/ShaderProgram.cpp src/render/Texture2D.cpp src/render/Mesh.cpp src/render/Camera.cpp
	src/render/Renderer.cpp src/render/Lights.cpp src/render/Model.cpp src/render/RenderTarget.cpp
	src/render/ShadowMap.cpp src/render/Environment.cpp src/render/PostProcess.cpp src/render/DebugDraw.cpp
	src/world/Scene.cpp src/world/Road.cpp src/world/RoadFurniture.cpp src/world/Terrain.cpp src/world/Forest.cpp
	src/physics/Car.cpp
	src/ui/Ui.cpp src/ui/Hud.cpp
	src/audio/Audio.cpp)

# les en-têtes sont rangés par dossier mais inclus sans chemin
target_include_directories(DriftGame PRIVATE ${CMAKE_SOURCE_DIR})
foreach(dir core render world physics ui audio)
	target_include_directories(DriftGame PRIVATE ${CMAKE_SOURCE_DIR}/src/${dir})
endforeach()

target_compile_definitions(DriftGame PRIVATE GL_SILENCE_DEPRECATION)
target_link_libraries(DriftGame PRIVATE OpenGL::GL glfw GLEW::GLEW glm::glm Threads::Threads ${CMAKE_DL_LIBS})

if(APPLE)
	target_link_libraries(DriftGame PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
endif()

# le jeu charge ses fichiers en chemin relatif : on les place à côté de l'exécutable (assets/ en lien symbolique)
add_custom_command(TARGET DriftGame POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders  $<TARGET_FILE_DIR:DriftGame>/shaders
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/models   $<TARGET_FILE_DIR:DriftGame>/models
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:DriftGame>/textures
	COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets   $<TARGET_FILE_DIR:DriftGame>/assets
)
