#pragma once

/**
 * Logger - spdlog 日志封装
 *
 * 功能:
 * - 同时输出到控制台和文件
 * - 按天滚动日志
 * - 最多保留7天
 * - 显示文件名和函数名
 */

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>

namespace player {

inline void init_logger(
    const std::string &name = "player",
    const std::string &log_dir = "log"
)
{
    std::vector<spdlog::sink_ptr> sinks;

    // 控制台输出 (带颜色)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    sinks.push_back(console_sink);

    // 日志文件输出 (按天滚动，最多保留7天)
    try {
        std::string log_path = log_dir + "/" + name + ".log";
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            log_path,
            0, // 每天 0 点滚动
            0, // 0 分钟
            false,
            7 // 最多保留 7 天
        );
        file_sink->set_level(spdlog::level::debug);
        sinks.push_back(file_sink);
    } catch (const spdlog::spdlog_ex &ex) {
        // 如果无法创建日志文件，仅使用控制台
        fmt::print(stderr, "无法创建日志文件: {}\n", ex.what());
    }

    // 创建 logger
    auto logger =
        std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);

    // 设置格式: [时间] [级别] [文件名:行号] [函数名] 消息
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%!] %v");

    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(1));
}

} // namespace player

// 便捷宏 - 使用 SPDLOG_* 系列宏可以自动捕获文件名、行号、函数名
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
