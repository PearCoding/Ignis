#include "loader/Transpiler.h"
#include "shader/ScriptCompiler.h"

using namespace IG;
int main(int, const char**)
{
    const std::string shader = Transpiler::generateTestShader();

    ScriptCompiler compiler;
    compiler.setVerbose(true);
    compiler.setOptimizationLevel(0);

    const std::string prepared_shader = compiler.prepare(shader);
    void* func                        = compiler.compile(prepared_shader, "_var_test_");

    return func != nullptr ? EXIT_SUCCESS : EXIT_FAILURE;
}