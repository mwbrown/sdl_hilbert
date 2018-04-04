cmake_minimum_required(VERSION 3.6)
project(sdl_hilbert)

set(PROJ_NAME_SRC
	./src/main.c
)

add_definitions("-Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function")

add_executable(sdl_hilbert ${PROJ_NAME_SRC})
target_link_libraries(sdl_hilbert SDL2)