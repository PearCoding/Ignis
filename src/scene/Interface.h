#pragma once

#include "IG_Config.h"

namespace IG {
void setup_interface(size_t width, size_t height);
void get_interface(size_t& width, size_t& height);
void cleanup_interface();
float* get_pixels();
void clear_pixels();
void add_resource_path(const char* path);
}