#include "TensorTreeLoader.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
// Only used in the building process
struct TensorTreeNode {
    std::vector<std::unique_ptr<TensorTreeNode>> Children;
    std::vector<float> Values;
};

using NodeValue = int32_t;
static inline bool isLeaf(NodeValue id) { return id < 0; }
static inline int32_t getLeafValueOffset(NodeValue id) { return -id - 1; }
static inline bool isLeafSingleValue(float val) { return val < 0; }

class TensorTreeComponent {
public:
    inline TensorTreeComponent()
    {
    }

    inline void addNode(const TensorTreeNode& node, NodeValue* parentValue)
    {
        if (node.Children.empty()) {
            size_t off = mValues.size();
            if (parentValue)
                *parentValue = -(static_cast<NodeValue>(off) + 1);

            bool single = node.Values.size() == 1;
            if (single) {
                mValues.emplace_back(-node.Values.front());
            } else {
                mValues.insert(mValues.end(), node.Values.begin(), node.Values.end());
            }
        } else {
            size_t off = mNodes.size();
            if (parentValue)
                *parentValue = static_cast<NodeValue>(off);

            // First create the entries to linearize access
            for (size_t i = 0; i < node.Children.size(); ++i)
                mNodes.emplace_back();

            // Recursively add nodes
            for (size_t i = 0; i < node.Children.size(); ++i)
                addNode(*node.Children[i], &mNodes[off + i]);
        }
    }

    inline void write(std::ostream& os)
    {
        uint32_t node_count  = mNodes.size();
        uint32_t value_count = mValues.size();

        os.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        os.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));

        os.write(reinterpret_cast<const char*>(mNodes.data()), mNodes.size() * sizeof(NodeValue));
        os.write(reinterpret_cast<const char*>(mValues.data()), mValues.size() * sizeof(float));
    }

private:
    std::vector<NodeValue> mNodes;
    std::vector<float> mValues;
};

bool TensorTreeLoader::prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data)
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
        const char* type = data.child_value("Wavelength");
        if (!type || strcmp(type, "Visible") != 0) // Skip entries for non-visible wavelengths
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
                        float val;
                        stream >> val;
                        node->Values.emplace_back(val);
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

        // FIXME: We require children here, not a leaf. This is a limitation from our side
        if (root->Children.empty()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Root of scatter data has no children" << std::endl;
            return false;
        }

        // Setup component
        std::shared_ptr<TensorTreeComponent> component = std::make_shared<TensorTreeComponent>();
        component->addNode(*root, nullptr);

        // Select correct component
        const std::string direction = block.child_value("WavelengthDataDirection");
        if (direction == "Transmission Front")
            transmissionFront = component;
        else if (direction == "Scattering Back")
            reflectionBack = component;
        else if (direction == "Transmission Back")
            transmissionBack = component;
        else
            reflectionFront = component;
    }

    // FIXME: This is not the standard radiance uses. It should be handled as "black"
    if (!reflectionBack)
        reflectionBack = reflectionFront;
    if (!reflectionFront)
        reflectionFront = reflectionBack;

    // Make sure both transmission parts are equal if not specified otherwise
    if (!transmissionBack)
        transmissionBack = transmissionFront;
    if (!transmissionFront)
        transmissionFront = transmissionBack;

    if (!reflectionFront && !reflectionBack && !transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No valid data found" << std::endl;
        return false;
    }

    if (!transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "While parsing " << in_xml << ": No transmission data found" << std::endl;
        return false;
    }

    if (!reflectionFront || !reflectionBack || !transmissionFront || !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Incomplete data found" << std::endl;
        return false;
    }

    std::ofstream stream(out_data, std::ios::binary);
    reflectionFront->write(stream);
    transmissionFront->write(stream);
    reflectionBack->write(stream);
    transmissionBack->write(stream);

    return true;
}

} // namespace IG