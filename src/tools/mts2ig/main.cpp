#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "SpectralMapper.h"
#include "tinyparser-mitsuba.h"

using namespace TPM_NAMESPACE;

static bool sQuiet   = false;
static bool sVerbose = false;

static inline void check_arg(int argc, char** argv, int arg, int n)
{
    if (arg + n >= argc)
        std::cerr << "Option '" << argv[arg] << "' expects " << n << " arguments, got " << (argc - arg) << std::endl;
}

static inline void version()
{
    std::cout << "mts2ig 0.1" << std::endl;
}

static inline void usage()
{
    std::cout
        << "mts2ig - Mitsuba to Ignis converter" << std::endl
        << "Usage: mts2ig [options] file" << std::endl
        << "Available options:" << std::endl
        << "   -h      --help                   Shows this message" << std::endl
        << "           --version                Show version and exit" << std::endl
        << "   -q      --quiet                  Do not print messages into console" << std::endl
        << "   -v      --verbose                Print detailed information" << std::endl
        << "           --no-ws                  Do not use any unnecessary whitespace" << std::endl
        << "   -l      --lookup    directory    Add directory as lookup path for external files" << std::endl
        << "   -D      --define    Key Value    Define key with value as an argument for mitsuba files" << std::endl
        << "   -o      --output    scene.json   Writes the output to a given file and not the default input file with .json" << std::endl;
}

class JsonWriter {
public:
    inline JsonWriter(const std::filesystem::path& path, bool whitespace)
        : mPath(path)
        , mStream(path.generic_u8string())
        , mUseWhitespace(whitespace)
        , mCurrentDepth(0)
    {
    }

    inline ~JsonWriter()
    {
        if (mUseWhitespace)
            mStream << std::endl;
    }

    inline std::ostream& w() { return mStream; }
    inline void s()
    {
        if (mUseWhitespace)
            mStream << " ";
    }

    inline void endl()
    {
        if (mUseWhitespace) {
            mStream << std::endl;

            const int whitespaces = mCurrentDepth * 2;
            for (int i = 0; i < whitespaces; ++i)
                mStream << " ";
        }
    }

    inline void goIn()
    {
        ++mCurrentDepth;
        endl();
    }

    inline void goOut()
    {
        if (mCurrentDepth > 0) {
            --mCurrentDepth;
            endl();
        } else {
            std::cerr << "INVALID OUTPUT STREAM HANDLING!" << std::endl;
        }
    }

    inline void objBegin()
    {
        mStream << "{";
        goIn();
    }

    inline void objEnd()
    {
        goOut();
        mStream << "}";
    }

    inline void arrBegin()
    {
        mStream << "[";
        // goIn();
    }

    inline void arrEnd()
    {
        // goOut();
        mStream << "]";
    }

    inline void key(const std::string& name)
    {
        mStream << "\"" << name << "\":";
        if (mUseWhitespace)
            mStream << " ";
    }

private:
    const std::filesystem::path mPath;
    std::ofstream mStream;
    const bool mUseWhitespace;
    int mCurrentDepth;
};

static std::string make_id(const char* prefix, int id)
{
    std::stringstream sstream;
    sstream << "__" << prefix << "_" << id;
    return sstream.str();
}

static inline std::string translate(const std::string& name)
{
    if (name == "to_world")
        return "transform";
    return name;
}

struct LookupEntry {
    const char* Name;
    float Value;
};
static LookupEntry sLookupsIOR[] = {
    { "vacuum", 1.0 },
    { "air", 1.00028 },
    { "glass", 1.55 },
    { "diamond", 2.419 },
    { "bromine", 1.661 },
    { "helium", 1.00004 },
    { "water ice", 1.31 },
    { "hydrogen", 1.00013 },
    { "fused quartz", 1.458 },
    { "pyrex", 1.470 },
    { "carbon dioxide", 1.00045 },
    { "acrylic glass", 1.49 },
    { "water", 1.3330 },
    { "polypropylene", 1.49 },
    { "acetone", 1.36 },
    { "bk7", 1.5046 },
    { "ethanol", 1.361 },
    { "sodium chloride", 1.544 },
    { "carbon tetrachloride", 1.461 },
    { "amber", 1.55 },
    { "glycerol", 1.4729 },
    { "pet", 1.575 },
    { "benzene", 1.501 },
    { "silicone oil", 1.52045 },
    { "none", 0.000 },
    { nullptr, 0 }
};

static LookupEntry sLookupsEta[] = {
    { "ag", 0.129 },
    { "au", 0.402 },
    { "cu", 1.040 },
    { "none", 0.000 },
    { nullptr, 0 }
};

static LookupEntry sLookupsKappa[] = {
    { "ag", 3.250 },
    { "au", 2.540 },
    { "cu", 2.583 },
    { "none", 1.000 },
    { nullptr, 0 }
};

static float lookupIOR(const std::string& name)
{
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    for (size_t i = 0; sLookupsIOR[i].Name; ++i) {
        if (sLookupsIOR[i].Name == s)
            return sLookupsIOR[i].Value;
    }

    if (!sQuiet)
        std::cerr << "Unknown IOR material '" << s << "'" << std::endl;

    return 0;
}

static float lookupEta(const std::string& name)
{
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    for (size_t i = 0; sLookupsEta[i].Name; ++i) {
        if (sLookupsEta[i].Name == s)
            return sLookupsEta[i].Value;
    }

    if (!sQuiet)
        std::cerr << "Unknown Conductor material '" << s << "'" << std::endl;

    return 0;
}

static float lookupKappa(const std::string& name)
{
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    for (size_t i = 0; sLookupsKappa[i].Name; ++i) {
        if (sLookupsKappa[i].Name == s)
            return sLookupsKappa[i].Value;
    }

    if (!sQuiet)
        std::cerr << "Unknown Conductor material '" << s << "'" << std::endl;

    return 1;
}

static void export_property(const Property& prop, JsonWriter& writer)
{
    switch (prop.type()) {
    case PT_ANIMATION:
        if (!sQuiet)
            std::cerr << "No support for Animation" << std::endl;
        writer.w() << "0";
        break;
    case PT_BLACKBODY: // TODO
        if (!sQuiet)
            std::cerr << "No support for Blackbody curve" << std::endl;
        writer.w() << "0";
        break;
    case PT_SPECTRUM: {
        const auto spec = prop.getSpectrum();
        if (spec.isUniform()) { // Scaled white
            writer.arrBegin();
            writer.w() << spec.uniformValue() << ",";
            writer.s();
            writer.w() << spec.uniformValue() << ",";
            writer.s();
            writer.w() << spec.uniformValue();
            writer.arrEnd();
        } else {
            const auto cie = SpectralMapper::eval(spec.wavelengths().data(), spec.weights().data(), false /*todo*/, spec.wavelengths().size());
            const auto rgb = SpectralMapper::mapRGB(std::get<0>(cie), std::get<1>(cie), std::get<2>(cie));
            writer.arrBegin();
            writer.w() << std::get<0>(rgb) << ",";
            writer.s();
            writer.w() << std::get<1>(rgb) << ",";
            writer.s();
            writer.w() << std::get<2>(rgb);
            writer.arrEnd();
        }
    } break;
    case PT_BOOL:
        if (prop.getBool())
            writer.w() << "true";
        else
            writer.w() << "false";
        break;
    case PT_INTEGER:
        writer.w() << prop.getInteger();
        break;
    case PT_NUMBER:
        writer.w() << prop.getNumber();
        break;
    case PT_STRING:
        writer.w() << "\"" << prop.getString() << "\"";
        break;
    case PT_COLOR: {
        const auto rgb = prop.getColor();
        writer.arrBegin();
        writer.w() << rgb.r << ",";
        writer.s();
        writer.w() << rgb.g << ",";
        writer.s();
        writer.w() << rgb.b;
        writer.arrEnd();
    } break;
    case PT_VECTOR: {
        const auto vec = prop.getVector();
        writer.arrBegin();
        writer.w() << vec.x << ",";
        writer.s();
        writer.w() << vec.y << ",";
        writer.s();
        writer.w() << vec.z;
        writer.arrEnd();
    } break;
    case PT_TRANSFORM: {
        const auto trans = prop.getTransform();
        writer.arrBegin();
        for (size_t i = 0; i < trans.matrix.size(); ++i) {
            writer.w() << trans.matrix[i];
            if (i < trans.matrix.size() - 1) {
                writer.w() << ",";
                writer.s();
            }
        }

        writer.arrEnd();
    } break;
    default:
        writer.w() << "0";
        break;
    }
}

static void export_generic(const char* name, const Object& obj, JsonWriter& writer)
{
    writer.key(name);
    writer.objBegin();
    writer.key("type");
    writer.w() << "\"" << obj.pluginType() << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }

    writer.objEnd();
}

static void export_film(const Object& obj, JsonWriter& writer)
{
    writer.key("film");
    writer.objBegin();

    const auto widthP  = obj.property("width");
    const auto heightP = obj.property("height");

    int width  = widthP.getInteger(0);
    int height = heightP.getInteger(0);

    writer.key("size");
    writer.arrBegin();
    writer.w() << width << ",";
    writer.s();
    writer.w() << height;
    writer.arrEnd();
    writer.objEnd();
}

static void export_integrator(const Object& obj, JsonWriter& writer)
{
    export_generic("technique", obj, writer);
}

static void export_sensor(const Object& obj, JsonWriter& writer)
{
    export_generic("camera", obj, writer);

    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_FILM) { // This is nested here
            writer.w() << ",";
            writer.endl();
            export_film(*child, writer);
        }
    }
}

static void export_texture(const std::string& name, const Object& obj, JsonWriter& writer)
{
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"" << obj.pluginType() << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }
    writer.objEnd();
}

static void export_bsdf(const std::string& name, const Object& obj, JsonWriter& writer, const std::vector<Object*>& textures, const std::vector<Object*>& bsdfs)
{
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"" << obj.pluginType() << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        if (pair.first == "material")
            continue;

        writer.key(translate(pair.first));

        if ((pair.first == "int_ior" || pair.first == "ext_ior")
            && pair.second.type() == PT_STRING) {
            const float ior = lookupIOR(pair.second.getString());
            writer.w() << ior;
        } else {
            export_property(pair.second, writer);
        }
        writer.w() << ",";
        writer.endl();
    }

    // TODO: We lose color information!
    const Property materialProp = obj.property("material");
    if (materialProp.type() == PT_STRING) {
        const float eta   = lookupEta(materialProp.getString());
        const float kappa = lookupKappa(materialProp.getString());

        writer.key("eta");
        writer.w() << eta << ",";
        writer.endl();
        writer.key("k");
        writer.w() << kappa << ",";
        writer.endl();
    }

    // Include textures
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), child.get()));
            writer.key("texture");
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    // Include bsdfs
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_BSDF) {
            int bsdf = std::distance(bsdfs.begin(), std::find(bsdfs.begin(), bsdfs.end(), child.get()));
            writer.key("bsdf");
            writer.w() << "\"" << make_id("bsdf", bsdf) << "\",";
            writer.endl();
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_BSDF) {
            int bsdf = std::distance(bsdfs.begin(), std::find(bsdfs.begin(), bsdfs.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("bsdf", bsdf) << "\",";
            writer.endl();
        }
    }
    writer.objEnd();
}

static void export_medium(const std::string& name, const Object& obj, JsonWriter& writer, const std::vector<Object*>& textures)
{
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"" << obj.pluginType() << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }

    // Include textures
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), child.get()));
            writer.key("texture");
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    writer.objEnd();
}

static void export_shape(const std::string& name, const Object& obj, JsonWriter& writer)
{
    std::string plugin_type = obj.pluginType();
    if (plugin_type == "serialized")
        plugin_type = "mitsuba";

    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"" << plugin_type << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }

    // Ignore Bsdf and Emitter, this will be handled in entity

    writer.objEnd();
}

static void export_area_light(const std::string& name, const Object& obj, JsonWriter& writer, int entity, const std::vector<Object*>& textures)
{
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"area\",";
    writer.endl();
    writer.key("entity");
    writer.w() << "\"" << make_id("entity", entity) << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }

    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), child.get()));
            writer.key("texture");
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }
    writer.objEnd();
}

static void export_light(const std::string& name, const Object& obj, JsonWriter& writer, const std::vector<Object*>& textures)
{
    if (obj.pluginType() == "sunsky") {
        // Split these two emitters appart
        Object obj2(obj.type(), "sun", obj.id() + "_sun");
        for (const auto& prop : obj.properties())
            obj2.setProperty(prop.first, prop.second);
        export_light(name + "_sun", obj2, writer, textures);

        writer.w() << ",";
        writer.endl();

        Object obj3(obj.type(), "sky", obj.id() + "_sky");
        for (const auto& prop : obj.properties())
            obj3.setProperty(prop.first, prop.second);
        export_light(name + "_sky", obj3, writer, textures);
        return;
    }

    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"" << obj.pluginType() << "\",";
    writer.endl();

    for (const auto& pair : obj.properties()) {
        writer.key(translate(pair.first));
        export_property(pair.second, writer);
        writer.w() << ",";
        writer.endl();
    }

    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), child.get()));
            writer.key("texture");
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_TEXTURE) {
            int tex = std::distance(textures.begin(), std::find(textures.begin(), textures.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("texture", tex) << "\",";
            writer.endl();
        }
    }
    writer.objEnd();
}

static std::string replace(const std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return str;
    std::string tmp = str;
    tmp.replace(start_pos, from.length(), to);
    return tmp;
}

static void export_entity(const std::string& name, const Object& obj, JsonWriter& writer, const std::vector<Object*>& bsdfs, const std::vector<Object*>& media)
{
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"" << name << "\",";
    writer.endl();
    writer.key("shape");
    writer.w() << "\"" << replace(name, "entity", "shape") << "\",";
    writer.endl();

    // Handle BSDF
    bool has_bsdf = false;
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_BSDF) {
            int bsdf = std::distance(bsdfs.begin(), std::find(bsdfs.begin(), bsdfs.end(), child.get()));
            writer.key("bsdf");
            writer.w() << "\"" << make_id("bsdf", bsdf) << "\",";
            writer.endl();
            has_bsdf = true;
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_BSDF) {
            int bsdf = std::distance(bsdfs.begin(), std::find(bsdfs.begin(), bsdfs.end(), pair.second.get()));
            writer.key(pair.first);
            writer.w() << "\"" << make_id("bsdf", bsdf) << "\",";
            writer.endl();
            has_bsdf = true;
        }
    }

    // Handle Media
    bool has_media = false;
    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_MEDIUM && (pair.first == "exterior" || pair.first == "interior")) {
            int medium = std::distance(media.begin(), std::find(media.begin(), media.end(), pair.second.get()));
            if (pair.first == "exterior")
                writer.key("outer_medium");
            else
                writer.key("inner_medium");
            writer.w() << "\"" << make_id("medium", medium) << "\",";
            writer.endl();
            has_media = true;
        }
    }

    // Handle special case
    if (!has_bsdf) {
        writer.key("bsdf");
        if (!has_media)
            writer.w() << "\"__black\",";
        else
            writer.w() << "\"__pass\",";
        writer.endl();
    }

    writer.objEnd();
}

static void extract_textures(const Object& obj, std::vector<Object*>& textures)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_TEXTURE) {
            textures.push_back(child.get());
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_TEXTURE) {
            textures.push_back(pair.second.get());
        }
    }

    for (const auto& child : obj.anonymousChildren()) {
        switch (child->type()) {
        case OT_BSDF:
            extract_textures(*child, textures);
            break;
        case OT_EMITTER:
            extract_textures(*child, textures);
            break;
        case OT_SHAPE:
            extract_textures(*child, textures);
            break;
        default:
            break;
        }
    }
}

static void extract_bsdfs(const Object& obj, std::vector<Object*>& bsdfs)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_BSDF) {
            bsdfs.push_back(child.get());
            extract_bsdfs(*child.get(), bsdfs);
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_BSDF) {
            bsdfs.push_back(pair.second.get());
            extract_bsdfs(*pair.second.get(), bsdfs);
        }
    }

    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_SHAPE) {
            extract_bsdfs(*child, bsdfs);
        }
    }
}

static void extract_media(const Object& obj, std::vector<Object*>& media)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_MEDIUM) {
            media.push_back(child.get());
            extract_bsdfs(*child.get(), media);
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_MEDIUM) {
            media.push_back(pair.second.get());
            extract_media(*pair.second.get(), media);
        }
    }

    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_SHAPE) {
            extract_media(*child, media);
        }
    }
}

static void extract_shapes(const Object& obj, std::vector<Object*>& shapes)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_SHAPE) {
            shapes.push_back(child.get());
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_SHAPE) {
            shapes.push_back(pair.second.get());
        }
    }
}

static void extract_area_lights(const Object& obj, std::vector<std::pair<Object*, Object*>>& area_lights)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_SHAPE) {
            for (const auto& child2 : child->anonymousChildren()) {
                if (child2->type() == OT_EMITTER)
                    area_lights.push_back({ child.get(), child2.get() });
            }
            for (const auto& pair : child->namedChildren()) {
                if (pair.second->type() == OT_EMITTER)
                    area_lights.push_back({ child.get(), pair.second.get() });
            }
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_SHAPE) {
            for (const auto& child2 : pair.second->anonymousChildren()) {
                if (child2->type() == OT_EMITTER)
                    area_lights.push_back({ pair.second.get(), child2.get() });
            }
            for (const auto& pair2 : pair.second->namedChildren()) {
                if (pair2.second->type() == OT_EMITTER)
                    area_lights.push_back({ pair.second.get(), pair2.second.get() });
            }
        }
    }
}

static void extract_lights(const Object& obj, std::vector<Object*>& lights)
{
    for (const auto& child : obj.anonymousChildren()) {
        if (child->type() == OT_EMITTER) {
            lights.push_back(child.get());
        }
    }

    for (const auto& pair : obj.namedChildren()) {
        if (pair.second->type() == OT_EMITTER) {
            lights.push_back(pair.second.get());
        }
    }
}

static void export_scene(const Scene& scene, JsonWriter& writer)
{
    // Common settings
    for (const auto& child : scene.anonymousChildren()) {
        switch (child->type()) {
        case OT_SENSOR:
            export_sensor(*child, writer);
            writer.w() << ",";
            writer.endl();
            break;
        case OT_INTEGRATOR:
            export_integrator(*child, writer);
            writer.w() << ",";
            writer.endl();
            break;
        default:
            break;
        }
    }

    // Textures
    std::vector<Object*> textures;
    extract_textures(scene, textures);

    if (!textures.empty()) {
        writer.key("textures");
        writer.arrBegin();
        writer.goIn();
        for (int i = 0; i < (int)textures.size(); ++i) {
            export_texture(make_id("texture", i), *textures[i], writer);
            writer.w() << ",";
            writer.endl();
        }
        writer.goOut();
        writer.arrEnd();
        writer.w() << ",";
        writer.endl();
    }

    // Bsdfs
    std::vector<Object*> bsdfs;
    extract_bsdfs(scene, bsdfs);

    writer.key("bsdfs");
    writer.arrBegin();
    writer.goIn();

    // Bsdf for entities without bsdfs
    writer.objBegin();
    writer.key("name");
    writer.w() << "\"__black\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"diffuse\"";
    writer.endl();
    writer.objEnd();
    writer.w() << ",";
    writer.endl();

    writer.objBegin();
    writer.key("name");
    writer.w() << "\"__pass\",";
    writer.endl();
    writer.key("type");
    writer.w() << "\"passthrough\"";
    writer.endl();
    writer.objEnd();
    writer.w() << ",";
    writer.endl();

    for (int i = 0; i < (int)bsdfs.size(); ++i) {
        export_bsdf(make_id("bsdf", i), *bsdfs[i], writer, textures, bsdfs);
        writer.w() << ",";
        writer.endl();
    }
    writer.goOut();
    writer.arrEnd();
    writer.w() << ",";
    writer.endl();

    // Media
    std::vector<Object*> media;
    extract_media(scene, media);

    if (!media.empty()) {
        writer.key("media");
        writer.arrBegin();
        writer.goIn();

        for (int i = 0; i < (int)media.size(); ++i) {
            export_medium(make_id("medium", i), *media[i], writer, textures);
            writer.w() << ",";
            writer.endl();
        }
        writer.goOut();
        writer.arrEnd();
        writer.w() << ",";
        writer.endl();
    }

    // Shapes
    std::vector<Object*> shapes;
    extract_shapes(scene, shapes);

    if (!shapes.empty()) {
        writer.key("shapes");
        writer.arrBegin();
        writer.goIn();
        for (int i = 0; i < (int)shapes.size(); ++i) {
            export_shape(make_id("shape", i), *shapes[i], writer);
            writer.w() << ",";
            writer.endl();
        }
        writer.goOut();
        writer.arrEnd();
        writer.w() << ",";
        writer.endl();

        // Entities
        writer.key("entities");
        writer.arrBegin();
        writer.goIn();
        for (int i = 0; i < (int)shapes.size(); ++i) {
            export_entity(make_id("entity", i), *shapes[i], writer, bsdfs, media);
            writer.w() << ",";
            writer.endl();
        }
        writer.goOut();
        writer.arrEnd();
        writer.w() << ",";
        writer.endl();
    }

    // Lights
    std::vector<std::pair<Object*, Object*>> area_lights;
    extract_area_lights(scene, area_lights);

    std::vector<Object*> lights;
    extract_lights(scene, lights);

    if (!area_lights.empty() || !lights.empty()) {
        writer.key("lights");
        writer.arrBegin();
        writer.goIn();
        for (int i = 0; i < (int)area_lights.size(); ++i) {
            int entity = std::distance(shapes.begin(), std::find(shapes.begin(), shapes.end(), area_lights[i].first));
            export_area_light(make_id("light", i), *area_lights[i].second, writer, entity, textures);
            writer.w() << ",";
            writer.endl();
        }
        for (int i = 0; i < (int)lights.size(); ++i) {
            export_light(make_id("light", i + area_lights.size()), *lights[i], writer, textures);
            writer.w() << ",";
            writer.endl();
        }
        writer.goOut();
        writer.arrEnd();
    }
}

int main(int argc, char** argv)
{
    if (argc <= 1) {
        usage();
        return EXIT_SUCCESS;
    }

    SceneLoader loader;
    bool whitespace = true;
    std::filesystem::path in_file;
    std::filesystem::path out_file;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-o")) {
                check_arg(argc, argv, i, 1);
                out_file = argv[i + 1];
                ++i;
            } else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
                sQuiet = true;
            } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
                sVerbose = true;
            } else if (!strcmp(argv[i], "--no-ws")) {
                whitespace = false;
            } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
                usage();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "--version")) {
                version();
                return EXIT_SUCCESS;
            } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--lookup")) {
                check_arg(argc, argv, i, 1);
                loader.addLookupDir(argv[i]);
            } else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--define")) {
                check_arg(argc, argv, i, 2);
                loader.addArgument(argv[i + 1], argv[i + 2]);
                i += 2;
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'" << std::endl;
                usage();
                return EXIT_FAILURE;
            }
        } else {
            if (in_file.empty()) {
                in_file = argv[i];
            } else {
                std::cerr << "Unexpected argument '" << argv[i] << "'" << std::endl;
                usage();
                return EXIT_FAILURE;
            }
        }
    }

    // Check some stuff
    if (in_file.empty()) {
        std::cerr << "No input file given" << std::endl;
        return EXIT_FAILURE;
    }

    if (out_file.empty()) {
        out_file = in_file;
        out_file.replace_extension(".json");
    }

    JsonWriter writer(out_file, whitespace);

    // Load
    try {
        Scene scene = loader.loadFromFile(in_file.generic_u8string());
        writer.objBegin();
        export_scene(scene, writer);
        writer.objEnd();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}