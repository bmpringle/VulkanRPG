default:
	mkdir -p obj
	mkdir -p bin
	clang -c -D VK_USE_PLATFORM_MACOS_MVK -I ./SDK/macOS/include/ ./SDK/macOS/include/volk/volk.c -o ./obj/volk.o
	clang++ -std=c++17 -Wall -I ./SDK/macOS/include/ -I ./include/ -L ./SDK/macOS/lib/ -l SDL2-2.0.0 -O3 ./src/main.cpp ./obj/* -o ./bin/game

	glslc shaders/src/shader_2d.vert -o shaders/bin/shader_2d_vert.spv
	glslc shaders/src/shader_2d.frag -o shaders/bin/shader_2d_frag.spv