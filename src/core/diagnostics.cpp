#include "core/diagnostics.h"

#include "core/product_info.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace solace::diagnostics
{
namespace
{
constexpr std::uintmax_t maximum_log_size = 1024 * 1024;

std::string env(const char* name)
{
#if defined(_MSC_VER)
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || !buffer)
        return {};
    std::string result(buffer);
    std::free(buffer);
    return result;
#else
    const char* raw = std::getenv(name);
    return raw ? std::string(raw) : std::string{};
#endif
}

std::filesystem::path log_root()
{
#if defined(_WIN32)
    if (const std::string local = env("LOCALAPPDATA"); !local.empty())
        return std::filesystem::path(local) / product_info::name / "logs";
#elif defined(__APPLE__)
    if (const std::string home = env("HOME"); !home.empty())
        return std::filesystem::path(home) / "Library" / "Logs" / product_info::name;
#else
    if (const std::string state = env("XDG_STATE_HOME"); !state.empty())
        return std::filesystem::path(state) / product_info::name / "logs";
    if (const std::string home = env("HOME"); !home.empty())
        return std::filesystem::path(home) / ".local" / "state" / product_info::name / "logs";
#endif
    std::error_code error;
    return std::filesystem::temp_directory_path(error) / product_info::name / "logs";
}

std::filesystem::path log_path()
{
    static const std::filesystem::path path = []
    {
        const std::filesystem::path directory = log_root();
        std::error_code error;
        std::filesystem::create_directories(directory, error);

        std::filesystem::path current = directory / "solace.log";
        if (std::filesystem::file_size(current, error) > maximum_log_size && !error)
        {
            const std::filesystem::path previous = directory / "solace.log.1";
            std::filesystem::remove(previous, error);
            error.clear();
            std::filesystem::rename(current, previous, error);
        }
        return current;
    }();
    return path;
}

std::tm local_now()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm time{};
#if defined(_WIN32)
    localtime_s(&time, &now);
#else
    localtime_r(&now, &time);
#endif
    return time;
}

void debug_output(const char* text) noexcept
{
#if defined(_WIN32)
    ::OutputDebugStringA(text);
#else
    std::fputs(text, stderr);
#endif
}

void write(const char* level, std::string_view subsystem, std::string_view message,
           long system_code) noexcept
{
    try
    {
        const std::tm time = local_now();

        std::ostringstream line;
        line << std::setfill('0') << (time.tm_year + 1900) << '-' << std::setw(2)
             << (time.tm_mon + 1) << '-' << std::setw(2) << time.tm_mday << ' ' << std::setw(2)
             << time.tm_hour << ':' << std::setw(2) << time.tm_min << ':' << std::setw(2)
             << time.tm_sec << " [" << level << "] [" << subsystem << "] " << message;
        if (system_code != 0)
            line << " (0x" << std::hex << std::uppercase << static_cast<unsigned long>(system_code)
                 << ')';
        line << '\n';

        const std::string text = line.str();
        debug_output(text.c_str());

        static std::mutex mutex;
        const std::lock_guard lock(mutex);
        std::ofstream output(log_path(), std::ios::app | std::ios::binary);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
    catch (...)
    {
        debug_output("[Solace] diagnostics write failed.\n");
    }
}
} // namespace

void info(std::string_view subsystem, std::string_view message) noexcept
{
    write("info", subsystem, message, 0);
}

void warning(std::string_view subsystem, std::string_view message, long system_code) noexcept
{
    write("warning", subsystem, message, system_code);
}

void error(std::string_view subsystem, std::string_view message, long system_code) noexcept
{
    write("error", subsystem, message, system_code);
}
} // namespace solace::diagnostics
