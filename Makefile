
CFLAGS=-O3 -Werror=implicit-function-declaration -Werror=implicit-fallthrough=3 -Werror=maybe-uninitialized -Werror=missing-field-initializers -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=redundant-decls -Werror=parentheses -Wformat-nonliteral -Wformat-security -Wformat -Winit-self -Wmaybe-uninitialized -Wold-style-definition -Wredundant-decls -Wstrict-prototypes

WASM_FLAGS=-D WASM=1

solitaire: solitaire.c solitaire.h
	gcc $(CFLAGS) -std=c11 -I/opt/homebrew/include/ `sdl2-config --cflags` -o solitaire solitaire.c `sdl2-config --libs` -lSDL2_image -lm -lSDL2_ttf

wasm: solitaire.c solitaire.h
	emcc $(CFLAGS) $(WASM_FLAGS) -o index.html solitaire.c -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png"]' -sUSE_SDL_TTF=2 --preload-file ./img -lm -lSDL2_ttf

clean:
	rm solitaire index.*
