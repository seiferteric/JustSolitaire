
CFLAGS=-O3 -Wall -Werror

solitaire: solitaire.c solitaire.h
	gcc $(CFLAGS) -std=c11 -I/opt/homebrew/include/ `sdl2-config --cflags` -o solitaire solitaire.c `sdl2-config --libs` -lSDL2_image -lm -lGL

wasm: solitaire.c solitaire.h
	emcc $(CFLAGS) $(WASM_FLAGS) -o index.html solitaire.c -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png"]' -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=1 -sUSE_WEBGL2=1 -sALLOW_MEMORY_GROWTH --preload-file ./img -lm -lGL --shell-file shell_file_tamplate.html

clean:
	rm solitaire index.*
