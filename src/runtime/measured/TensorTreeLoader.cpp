#include "TensorTreeLoader.h"
#include "Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
bool TensorTreeLoader::load(const Path& in_xml, TensorTree& tree)
{
    // Read Radiance based klems BSDF xml document
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(in_xml.c_str());
    if (!result) {
        IG_LOG(L_ERROR) << "Could not load file " << in_xml << ": " << result.description() << std::endl;
        return false;
    }

    const auto layer = doc.child("WindowElement").child("Optical").child("Layer");
    if (!layer) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No Layer tag" << std::endl;
        return false;
    }

    // Extract data definition and therefor also angle basis
    const auto datadefinition = layer.child("DataDefinition");
    if (!datadefinition) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No DataDefinition tag" << std::endl;
        return false;
    }

    std::string type = datadefinition.child_value("IncidentDataStructure");
    bool dim4        = type == "TensorTree4";
    if (!dim4 && type != "TensorTree3") {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Expected IncidentDataStructure of 'TensorTree4' or 'TensorTree3' but got '" << type << "' instead" << std::endl;
        return false;
    }

    std::shared_ptr<TensorTreeComponent> reflectionFront;
    std::shared_ptr<TensorTreeComponent> transmissionFront;
    std::shared_ptr<TensorTreeComponent> reflectionBack;
    std::shared_ptr<TensorTreeComponent> transmissionBack;
    // Extract wavelengths
    for (const auto& data : layer.children("WavelengthData")) {
        const char* wvl_type = data.child_value("Wavelength");
        if (!wvl_type || strcmp(wvl_type, "Visible") != 0) // Skip entries for non-visible wavelengths
            continue;

        const auto block = data.child("WavelengthDataBlock");
        if (!block) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No WavelengthDataBlock given" << std::endl;
            return false;
        }

        // Connect angle basis
        std::string basisName = block.child_value("AngleBasis");
        if (basisName.empty()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": WavelengthDataBlock has no angle basis given" << std::endl;
            return false;
        }

        if (basisName != "LBNL/Shirley-Chiu") {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": AngleBasis is not 'LBNL/Shirley-Chiu'" << std::endl;
            return false;
        }

        // Parse tree data
        const char* scat_str = block.child_value("ScatteringData");

        std::unique_ptr<TensorTreeNode> root = std::make_unique<TensorTreeNode>();
        const size_t max_values_per_node     = dim4 ? 16 : 8;
        std::vector<TensorTreeNode*> node_stack;
        node_stack.push_back(root.get());

        bool did_warn_sign = false;

        std::stringstream stream(scat_str);
        const auto eof = std::char_traits<std::stringstream::char_type>::eof();
        while (stream.good()) {
            const auto c = stream.peek();
            if (c == eof) {
                break;
            } else if (c == '{') {
                stream.ignore();
                node_stack.emplace_back(node_stack.back()->Children.emplace_back(std::make_unique<TensorTreeNode>()).get());
            } else if (c == '}') {
                if (node_stack.empty()) {
                    IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Misformed scatter data" << std::endl;
                    return false;
                }

                stream.ignore();
                node_stack.pop_back();
            } else if (c == ',' || std::isspace(c)) {
                stream.ignore();
            } else {
                TensorTreeNode* node = node_stack.back();
                while (stream.good() && node->Values.size() < max_values_per_node) {
                    const auto c2 = stream.peek();
                    if (c2 == eof) {
                        break; // Should not happen
                    } else if (c2 == '}') {
                        break;
                    } else if (c2 == ',' || std::isspace(c2)) {
                        stream.ignore();
                    } else {
                        float val = 0;
                        stream >> val;
                        if (std::signbit(val) && !did_warn_sign) {
                            IG_LOG(L_WARNING) << "Data contains negative values in " << in_xml << ": Using absolute value instead" << std::endl;
                            did_warn_sign = true;
                        }
                        node->Values.emplace_back(std::abs(val));
                    }
                }

                if (node->Values.size() != 1 && node->Values.size() != max_values_per_node) {
                    IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Misformed scatter data. Bad amount of values per node" << std::endl;
                    return false;
                }
            }
        }

        // Only the root shall remain
        if (node_stack.size() != 1) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Misformed scatter data" << std::endl;
            return false;
        }

        // Make sure the root node has children instead of being a leaf
        if (root->Children.empty()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Root of scatter data has no data" << std::endl;
            return false;
        } else if (root->Children.size() != 1) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Root of scatter data has invalid data" << std::endl;
            return false;
        } else {
            root->eat(); // Eat the only node we have
        }

        if (root->Children.empty() && root->Values.empty()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No data given" << std::endl;
            return false;
        }

        // root->dump(std::cout);
        // std::cout << std::endl;

        // Setup component
        std::shared_ptr<TensorTreeComponent> component = std::make_shared<TensorTreeComponent>(dim4 ? 4 : 3);
        component->setRoot(std::move(root));

        // Select correct component
        // The window definition flips the front & back
        const std::string direction = block.child_value("WavelengthDataDirection");
        if (direction == "Transmission Front")
            transmissionBack = component;
        else if (direction == "Scattering Back" || direction == "Reflection Back")
            reflectionFront = component;
        else if (direction == "Transmission Back")
            transmissionFront = component;
        else
            reflectionBack = component;
    }

    // If reflection components are not given, make them black
    // See docs/notes/BSDFdirections.txt in Radiance for more information
    if (!reflectionBack) {
        reflectionBack = std::make_shared<TensorTreeComponent>(4);
        reflectionBack->makeBlack();
    }
    if (!reflectionFront) {
        reflectionFront = std::make_shared<TensorTreeComponent>(4);
        reflectionFront->makeBlack();
    }

    // Make sure both transmission parts are equal if not specified otherwise
    if (!transmissionBack || (transmissionFront && transmissionBack->total() <= FltEps))
        transmissionBack = transmissionFront;
    if (!transmissionFront || (transmissionBack && transmissionFront->total() <= FltEps))
        transmissionFront = transmissionBack;

    if (!transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "While parsing " << in_xml << ": No transmission data found" << std::endl;
        return false;
    }

    if (!reflectionFront || !reflectionBack || !transmissionFront || !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Incomplete data found" << std::endl;
        return false;
    }

    tree.is_isotropic       = !dim4;
    tree.front_reflection   = std::move(reflectionFront);
    tree.back_reflection    = std::move(reflectionBack);
    tree.front_transmission = std::move(transmissionFront);
    tree.back_transmission  = std::move(transmissionBack);

    return true;
}

bool TensorTreeLoader::prepare(const Path& in_xml, const Path& out_data, TensorTreeSpecification& spec)
{
    TensorTree tree;
    if (!load(in_xml, tree))
        return false;

    // Fill missing parts in the specification
    const auto assignSpec = [](TensorTreeComponentSpecification& spec, const TensorTreeComponent& comp) {
        spec.node_count   = comp.nodeCount();
        spec.value_count  = comp.valueCount();
        spec.total        = comp.total();
        spec.root_is_leaf = comp.isRootLeaf();
        spec.min_proj_sa  = comp.minProjSA();
    };

    spec.ndim = tree.is_isotropic ? 3 : 4;
    assignSpec(spec.front_reflection, *tree.front_reflection);
    assignSpec(spec.back_reflection, *tree.back_reflection);
    assignSpec(spec.front_transmission, *tree.front_transmission);
    assignSpec(spec.back_transmission, *tree.back_transmission);

    if (!out_data.empty()) {
        // Note: Order matters!
        std::ofstream stream(out_data, std::ios::binary | std::ios::trunc);
        tree.front_reflection->write(stream);
        tree.front_transmission->write(stream);
        tree.back_reflection->write(stream);
        tree.back_transmission->write(stream);
    }

    return true;
}

} // namespace IG