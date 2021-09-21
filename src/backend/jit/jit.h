#pragma once

#include <string>

namespace IG {
/// Compile given source together with the ignis standard library and return pointer to the main function 'ig_main'
void* ig_compile_source(const std::string& str);
}