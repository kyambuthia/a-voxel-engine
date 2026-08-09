#include "game/run_options.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sstream>

namespace voxel::game {
namespace {

bool parseNonNegativeLong(const char* text, long& out) {
    if (text == nullptr || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (errno == ERANGE || end == nullptr || *end != '\0' || value < 0) {
        return false;
    }
    out = value;
    return true;
}

bool parseSeed(const char* text, std::uint32_t& out) {
    if (text == nullptr || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 0);
    if (errno == ERANGE || end == nullptr || *end != '\0' ||
        value > 0xfffffffful) {
        return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

}  // namespace

std::string runOptionsUsage(const char* program) {
    std::ostringstream out;
    out << "Usage: " << (program ? program : "a-voxel-engine")
        << " [maxFrames] [seed] [options]\n"
           "  --frames N          Stop after N rendered frames.\n"
           "  --seed N            Deterministic world seed (decimal or 0x...).\n"
           "  --capture PATH      Save a PNG from the rendered back buffer.\n"
           "  --capture-frame N   Capture on rendered frame N (default: 48).\n"
           "  --capture-dir PATH  Save a numbered PNG sequence while playing.\n"
           "  --capture-every N   Capture every N frames with --capture-dir.\n"
           "  --help              Show this help.\n\n"
           "The positional maxFrames and seed form is retained for smoke tests.\n";
    return out.str();
}

ParseOptionsResult parseRunOptions(int argc, char* argv[], RunOptions& out,
                                   std::string& message) {
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") {
            message = runOptionsUsage(argv[0]);
            return ParseOptionsResult::Help;
        }

        const auto next = [&]() -> const char* {
            if (i + 1 >= argc) {
                message = "Missing value for " + arg;
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--frames") {
            const char* value = next();
            if (value == nullptr || !parseNonNegativeLong(value, out.maxFrames)) {
                if (message.empty()) {
                    message = "--frames requires a non-negative integer";
                }
                return ParseOptionsResult::Error;
            }
        } else if (arg == "--seed") {
            const char* value = next();
            if (value == nullptr || !parseSeed(value, out.seed)) {
                if (message.empty()) {
                    message = "--seed requires a 32-bit integer";
                }
                return ParseOptionsResult::Error;
            }
        } else if (arg == "--capture") {
            const char* value = next();
            if (value == nullptr || value[0] == '\0') {
                if (message.empty()) {
                    message = "--capture requires an output path";
                }
                return ParseOptionsResult::Error;
            }
            out.screenshotPath = value;
        } else if (arg == "--capture-frame") {
            const char* value = next();
            if (value == nullptr ||
                !parseNonNegativeLong(value, out.screenshotFrame) ||
                out.screenshotFrame < 1) {
                if (message.empty()) {
                    message = "--capture-frame requires an integer of at least 1";
                }
                return ParseOptionsResult::Error;
            }
        } else if (arg == "--capture-dir") {
            const char* value = next();
            if (value == nullptr || value[0] == '\0') {
                if (message.empty()) {
                    message = "--capture-dir requires an output directory";
                }
                return ParseOptionsResult::Error;
            }
            out.screenshotDirectory = value;
        } else if (arg == "--capture-every") {
            const char* value = next();
            if (value == nullptr ||
                !parseNonNegativeLong(value, out.screenshotInterval) ||
                out.screenshotInterval < 1) {
                if (message.empty()) {
                    message = "--capture-every requires an integer of at least 1";
                }
                return ParseOptionsResult::Error;
            }
        } else if (!arg.empty() && arg[0] == '-') {
            message = "Unknown option: " + arg;
            return ParseOptionsResult::Error;
        } else if (positional == 0) {
            if (!parseNonNegativeLong(argv[i], out.maxFrames)) {
                message = "maxFrames requires a non-negative integer";
                return ParseOptionsResult::Error;
            }
            ++positional;
        } else if (positional == 1) {
            if (!parseSeed(argv[i], out.seed)) {
                message = "seed requires a 32-bit integer";
                return ParseOptionsResult::Error;
            }
            ++positional;
        } else {
            message = "Too many positional arguments";
            return ParseOptionsResult::Error;
        }
    }

    if (!out.screenshotPath.empty() && !out.screenshotDirectory.empty()) {
        message = "Use either --capture or --capture-dir, not both";
        return ParseOptionsResult::Error;
    }
    if (!out.screenshotDirectory.empty() && out.screenshotInterval == 0) {
        message = "--capture-dir requires --capture-every N";
        return ParseOptionsResult::Error;
    }
    if (out.screenshotDirectory.empty() && out.screenshotInterval != 0) {
        message = "--capture-every requires --capture-dir PATH";
        return ParseOptionsResult::Error;
    }
    if (!out.screenshotPath.empty() && out.maxFrames < out.screenshotFrame) {
        out.maxFrames = out.screenshotFrame;
    }
    return ParseOptionsResult::Run;
}

}  // namespace voxel::game
