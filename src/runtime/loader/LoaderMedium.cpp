#include "LoaderMedium.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"

namespace IG {

static void medium_homogeneous(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& medium, ShadingTree& tree)
{
    tree.beginClosure(name);

    tree.addColor("sigma_a", *medium, Vector3f::Zero(), true);
    tree.addColor("sigma_s", *medium, Vector3f::Zero(), true);
    tree.addNumber("g", *medium, 0, true);

    const std::string media_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let medium_" << media_id << " : MediumGenerator = @|ctx| { maybe_unused(ctx); make_homogeneous_medium(" << tree.getInline("sigma_a")
           << ", " << tree.getInline("sigma_s")
           << ", make_henyeygreenstein_phase(" << tree.getInline("g") << ")) };" << std::endl;

    tree.endClosure();
}

// It is recommended to not define the medium, instead of using vacuum
static void medium_vacuum(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>&, ShadingTree& tree)
{
    tree.beginClosure(name);

    const std::string media_id = tree.currentClosureID();
    stream << tree.pullHeader()
           << "  let medium_" << media_id << " : MediumGenerator = @|_ctx| make_vacuum_medium();" << std::endl;

    tree.endClosure();
}

using MediumLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, ShadingTree&);
static const struct {
    const char* Name;
    MediumLoader Loader;
} _generators[] = {
    { "homogeneous", medium_homogeneous },
    { "constant", medium_homogeneous },
    { "vacuum", medium_vacuum },
    { "", nullptr }
};

std::string LoaderMedium::generate(ShadingTree& tree)
{
    std::stringstream stream;

    size_t counter = 0;
    for (const auto& pair : tree.context().Options.Scene.media()) {
        const auto medium = pair.second;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == medium->pluginType()) {
                _generators[i].Loader(stream, pair.first, medium, tree);
                ++counter;
                found = true;
                break;
            }
        }
        if (!found)
            IG_LOG(L_ERROR) << "No medium type '" << medium->pluginType() << "' available" << std::endl;
    }

    if (counter != 0)
        stream << std::endl;

    stream << "  let media = @|id:i32| {" << std::endl
           << "    match(id) {" << std::endl;

    size_t counter2 = 0;
    for (const auto& pair : tree.context().Options.Scene.media()) {
        const auto medium          = pair.second;
        const std::string media_id = tree.getClosureID(pair.first);
        stream << "      " << counter2 << " => medium_" << media_id
               << "," << std::endl;
        ++counter2;
    }

    stream << "    _ => @|_ctx : ShadingContext| make_vacuum_medium()" << std::endl;

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}

} // namespace IG