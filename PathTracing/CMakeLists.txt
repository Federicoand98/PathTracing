cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


project ("Lucerna")

# Senza un build type esplicito CMake non passa NESSUN flag di ottimizzazione (-O0): tutto
# il lato CPU (parsing OBJ, costruzione BVH/SBVH) gira non ottimizzato e molto lento. Default
# a Release quando non specificato; per debuggare basta -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")

# Source Files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RES_DIR})

execute_process(COMMAND ${CMAKE_COMMAND} -E echo "Contents of ${RES_DIR}:")
# List contents of RES_DIR
file(GLOB_RECURSE RES_FILES "${RES_DIR}/**")
message("Contents of ${RES_DIR}:")
foreach(file ${RES_FILES})
  message("${file}")
endforeach()


add_executable (${PROJECT_NAME} ${SOURCES} "src/stb_image_write.cpp" "src/stb_image_impl.cpp")
target_precompile_headers(${PROJECT_NAME} PRIVATE "src/ptpch.h")
# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw")

# GLFW
# set(GLFW_DIR "${LIB_DIR}/glfw")
# set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
include_directories(
		"${LIB_DIR}/glm"
		"${SRC_DIR}"
		"${SRC_DIR}/ImGui"
		"${SRC_DIR}/ImGuizmo"
		"${SRC_DIR}/vendor"
    ${GLFW_INCLUDE_DIRS}
)


target_compile_definitions(${PROJECT_NAME} PUBLIC -DImTextureID=ImU64 -DGLFW_INCLUDE_NONE)

# Shader: SPIR-V generato a build time. glslc risolve da se' gli #include, quindi un compute
# dipende da tutti i .glsl: modificarne uno ricompila solo gli shader.
find_program(GLSLC glslc REQUIRED)
set(SHADER_DIR "${RES_DIR}/shaders")
set(SPV_DIR "${SHADER_DIR}/spv")
file(MAKE_DIRECTORY ${SPV_DIR})
file(GLOB_RECURSE SHADER_INCLUDES CONFIGURE_DEPENDS "${SHADER_DIR}/*.glsl")
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${SHADER_DIR}/*.comp")

set(SPV_FILES "")
foreach(source ${SHADER_SOURCES})
  get_filename_component(name ${source} NAME_WE)
  set(output "${SPV_DIR}/${name}.spv")
  add_custom_command(OUTPUT ${output}
    COMMAND ${GLSLC} --target-env=vulkan1.3 -O -I ${SHADER_DIR} -o ${output} ${source}
    DEPENDS ${source} ${SHADER_INCLUDES}
    COMMENT "glslc ${name}.comp")
  list(APPEND SPV_FILES ${output})
endforeach()
add_custom_target(Shaders ALL DEPENDS ${SPV_FILES})
add_dependencies(${PROJECT_NAME} Shaders)

# set(CMAKE_EXE_LINKER_FLAGS "-static")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()
