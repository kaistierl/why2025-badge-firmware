#!/bin/bash

SDL3_FLAGS=$(pkg-config --cflags --libs sdl3)

gcc -o sshterm -std=c11 -Wall -Wextra $SDL3_FLAGS \
  -I../../badgevms/include/ \
  -Icomponents \
  -Ithirdparty/libvterm-0.3.3/include \
  -DHAVE_CONFIG_H \
  main.c \
  components/renderer/renderer.c \
  components/term/term.c \
  components/keyboard/keyboard.c \
  thirdparty/libvterm-0.3.3/src/vterm.c \
  thirdparty/libvterm-0.3.3/src/screen.c \
  thirdparty/libvterm-0.3.3/src/state.c \
  thirdparty/libvterm-0.3.3/src/pen.c \
  thirdparty/libvterm-0.3.3/src/encoding.c \
  thirdparty/libvterm-0.3.3/src/parser.c \
  thirdparty/libvterm-0.3.3/src/keyboard.c \
  thirdparty/libvterm-0.3.3/src/unicode.c