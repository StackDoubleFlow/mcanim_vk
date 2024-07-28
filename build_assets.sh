#!/usr/bin/env bash

# TODO: move this all to meson build

glslc assets/shader.frag -o assets/shader.frag.spv
glslc assets/shader.vert -o assets/shader.vert.spv