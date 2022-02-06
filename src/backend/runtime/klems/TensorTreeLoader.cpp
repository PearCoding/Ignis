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

    // The root is the only node with one child. Get rid of this special case
    inline void eat()
    {
        IG_ASSERT(Values.empty() && Children.size() == 1, "Invalid call to eat");

        std::swap(Children[0]->Values, Values);
        std::swap(Children[0]->Children, Children);
    }

    // Will expand the node such that it has children instead of values
    // This is actually only used in a special case on the root
    inline void expand(size_t ndim)
    {
        IG_ASSERT(!Values.empty() && Children.empty(), "Invalid call to expand");

        Children.resize(ndim);
        for (size_t i = 0; i < Children.size(); ++i) {
            Children[i]         = std::make_unique<TensorTreeNode>();
            Children[i]->Values = std::vector<float>{ Values[Values.size() == 1 ? 0 : i] };
        }

        Values.clear();
    }
};

using NodeValue = int32_t;

static_assert(sizeof(NodeValue) == sizeof(float), "Node and Leaf value elements have to have the same size");
class TensorTreeComponent {
public:
    inline TensorTreeComponent(uint32 ndim)
        : mNDim(ndim)
        , mMaxValuesPerNode(1 << ndim)
    {
    }

    inline void addNode(const TensorTreeNode& node, NodeValue* parentValue)
    {
        if (node.Children.empty()) {
            IG_ASSERT(parentValue != nullptr, "Root can not be a leaf!");
            size_t off   = mValues.size();
            *parentValue = -(static_cast<NodeValue>(off) + 1);

            bool single = node.Values.size() == 1;
            if (single) {
                mValues.emplace_back(std::copysign(node.Values.front(), -1));
            } else {
                IG_ASSERT(node.Values.size() == mMaxValuesPerNode, "Expected valid number of values in a leaf");
                mValues.insert(mValues.end(), node.Values.begin(), node.Values.end());
            }
        } else {
            IG_ASSERT(node.Children.size() == mMaxValuesPerNode, "Expected valid number of children in a node");

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

    inline void makeBlack()
    {
        mNodes  = std::vector<NodeValue>(mMaxValuesPerNode, 0);
        mValues = std::vector<float>(1, std::copysign(0, -1));

        for (size_t i = 0; i < mNodes.size(); ++i)
            mNodes[i] = -1;
    }

    inline void write(std::ostream& os)
    {
        uint32_t node_count  = mNodes.size();
        uint32_t value_count = mValues.size();

        // We do not make use of this header, but it might get handy in other applications
        os.write(reinterpret_cast<const char*>(&mNDim), sizeof(mNDim));
        os.write(reinterpret_cast<const char*>(&mMaxValuesPerNode), sizeof(mMaxValuesPerNode));
        os.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        os.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));

        os.write(reinterpret_cast<const char*>(mNodes.data()), mNodes.size() * sizeof(NodeValue));
        os.write(reinterpret_cast<const char*>(mValues.data()), mValues.size() * sizeof(float));
    }

    inline size_t nodeCount() const { return mNodes.size(); }
    inline size_t valueCount() const { return mValues.size(); }

private:
    uint32 mNDim;
    uint32 mMaxValuesPerNode;
    std::vector<NodeValue> mNodes;
    std::vector<float> mValues;
};

bool TensorTreeLoader::prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data, TensorTreeSpecification& spec)
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

    spec.ndim = dim4 ? 4 : 3;

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
                        float val;
                        stream >> val;
                        if (std::signbit(val) && !did_warn_sign) {
                            IG_LOG(L_WARNING) << "Data contains negative values in " << in_xml << ": Use absolute value instead" << std::endl;
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
            if (root->Children.empty()) {
                if (root->Values.empty()) {
                    IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Root of scatter data has no data" << std::endl;
                    return false;
                } else {
                    root->expand(max_values_per_node);
                }
            }
        }

        // Setup component
        std::shared_ptr<TensorTreeComponent> component = std::make_shared<TensorTreeComponent>(dim4 ? 4 : 3);
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

    spec.has_reflection = reflectionFront || reflectionBack;

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
    if (!transmissionBack)
        transmissionBack = transmissionFront;
    if (!transmissionFront)
        transmissionFront = transmissionBack;

    if (!transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "While parsing " << in_xml << ": No transmission data found" << std::endl;
        return false;
    }

    if (!reflectionFront || !reflectionBack || !transmissionFront || !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Incomplete data found" << std::endl;
        return false;
    }

    std::ofstream stream(out_data, std::ios::binary | std::ios::trunc);
    reflectionFront->write(stream);
    transmissionFront->write(stream);
    reflectionBack->write(stream);
    transmissionBack->write(stream);

    // Fill missing parts in the specification
    spec.front_reflection.node_count    = reflectionFront->nodeCount();
    spec.front_reflection.value_count   = reflectionFront->valueCount();
    spec.back_reflection.node_count     = reflectionBack->nodeCount();
    spec.back_reflection.value_count    = reflectionBack->valueCount();
    spec.front_transmission.node_count  = transmissionFront->nodeCount();
    spec.front_transmission.value_count = transmissionFront->valueCount();
    spec.back_transmission.node_count   = transmissionBack->nodeCount();
    spec.back_transmission.value_count  = transmissionBack->valueCount();

    return true;
}

} // namespace IG