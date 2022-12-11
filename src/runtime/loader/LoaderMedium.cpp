#include "LoaderMedium.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "medium/HomogeneousMedium.h"
#include "medium/VacuumMedium.h"

namespace IG {

static std::shared_ptr<Medium> medium_homogeneous(const std::string& name, const std::shared_ptr<SceneObject>& medium, LoaderContext&)
{
    return std::make_shared<HomogeneousMedium>(name, medium);
}

// It is recommended to not define the medium, instead of using vacuum
static std::shared_ptr<Medium> medium_vacuum(const std::string& name, const std::shared_ptr<SceneObject>&, LoaderContext&)
{
    return std::make_shared<VacuumMedium>(name);
}

using MediumLoader = std::shared_ptr<Medium> (*)(const std::string&, const std::shared_ptr<SceneObject>&, LoaderContext&);
static const struct {
    const char* Name;
    MediumLoader Loader;
} _generators[] = {
    { "homogeneous", medium_homogeneous },
    { "constant", medium_homogeneous },
    { "vacuum", medium_vacuum },
    { "", nullptr }
};

void LoaderMedium::loadMedia(LoaderContext& ctx)
{
    size_t id = 0;
    for (const auto& pair : ctx.Options.Scene.media()) {
        const auto medium = pair.second;

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == medium->pluginType()) {
                auto ptr = _generators[i].Loader(pair.first, medium, ctx);
                ptr->setID(id++);
                mMedia[pair.first] = ptr;
                found              = true;
                break;
            }
        }
        if (!found)
            IG_LOG(L_ERROR) << "No medium type '" << medium->pluginType() << "' available" << std::endl;
    }
}

void LoaderMedium::handleReferenceEntity(const std::string& mediumInnerName, const std::string& entity_name, size_t id)
{
    // TODO: This can be a bottle neck if many different media is used (which is unlikely the case though)
    for (auto pair : mMedia) {
        auto medium = pair.second;
        if (medium->hasReferenceEntity() && medium->referenceEntity() == entity_name) {
            medium->setReferenceEntityID(id);
        } else if (pair.first == mediumInnerName && !medium->hasReferenceEntity() && !medium->isReferenceEntityIDSet()) {
            medium->setReferenceEntityID(id);
            medium->setReferenceEntity(entity_name);
        }
    }
}

void LoaderMedium::prepare(LoaderContext& ctx)
{
    if (ctx.Options.Scene.media().empty())
        return;

    IG_LOG(L_DEBUG) << "Prepare participating media" << std::endl;
    loadMedia(ctx);
}

std::string LoaderMedium::generate(ShadingTree& tree)
{
    std::stringstream stream;

    size_t counter = 0;
    for (auto pair : mMedia)
        pair.second->serialize(Medium::SerializationInput{ counter++, stream, tree });

    if (counter != 0)
        stream << std::endl;

    stream << "  let media = @|id:i32| {" << std::endl
           << "    match(id) {" << std::endl;

    counter = 0;
    for (const auto& pair : mMedia) {
        const auto medium          = pair.second;
        const std::string media_id = tree.getClosureID(medium->name());
        stream << "      " << counter << " => medium_" << media_id
               << "," << std::endl;
        ++counter;
    }

    stream << "    _ => @|_ctx : ShadingContext| make_vacuum_medium()" << std::endl;

    stream << "    }" << std::endl
           << "  };" << std::endl;

    return stream.str();
}
} // namespace IG