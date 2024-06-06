#include "DeviceInfo.h"

#include "Utils/Logger.h"

MAA_CTRL_UNIT_NS_BEGIN

bool DeviceInfo::parse(const json::value& config)
{
    static const json::array kDefaultUuidArgv = {
        "{ADB}", "-s", "{ADB_SERIAL}", "shell", "settings get secure android_id",
    };
    static const json::array kDefaultResolutionArgv = {
        "{ADB}",
        "-s",
        "{ADB_SERIAL}",
        "shell",
        "dumpsys window displays | grep -o -E cur=+[^\\ ]+ | grep -o -E [0-9]+",
    };
    static const json::array kDefaultOrientationArgv = {
        "{ADB}",
        "-s",
        "{ADB_SERIAL}",
        "shell",
        "dumpsys input | grep SurfaceOrientation | grep -m 1 -o -E [0-9]",
    };

    return parse_command("UUID", config, kDefaultUuidArgv, uuid_argv_)
           && parse_command("Resolution", config, kDefaultResolutionArgv, resolution_argv_)
           && parse_command("Orientation", config, kDefaultOrientationArgv, orientation_argv_);
}

std::optional<std::string> DeviceInfo::request_uuid()
{
    LogFunc;

    auto argv_opt = uuid_argv_.gen(argv_replace_);
    if (!argv_opt) {
        return std::nullopt;
    }

    auto output_opt = startup_and_read_pipe(*argv_opt);
    if (!output_opt) {
        return std::nullopt;
    }

    auto& uuid_str = output_opt.value();
    std::erase_if(uuid_str, [](char c) { return !std::isdigit(c) && !std::isalpha(c); });

    return uuid_str;
}

std::optional<std::pair<int, int>> DeviceInfo::request_resolution()
{
    LogFunc;

    auto argv_opt = resolution_argv_.gen(argv_replace_);
    if (!argv_opt) {
        return std::nullopt;
    }

    auto output_opt = startup_and_read_pipe(*argv_opt);
    if (!output_opt) {
        return std::nullopt;
    }

    int width = 0, height = 0;

    std::istringstream iss(output_opt.value());
    iss >> width >> height;

    auto orientation_opt = request_orientation();
    if (!orientation_opt) {
        LogWarn << "failed to request_orientation" << VAR(width) << VAR(height);
        return std::make_pair(width, height);
    }

    int orientation = *orientation_opt;
    switch (orientation) {
    case 0:
    case 2:
        std::tie(width, height) = std::minmax(width, height);
        LogInfo << "portrait" << VAR(orientation) << VAR(width) << VAR(height);
        break;
    case 1:
    case 3:
        std::tie(height, width) = std::minmax(width, height);
        LogInfo << "landscape" << VAR(orientation) << VAR(width) << VAR(height);
        break;
    default:
        LogWarn << "unknown" << VAR(orientation) << VAR(width) << VAR(height);
        break;
    }

    return std::make_pair(width, height);
}

std::optional<int> DeviceInfo::request_orientation()
{
    LogFunc;

    auto argv_opt = orientation_argv_.gen(argv_replace_);
    if (!argv_opt) {
        return std::nullopt;
    }

    auto output_opt = startup_and_read_pipe(*argv_opt);
    if (!output_opt) {
        return std::nullopt;
    }

    const auto& s = output_opt.value();

    if (s.empty()) {
        return std::nullopt;
    }

    int ori = s.front() - '0';

    if (!(ori >= 0 && ori <= 3)) {
        return std::nullopt;
    }

    return ori;
}

MAA_CTRL_UNIT_NS_END
