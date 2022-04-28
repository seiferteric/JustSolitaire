# JustSolitaire

This was a *weekendish* (plus some days) project while flying home from Hawaii and playing a really bad and buggy solitaire on the in-flight entertainment system. I wanted to see if I could build one better, and I did!

I have not done any game stuff in a long time so I wanted to play with SDL2 and stuff. Also I used this as an opportunity to try out that WASM/emscripten stuff. You can run it in your browser! [HERE!](https://eric.seifert.casa/solitaire/)

![Screenshot][logo]

[logo]: https://eric.seifert.casa/solitaire/screenshot.jpg "Screenshot"


## BUILD:

The makefile is very simple and assumes you have SDL2 dev libs installed, just run "make".

To build WASM run "make wasm", but you will need emscripten installed and in your path.

The playing card images are from: https://code.google.com/archive/p/vector-playing-cards/
