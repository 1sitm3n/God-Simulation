#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace godsim {

class Log {
public:
    static void init() {
        auto console = spdlog::stdout_color_mt("godsim");
        console->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        console->set_level(spdlog::level::trace);
        spdlog::set_default_logger(console);
    }

    template<typename... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::critical(fmt, std::forward<Args>(args)...);
    }
};

} // namespace godsim

// ─── Convenience Macros ───
#define LOG_TRACE(...)    godsim::Log::trace(__VA_ARGS__)
#define LOG_INFO(...)     godsim::Log::info(__VA_ARGS__)
#define LOG_WARN(...)     godsim::Log::warn(__VA_ARGS__)
#define LOG_ERROR(...)    godsim::Log::error(__VA_ARGS__)
#define LOG_CRITICAL(...) godsim::Log::critical(__VA_ARGS__)
