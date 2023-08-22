cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PathTracing")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")

# GLFW
set(GLFW_DIR "${LIB_DIR}/glfw")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLFW_DIR}" binary_dir EXCLUDE_FROM_ALL)
include_directories(${GLFW_DIR}/include)

# ImGUI
set(IMGUI_DIR "${LIB_DIR}/imgui")
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends ..)

# GLM
set(GLM_DIR "${LIB_DIR}/glm")
find_package(GLM REQUIRED)

# OpenGL
set(GLEW_DIR "${LIB_DIR}/glew")
list(APPEND CMAKE_PREFIX_PATH "${GLEW_DIR}")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

set(LIBRARIES "glfw;OpenGL::GL;GLEW::GLEW;glm::glm")

include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${GLFW_DIR}/deps)

# Source Files
set(SOURCES
	"${SRC_DIR}/Main.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
	"${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
	"${IMGUI_DIR}/imgui.cpp"
	"${IMGUI_DIR}/imgui_draw.cpp"
	"${IMGUI_DIR}/imgui_demo.cpp"
	"${IMGUI_DIR}/imgui_tables.cpp"
	"${IMGUI_DIR}/imgui_widgets.cpp"
	"${IMGUI_DIR}/imgui.cpp"
 "src/Application.h" "src/Application.cpp" "src/Image.h" "src/Image.cpp"   "src/stb_image.h" src/Renderer.cpp src/Renderer.h src/Ray.h src/Camera.cpp src/Camera.h src/Input.cpp src/Input.h)

add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
target_compile_definitions(${PROJECT_NAME} PUBLIC -DImTextureID=ImU64)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PathTracing PROPERTY CXX_STANDARD 20)
endif()
