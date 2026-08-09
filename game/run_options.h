#pragma once

#include <cstdint>
#include <string>

namespace voxel::game {

struct RunOptions {
    long maxFrames = 0;
    std::uint32_t seed = 2024u;
    std::string screenshotPath;
    std::string screenshotDirectory;
    long screenshotFrame = 48;
    long screenshotInterval = 0;
};

enum class ParseOptionsResult {
    Run,
    Help,
    Error,
};

// Parse desktop smoke-test and screenshot-capture options without touching
// SDL or global process state. On Error, `message` contains a user-facing
// diagnostic. Positional [maxFrames] [seed] remains supported.
ParseOptionsResult parseRunOptions(int argc, char* argv[], RunOptions& out,
                                   std::string& message);

std::string runOptionsUsage(const char* program);

}  // namespace voxel::game
