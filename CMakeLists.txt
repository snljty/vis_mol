cmake_minimum_required(VERSION 3.16...3.31)

project(vis_mol LANGUAGES C)

include(GNUInstallDirs)

# CMake 3.11 开始，实际可以找到两种 OpenGL:
# Legacy（老式）：一个统一的 libGL.so（就是 OPENGL_gl_LIBRARY）。
# GLVND（新式）：拆成 libOpenGL.so + libGLX.so（OPENGL_opengl_LIBRARY / OPENGL_glx_LIBRARY）。
# GLVND = GL Vendor-Neutral Dispatch，让 Mesa、NVIDIA 等多个 GL 实现能在运行时共存分发。
# 3.16 是 2019 年，一个比较合适的兼容老项目的版本

# 3.16...3.31 是 3.12 引入的写法，后面是上限式兼容标记，告诉新 CMake "我知道有更新的策略，别用旧策略对我报 deprecation 警告"

# Windows 下启用资源（可执行文件图标）嵌入 (MinGW: windres)
if(WIN32)
    enable_language(RC)
# Linux 桌面图标+启动器
elseif(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    # 将程序图标安装到高彩色图标主题中，从而同名程序会直接使用。
    # 用 .in 拓展名习惯上告诉别人这个是模板，不能直接用
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/${PROJECT_NAME}.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.desktop
        @ONLY)

    # 安装图标
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME ${PROJECT_NAME}.png)

    # 安装桌面入口（登录器），注意是替换路径变量名后的桌面文件
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    # .desktop 里面 Exec 后面的 %f = 单个文件，%F = 多个文件。
endif()


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ========== 查找 OpenGL ==========
find_package(OpenGL REQUIRED)
if(OpenGL_FOUND)
    message(STATUS "OpenGL found: OpenGL::GL")
else()
    message(FATAL_ERROR "OpenGL not found.")
endif()

# set(FreeGLUT_LIBS FreeGLUT::freeglut)
# 默认静态链接，只需发布可执行文件即可。如果用上面的就是动态链接，要带上FreeGLUT的动态链接库才能运行。
set(FreeGLUT_LIBS FreeGLUT::freeglut_static)

# ========== 查找 FreeGLUT ==========
find_package(FreeGLUT CONFIG REQUIRED)
if(FreeGLUT_FOUND)
    message(STATUS "FreeGLUT found: ${FreeGLUT_LIBS}")
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
        ${GLU_LIB}
    )
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

# ========== Windows 可执行文件图标 ==========
if(WIN32)
    set(APP_ICON_RC ${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.rc)
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RC})
    # 确保 windres 可以找到 logo.rc 同目录下的 logo.ico
    set_source_files_properties(${APP_ICON_RC} PROPERTIES
        COMPILE_FLAGS "-I\"${CMAKE_CURRENT_SOURCE_DIR}/resources\"")
endif()

# ========== 高分辨率选项（可选） ==========
option(USE_HIGH_RESOLUTION "Use higher resolution sphere" OFF)
if(USE_HIGH_RESOLUTION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_HIGH_RESOLUTION=1)
    message(STATUS "USE_HIGH_RESOLUTION: ON")
else()
    message(STATUS "USE_HIGH_RESOLUTION: OFF")
endif()

# 关键：让包含着色器头文件的源文件依赖这些生成的头文件
set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/shader.c APPEND PROPERTY OBJECT_DEPENDS ${SHADER_HEADERS})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${PLATFORM_LIBS}
    ${FreeGLUT_LIBS}
    GLAD
    ${CMAKE_DL_LIBS} # 为 Linux 下静态库。 Windows 下此变量为空
)

# libdl 是 Linux/Unix 上提供动态加载（dynamic loading）接口的库。核心就四个函数：

# dlopen(path, flags) —— 在运行时打开一个 .so（共享库），拿到句柄。
# dlsym(handle, "name") —— 按名字在该库里查找函数/变量地址，返回函数指针。
# dlclose(handle) —— 关闭句柄。
# dlerror() —— 取错误信息。

# Linux 下 GLAD 的工作方式：它替在运行时把 OpenGL 的所有函数入口查出来，赋给一组全局函数指针（glad_glXxx）
# 所以需要上面的 ${CMAKE_FL_LIBS} 提供 -ldl，而且静态链接时候必须在 GLAD 之后，提供接口。

# 安装规则：只安装 lollipop，不安装 embed_shaders
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
