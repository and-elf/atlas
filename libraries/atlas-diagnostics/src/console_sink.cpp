#include "atlas/diagnostics/console_sink.hpp"

#include "atlas/diagnostics/severity.hpp"

#include <cstddef>

namespace atlas::diagnostics {

void ConsoleSink::write(const Record& record) {
    *stream_ << "[" << to_string(record.severity) << "] " << record.system << ": " << record.message;

    if (!record.details.empty()) {
        *stream_ << " {";
        for (std::size_t i = 0; i < record.details.size(); ++i) {
            if (i != 0) {
                *stream_ << ", ";
            }
            *stream_ << record.details[i].key << "=" << record.details[i].value;
        }
        *stream_ << "}";
    }

    *stream_ << "\n";
}

} // namespace atlas::diagnostics
