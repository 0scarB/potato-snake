#!/bin/sh

set -eu

clang \
    -ggdb \
    -std=c99 -pedantic \
    -Wall -Wextra \
    -O3 \
    snake.c \
    -o snake

clang \
    -DWASM \
    --target=wasm32 -nostartfiles -nostdlib -Wl,--no-entry \
    -ggdb \
    -std=c99 -pedantic \
    -Wall -Wextra \
    -O3 \
    snake.c \
    -o snake.wasm
wasm-opt -O4 snake.wasm -o snake.wasm

clang \
    -DTEST \
    -ggdb \
    -std=c99 -pedantic \
    -Wall -Wextra \
    -O3 \
    snake.c \
    test_framework.c \
    -o snake-test

