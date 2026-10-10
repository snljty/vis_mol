cmake_minimum_required(VERSION 3.10)
project(vis_mol LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ========== 查找 OpenGL ==========
find_package(OpenGL REQUIRED)
if(OpenGL_FOUND)
    message(STATUS "OpenGL found: OpenGL::GL")
else()
    message(FATAL_ERROR "OpenGL not found.")
endif()

# ========== 查找 FreeGLUT ==========
find_package(FreeGLUT CONFIG REQUIRED)
if(FreeGLUT_FOUND)
    message(STATUS "FreeGLUT found: FreeGLUT::freeglut")
else()
    message(FATAL_ERROR
        "FreeGLUT not found. Set CMAKE_PREFIX_PATH to the FreeGLUT installation directory.\n"
)
endif()

# ========== 查找 GLAD ==========
find_package(GLAD CONFIG REQUIRED)
if(GLAD_FOUND)
    message(STATUS "GLAD found: GLAD")
else()
    message(FATAL_ERROR "GLAD not found. Set CMAKE_PREFIX_PATH to GLAD install dir.")
endif()

# ========== 平台相关链接库 ==========
if(WIN32)
    set(PLATFORM_LIBS
        opengl32
        glu32
        winmm)
elseif(UNIX)
    find_library(GL_LIB GL)
    find_library(GLU_LIB GLU)
    set(PLATFORM_LIBS
        ${GL_LIB}
        ${GLU_LIB})
endif()

# ========== 着色器头文件生成 ==========
file(GLOB SHADER_SOURCES ${CMAKE_SOURCE_DIR}/shaders/*)
# 生成对应的输出头文件列表（使用绝对路径，避免 CMake 路径混淆）
foreach(shader ${SHADER_SOURCES})
    get_filename_component(name ${shader} NAME)
    list(APPEND SHADER_HEADERS ${CMAKE_SOURCE_DIR}/include/${name}.h)
endforeach()

# 构建 embed_shaders 工具
file(GLOB TOOLS_SOURCES ${CMAKE_SOURCE_DIR}/tools/src/*.c)
add_executable(embed_shaders ${TOOLS_SOURCES})
target_include_directories(embed_shaders PRIVATE ${CMAKE_SOURCE_DIR}/tools/include)

# 自定义命令：运行 embed_shaders 生成头文件
add_custom_command(
    OUTPUT ${SHADER_HEADERS}
    COMMAND embed_shaders ${CMAKE_SOURCE_DIR}
    DEPENDS embed_shaders ${SHADER_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating shader headers from ${SHADER_SOURCES}"
)

# ========== 主程序 ==========
file(GLOB MAIN_SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
add_executable(${PROJECT_NAME} WIN32 ${MAIN_SOURCES})

# ========== 高分辨率选项（可选） ==========
option(USE_HIGH_RESOLUTION "Use higher resolution sphere" OFF)
if(USE_HIGH_RESOLUTION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_HIGH_RESOLUTION=1)
    message(STATUS "USE_HIGH_RESOLUTION: ON")
else()
    message(STATUS "USE_HIGH_RESOLUTION: OFF")
endif()

# ========== 查找 uthash（可选依赖） ==========
find_path(UTHASH_INCLUDE_DIR uthash.h PATH_SUFFIXES src)
if(UTHASH_INCLUDE_DIR)
    message(STATUS "uthash found: ${UTHASH_INCLUDE_DIR}")
    # 告诉源代码：有 uthash，编译基于hash的细分球
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_UTHASH=1)
    # 添加 include 路径
    target_include_directories(${PROJECT_NAME} PRIVATE ${UTHASH_INCLUDE_DIR})
else()
    message(STATUS "uthash NOT found - icosphere subdivision and element index search will use slower version.")
endif()

# 关键：让包含着色器头文件的源文件依赖这些生成的头文件
set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/shader.c APPEND PROPERTY OBJECT_DEPENDS ${SHADER_HEADERS})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_LIBS} FreeGLUT::freeglut GLAD)

# 安装规则：只安装 lollipop，不安装 embed_shaders
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
