#include "atlas/contract_gen/template_engine.hpp"

#include <stdexcept>

namespace atlas::contract_gen {

std::string render_template(std::string_view template_text,
                            const std::map<std::string, std::string>& values) {
    std::string result;
    result.reserve(template_text.size());

    std::size_t pos = 0;
    while (pos < template_text.size()) {
        const auto open = template_text.find("{{", pos);
        if (open == std::string_view::npos) {
            result.append(template_text.substr(pos));
            break;
        }
        result.append(template_text.substr(pos, open - pos));

        const auto close = template_text.find("}}", open + 2);
        if (close == std::string_view::npos) {
            throw std::invalid_argument("unterminated '{{' placeholder starting at offset " +
                                        std::to_string(open));
        }

        const std::string name(template_text.substr(open + 2, close - (open + 2)));
        const auto it = values.find(name);
        if (it == values.end()) {
            throw std::invalid_argument("unresolved placeholder '{{" + name + "}}'");
        }
        result.append(it->second);

        pos = close + 2;
    }

    return result;
}

} // namespace atlas::contract_gen
