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


set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RES_DIR})

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_compile_definitions(GLFW_INCLUDE_NONE)

# Header visibili a tutti: glm, la radice di src (gli include si scrivono "Modulo/File.h") e i
# vendor header-only. ImGui e ImGuizmo NO: arrivano solo a chi linka il loro target.
include_directories("${LIB_DIR}/glm" "${SRC_DIR}" "${SRC_DIR}/vendor")

# ---- vendor ----------------------------------------------------------------
set(VENDOR "${SRC_DIR}/vendor")
add_library(imgui STATIC
  ${VENDOR}/imgui/imgui.cpp ${VENDOR}/imgui/imgui_demo.cpp ${VENDOR}/imgui/imgui_draw.cpp
  ${VENDOR}/imgui/imgui_tables.cpp ${VENDOR}/imgui/imgui_widgets.cpp
  ${VENDOR}/imgui/imgui_impl_glfw.cpp ${VENDOR}/imgui/imgui_impl_vulkan.cpp)
target_include_directories(imgui PUBLIC ${VENDOR}/imgui)
target_compile_definitions(imgui PUBLIC ImTextureID=ImU64)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

add_library(imguizmo STATIC ${VENDOR}/ImGuizmo/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${VENDOR}/ImGuizmo)
target_link_libraries(imguizmo PUBLIC imgui)

add_library(stb STATIC ${VENDOR}/stb/stb_image_impl.cpp ${VENDOR}/stb/stb_image_write.cpp)
add_library(cgltf STATIC ${VENDOR}/cgltf_impl.cpp)

# ---- moduli ----------------------------------------------------------------
# Un modulo = una cartella di src = una libreria. DEPS sono i moduli che puo' includere:
# tools/check_modules.py fa fallire il build se un file ne include un altro (o ImGui fuori
# dall'Editor). La tabella che legge e' questa, scritta in modules.txt piu' sotto.
function(lucerna_module name)
  cmake_parse_arguments(M "" "" "DEPS;LIBS" ${ARGN})
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${SRC_DIR}/${name}/*.cpp")
  file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${SRC_DIR}/${name}/*.h")
  set(target Lucerna${name})
  if(sources)
    add_library(${target} STATIC ${sources} ${headers})
    target_precompile_headers(${target} PRIVATE "${SRC_DIR}/ptpch.h")
    set(scope PUBLIC)
  else()
    add_library(${target} INTERFACE ${headers}) # solo header
    set(scope INTERFACE)
  endif()
  foreach(dep ${M_DEPS})
    target_link_libraries(${target} ${scope} Lucerna${dep})
  endforeach()
  target_link_libraries(${target} ${scope} ${M_LIBS})
  add_dependencies(${target} ModuleCheck)
  set_property(GLOBAL APPEND_STRING PROPERTY LUCERNA_MODULES "${name}: ${M_DEPS}\n")
endfunction()

add_custom_target(ModuleCheck
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_modules.py ${SRC_DIR} ${CMAKE_BINARY_DIR}/modules.txt
  COMMENT "Controllo delle dipendenze fra moduli")

lucerna_module(Core)
lucerna_module(Assets        DEPS Core)
lucerna_module(Scene         DEPS Core Assets)
lucerna_module(Import        DEPS Core Assets Scene LIBS cgltf)
lucerna_module(Vulkan        DEPS Core LIBS Vulkan::Vulkan glfw stb)
# Transitorio (milestone Scene architecture): World e il codice ancora intrecciato con lui.
# Ogni issue ne porta fuori un pezzo, la #29 lo cancella.
lucerna_module(Legacy        DEPS Core Assets Scene Import)
lucerna_module(Render        DEPS Core Assets Scene Vulkan Legacy LIBS stb)
lucerna_module(Serialization DEPS Core Assets Scene Legacy Render)
lucerna_module(Editor        DEPS Core Assets Scene Import Vulkan Legacy Render Serialization LIBS imgui imguizmo glfw)

get_property(LUCERNA_MODULES GLOBAL PROPERTY LUCERNA_MODULES)
file(WRITE ${CMAKE_BINARY_DIR}/modules.txt "${LUCERNA_MODULES}")

add_executable(${PROJECT_NAME} "${SRC_DIR}/Main.cpp")

# Controlli della logica senza GPU (tests/Checks.cpp): ./build/LucernaChecks
add_executable(LucernaChecks "${CMAKE_CURRENT_SOURCE_DIR}/tests/Checks.cpp")
target_link_libraries(LucernaChecks PRIVATE LucernaScene LucernaLegacy)
set_target_properties(LucernaChecks PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE LucernaEditor)

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


