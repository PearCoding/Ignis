#pragma once

#include <string>

namespace IG {
/// Init jit compiling with a specific path to a Ignis driver
void ig_init_jit(const std::string& driver_path);

/// Compile given source together with the ignis standard library and return pointer to the given function
void* ig_compile_source(const std::string& str, const std::string& function);
}