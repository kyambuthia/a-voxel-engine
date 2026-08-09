#include "test_harness.h"

#include <string>
#include <vector>

#include "game/run_options.h"

using voxel::game::ParseOptionsResult;
using voxel::game::RunOptions;

namespace {

ParseOptionsResult parse(std::vector<std::string> args, RunOptions& options,
                         std::string& message) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    return voxel::game::parseRunOptions(static_cast<int>(argv.size()),
                                        argv.data(), options, message);
}

}  // namespace

TEST(run_options_defaults) {
    RunOptions options;
    std::string message;
    CHECK(parse({"voxel"}, options, message) == ParseOptionsResult::Run);
    CHECK_EQ(options.seed, 2024u);
    CHECK_EQ(options.maxFrames, 0);
}

TEST(run_options_capture_sequence) {
    RunOptions options;
    std::string message;
    CHECK(parse({"voxel", "--seed", "0x2a", "--capture-dir", "shots",
                 "--capture-every", "120"},
                options, message) == ParseOptionsResult::Run);
    CHECK_EQ(options.seed, 42u);
    CHECK_EQ(options.screenshotInterval, 120);
    CHECK(options.screenshotDirectory == "shots");
}

TEST(run_options_single_capture_extends_frame_limit) {
    RunOptions options;
    std::string message;
    CHECK(parse({"voxel", "--frames", "2", "--capture", "frame.png",
                 "--capture-frame", "10"},
                options, message) == ParseOptionsResult::Run);
    CHECK_EQ(options.maxFrames, 10);
}

TEST(run_options_help_is_not_an_error_in_any_position) {
    RunOptions options;
    std::string message;
    CHECK(parse({"voxel", "--seed", "7", "--help"}, options, message) ==
          ParseOptionsResult::Help);
    CHECK(message.find("Usage:") != std::string::npos);
}

TEST(run_options_rejects_invalid_combinations_and_overflow) {
    RunOptions options;
    std::string message;
    CHECK(parse({"voxel", "--capture-dir", "shots"}, options, message) ==
          ParseOptionsResult::Error);

    options = RunOptions{};
    message.clear();
    CHECK(parse({"voxel", "--frames", "999999999999999999999999"}, options,
                message) == ParseOptionsResult::Error);

    options = RunOptions{};
    message.clear();
    CHECK(parse({"voxel", "--seed", "-1"}, options, message) ==
          ParseOptionsResult::Error);
}
