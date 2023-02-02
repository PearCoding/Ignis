#include "TensorTreeLoader.h"
#include "Logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
// Only used in the building process
struct TensorTreeNode {
    std::vector<std::unique_ptr<TensorTreeNode>> Children;
    std::vector<float> Values;

    inline bool isLeaf() const { return Children.empty(); }

    // The root is the only node with one child. Get rid of this special case
    inline void eat()
    {
        IG_ASSERT(Values.empty() && Children.size() == 1, "Invalid call to eat");

        std::swap(Children[0]->Values, Values);
        std::swap(Children[0]->Children, Children);
    }

    inline float computeTotal(size_t depth) const
    {
        const float area = 1.0f / (depth * (Values.size() + Children.size()));

        float total = 0;
        for (const auto& child : Children)
            total += child->computeTotal(depth + 1);

        for (float val : Values)
            total += Pi * val * area;

        return total;
    }

    inline void dump(std::ostream& stream) const
    {
        if (isLeaf()) {
            stream << "[ ";
            for (const auto& c : Values)
                stream << c << " ";
            stream << "]";
        } else {
            stream << "{ ";
            for (const auto& c : Children)
                c->dump(stream);
            stream << " }";
        }
    }
};

using NodeValue = int32_t;

static_assert(sizeof(NodeValue) == sizeof(float), "Node and Leaf value elements have to have the same size");
class TensorTreeComponent {
public:
    inline explicit TensorTreeComponent(uint32 ndim)
        : mNDim(ndim)
        , mMaxValuesPerNode(1 << ndim)
        , mTotal(0)
        , mRootIsLeaf(false)
    {
    }

    inline void addNode(const TensorTreeNode& node, std::optional<size_t> parentOffsetValue)
    {
        if (node.isLeaf()) {
            size_t off = mValues.size();
            if (parentOffsetValue.has_value())
                mNodes[parentOffsetValue.value()] = -(static_cast<NodeValue>(off) + 1);
            else
                mRootIsLeaf = true;

            bool single = node.Values.size() == 1;
            if (single) {
                mValues.emplace_back((float)std::copysign(node.Values.front(), -1));
            } else {
                IG_ASSERT(node.Values.size() == mMaxValuesPerNode, "Expected valid number of values in a leaf");
                mValues.insert(mValues.end(), node.Values.begin(), node.Values.end());
            }
        } else {
            IG_ASSERT(node.Children.size() == mMaxValuesPerNode, "Expected valid number of children in a node");

            size_t off = mNodes.size();
            if (parentOffsetValue.has_value())
                mNodes[parentOffsetValue.value()] = static_cast<NodeValue>(off);

            // First create the entries to linearize access
            for (size_t i = 0; i < node.Children.size(); ++i)
                mNodes.emplace_back();

            // Recursively add nodes
            for (size_t i = 0; i < node.Children.size(); ++i)
                addNode(*node.Children[i], off + i);
        }
    }

    inline void makeBlack()
    {
        mNodes  = std::vector<NodeValue>(mMaxValuesPerNode, 0);
        mValues = std::vector<float>(1, static_cast<float>(std::copysign(0, -1)));
        mTotal  = 0;

        std::fill(mNodes.begin(), mNodes.end(), -1);
    }

    inline void write(std::ostream& os) const
    {
        auto node_count  = static_cast<uint32>(mNodes.size());
        auto value_count = static_cast<uint32>(mValues.size());

        // We do not make use of this header, but it might get handy in other applications
        os.write(reinterpret_cast<const char*>(&mNDim), sizeof(mNDim));
        os.write(reinterpret_cast<const char*>(&mMaxValuesPerNode), sizeof(mMaxValuesPerNode));
        os.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
        os.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));

        os.write(reinterpret_cast<const char*>(mNodes.data()), mNodes.size() * sizeof(NodeValue));
        os.write(reinterpret_cast<const char*>(mValues.data()), mValues.size() * sizeof(float));
    }

    [[nodiscard]] inline size_t nodeCount() const { return mNodes.size(); }
    [[nodiscard]] inline size_t valueCount() const { return mValues.size(); }

    inline void setTotal(float f) { mTotal = f; }
    [[nodiscard]] inline float total() const { return mTotal; }
    [[nodiscard]] inline float isRootLeaf() const { return mRootIsLeaf; }

private:
    uint32 mNDim;
    uint32 mMaxValuesPerNode;
    std::vector<NodeValue> mNodes;
    std::vector<float> mValues;
    float mTotal;
    bool mRootIsLeaf;
};

bool TensorTreeLoader::prepare(const Path& in_xml, const Path& out_data, TensorTreeSpecification& spec)
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
        component->addNode(*root, {});
        component->setTotal(root->computeTotal(1));

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

    std::ofstream stream(out_data, std::ios::binary | std::ios::trunc);
    reflectionFront->write(stream);
    transmissionFront->write(stream);
    reflectionBack->write(stream);
    transmissionBack->write(stream);

    // Fill missing parts in the specification
    const auto assignSpec = [](TensorTreeComponentSpecification& spec, const TensorTreeComponent& comp) {
        spec.node_count   = comp.nodeCount();
        spec.value_count  = comp.valueCount();
        spec.total        = comp.total();
        spec.root_is_leaf = comp.isRootLeaf();
    };

    assignSpec(spec.front_reflection, *reflectionFront);
    assignSpec(spec.back_reflection, *reflectionBack);
    assignSpec(spec.front_transmission, *transmissionFront);
    assignSpec(spec.back_transmission, *transmissionBack);

    return true;
}

} // namespace IG