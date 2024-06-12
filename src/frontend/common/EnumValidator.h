#pragma once

#include <CLI/CLI.hpp>

namespace IG {
class EnumValidator : public CLI::Validator {
public:
    using filter_fn_t = std::function<std::string(std::string)>;

    /// This allows in-place construction
    template <typename... Args>
    EnumValidator(std::initializer_list<std::pair<std::string, std::string>> values, Args&&... args)
        : EnumValidator(CLI::TransformPairs<std::string>(values), std::forward<Args>(args)...)
    {
    }

    /// direct map of std::string to std::string
    template <typename T>
    explicit EnumValidator(T&& mapping)
        : EnumValidator(std::forward<T>(mapping), nullptr)
    {
    }

    /// This checks to see if an item is in a set: pointer or copy version. You can pass in a function that will filter
    /// both sides of the comparison before computing the comparison.
    template <typename T, typename F>
    explicit EnumValidator(T mapping, F filter_function)
    {

        static_assert(CLI::detail::pair_adaptor<typename CLI::detail::element_type<T>::type>::value,
                      "mapping must produce value pairs");
        // Get the type of the contained item - requires a container have ::value_type
        // if the type does not have first_type and second_type, these are both value_type
        using element_t    = typename CLI::detail::element_type<T>::type;               // Removes (smart) pointers if needed
        using item_t       = typename CLI::detail::pair_adaptor<element_t>::first_type; // Is value_type if not a map
        using local_item_t = typename CLI::IsMemberType<item_t>::type;                  // Will convert bad types to good ones
                                                                                        // (const char * to std::string)

        // Make a local copy of the filter function, using a std::function if not one already
        std::function<local_item_t(local_item_t)> filter_fn = filter_function;

        // This is the type name for help, it will take the current version of the set contents
        desc_function_ = [mapping]() { return CLI::detail::generate_map(CLI::detail::smart_deref(mapping), true /*The only difference*/); };

        func_ = [mapping, filter_fn](std::string& input) {
            local_item_t b;
            if (!CLI::detail::lexical_cast(input, b)) {
                return std::string();
                // there is no possible way we can match anything in the mapping if we can't convert so just return
            }
            if (filter_fn) {
                b = filter_fn(b);
            }
            auto res = CLI::detail::search(mapping, b, filter_fn);
            if (res.first) {
                input = CLI::detail::value_string(CLI::detail::pair_adaptor<element_t>::second(*res.second));
            }
            return std::string{};
        };
    }

    /// You can pass in as many filter functions as you like, they nest
    template <typename T, typename... Args>
    EnumValidator(T&& mapping, filter_fn_t filter_fn_1, filter_fn_t filter_fn_2, Args&&... other)
        : EnumValidator(
            std::forward<T>(mapping),
            [filter_fn_1, filter_fn_2](std::string a) { return filter_fn_2(filter_fn_1(a)); },
            other...)
    {
    }
};
} // namespace IG