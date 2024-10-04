cmake_minimum_required(VERSION 3.30.2)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
project(art)

include_directories(C:/dev/raylib/include C:/dev/openssl/include C:/dev/libwebsockets/build/include)

add_executable(art art.c arr.c arr.h hash.h map.c map.h)

target_link_libraries(art C:/dev/raylib/lib/libraylib.a C:/dev/libwebsockets/build/bin/libwebsockets.dll user32.lib gdi32.lib opengl32.lib winmm.lib)
