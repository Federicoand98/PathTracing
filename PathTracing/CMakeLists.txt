cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PathTracing")

set(CMAKE_CXX_STANDARD 17)

# Source Files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_executable (${PROJECT_NAME} ${SOURCES})
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RES_DIR})

# GLFW
set(GLFW_DIR "${LIB_DIR}/glfw")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
include_directories(
		"${PROJECT_SOURCE_DIR}/libs/glm"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/ImGui"
)

include_directories("${PROJECT_SOURCE_DIR}/libs/glew/include")

target_link_libraries(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/libs/glew/lib/Release/x64/glew32.lib")

file(GLOB DLLS "libs/glew/glew32.dll")
file(COPY ${DLLS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(${PROJECT_NAME} PUBLIC -DImTextureID=ImU64)

set(CMAKE_EXE_LINKER_FLAGS "-static")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PathTracing PROPERTY CXX_STANDARD 20)
endif()
