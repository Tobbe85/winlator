cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

Project(Winlator)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wno-unused-function -Wimplicit-function-declaration")

#add_subdirectory(proot)
add_subdirectory(virglrenderer)
add_subdirectory(OpenXR-SDK)
add_subdirectory(patchelf)

add_library(winlator SHARED
            xr/engine.c
            xr/framebuffer.c
            xr/input.c
            xr/main.c
            xr/math.c
            xr/renderer.c
            winlator/drawable.c
            winlator/gpu_image.c
            winlator/sysvshared_memory.c
            winlator/xconnector_epoll.c
            winlator/alsa_client.c
            winlator/patchelf_wrapper.cpp)

target_link_libraries(winlator
                      log
                      android
                      jnigraphics
                      openxr_loader
                      aaudio
                      EGL
                      GLESv2
                      GLESv3)