#!/bin/bash

SDL3_FLAGS=$(pkg-config --cflags --libs sdl3)
gcc main.c -o cyb3rk4t_term -std=c11 -Wall -Wextra $SDL3_FLAGS -I../../badgevms/include/