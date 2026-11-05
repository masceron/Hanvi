// Times the conversion pipeline the apps run (load, normalize, convert_plain, convert, teardown)
// over a set of texts, single-threaded, to compare compilers and compile options. The output
// hash must match between builds: a different hash means a build converts differently.
//
//   HanviBench [--rounds N] [--dict dict.db] [--opencc t2s.json] <file or folder>...
//
// Folders are searched recursively for .txt files. Texts must be UTF-8.
//
// The last line, for collecting runs, is
//   csv,compiler,load ms,load MB,normalize ms,convert_plain ms,convert ms,teardown ms,peak MB,M units,hash
// with the median of the rounds for normalize and the conversions.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <print>
#include <span>
#include <string>
#include <vector>

#include "core/converter.h"
#include "core/dict.h"
#include "core/unicode_utils.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace
{

struct Memory
{
    double current_mb = 0;
    double peak_mb = 0;
};

// Resident memory (working set on Windows) of this process; zero where unknown.
Memory process_memory()
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    {
        return {static_cast<double>(counters.WorkingSetSize) / 1048576.0,
                static_cast<double>(counters.PeakWorkingSetSize) / 1048576.0};
    }
    return {};
#elif defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
    {
        return {static_cast<double>(info.resident_size) / 1048576.0,
                static_cast<double>(info.resident_size_max) / 1048576.0};
    }
    return {};
#else
    Memory memory;
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);)
    {
        if (line.starts_with("VmRSS:")) memory.current_mb = std::stod(line.substr(6)) / 1024.0;
        if (line.starts_with("VmHWM:")) memory.peak_mb = std::stod(line.substr(6)) / 1024.0;
    }
    return memory;
#endif
}

std::string compiler_name()
{
#if defined(__clang__)
    return std::format("Clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_FULL_VER);
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#else
    return "unknown compiler";
#endif
}

double ms_since(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double median(std::vector<double> values)
{
    std::ranges::sort(values);
    const size_t mid = values.size() / 2;
    return values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2;
}

std::string display(const fs::path& path)
{
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}

bool is_utf8(const std::string_view text)
{
    size_t i = 0;
    while (i < text.size())
    {
        const auto c = static_cast<unsigned char>(text[i]);
        const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (len == 0 || i + len > text.size()) return false;
        for (size_t k = 1; k < len; ++k)
        {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
        }
        i += len;
    }
    return true;
}

// FNV-1a over everything the conversions produced.
struct Hash
{
    uint64_t value = 14695981039346656037ull;

    void add(const std::u16string_view text)
    {
        for (const char16_t c : text)
        {
            value = (value ^ (c & 0xFF)) * 1099511628211ull;
            value = (value ^ (c >> 8)) * 1099511628211ull;
        }
        value = (value ^ 0xFFFF) * 1099511628211ull;
    }
};

struct Text
{
    fs::path path;
    std::u16string content;
};

std::vector<Text> read_texts(const std::span<const fs::path> inputs)
{
    std::vector<fs::path> files;
    for (const auto& input : inputs)
    {
        if (fs::is_directory(input))
        {
            for (const auto& entry : fs::recursive_directory_iterator(input))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") files.push_back(entry.path());
            }
        }
        else
        {
            files.push_back(input);
        }
    }
    std::ranges::sort(files);

    std::vector<Text> texts;
    for (const auto& file : files)
    {
        std::ifstream in(file, std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        if (!in.good() && !in.eof())
        {
            std::println(stderr, "Skipped {}: could not read it.", display(file));
            continue;
        }
        std::string_view utf8 = bytes;
        if (utf8.starts_with("\xEF\xBB\xBF")) utf8.remove_prefix(3);
        if (!is_utf8(utf8))
        {
            std::println(stderr, "Skipped {}: not UTF-8 (GBK or Big5? Convert it first).", display(file));
            continue;
        }
        texts.push_back({file, hanvi::to_utf16(utf8)});
    }
    return texts;
}

int usage()
{
    std::println(stderr, "Usage: HanviBench [--rounds N] [--dict dict.db] [--opencc t2s.json] <file or folder>...");
    return 2;
}

} // namespace

int main(int argc, char* argv[])
{
    const fs::path exe_dir = fs::absolute(fs::path(argv[0])).parent_path();
    fs::path dict_path = exe_dir / "dict.db";
    fs::path opencc_path = exe_dir / "opencc" / "t2s.json";
    int rounds = 5;
    std::vector<fs::path> inputs;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if ((arg == "--rounds" || arg == "--dict" || arg == "--opencc") && i + 1 < argc)
        {
            const char* value = argv[++i];
            if (arg == "--rounds") rounds = std::max(1, std::atoi(value));
            else if (arg == "--dict") dict_path = value;
            else opencc_path = value;
        }
        else if (arg.starts_with("--"))
        {
            return usage();
        }
        else
        {
            inputs.emplace_back(arg);
        }
    }
    if (inputs.empty()) return usage();
    if (!fs::exists(dict_path) || !fs::exists(opencc_path))
    {
        std::println(stderr, "Missing {} or {}.", display(dict_path), display(opencc_path));
        return 1;
    }

    const std::vector<Text> texts = read_texts(inputs);
    if (texts.empty())
    {
        std::println(stderr, "No texts to convert.");
        return 1;
    }
    size_t total_chars = 0;
    for (const auto& text : texts) total_chars += text.content.size();

    std::println("{}, {}-bit, {} text(s), {:.2f} M UTF-16 units, {} round(s)", compiler_name(),
                 sizeof(void*) * 8, texts.size(), static_cast<double>(total_chars) / 1e6, rounds);

    const Memory before_load = process_memory();
    auto start = Clock::now();
    hanvi::init_opencc(display(opencc_path));
    hanvi::load_dict(display(dict_path));
    const double load_ms = ms_since(start);
    const Memory after_load = process_memory();

    std::vector<std::u16string> normalized(texts.size());
    std::vector<double> normalize_ms, plain_ms, convert_ms;
    Hash hash;
    for (int round = 0; round < rounds; ++round)
    {
        start = Clock::now();
        for (size_t t = 0; t < texts.size(); ++t) normalized[t] = hanvi::normalize_text(texts[t].content);
        normalize_ms.push_back(ms_since(start));

        start = Clock::now();
        for (const auto& text : normalized)
        {
            const std::u16string plain = hanvi::convert_plain(text);
            if (round == 0) hash.add(plain);
        }
        plain_ms.push_back(ms_since(start));

        start = Clock::now();
        for (const auto& text : normalized)
        {
            const auto document = hanvi::convert(text);
            if (round != 0) continue;
            for (const auto& paragraph : document->paragraphs)
            {
                for (const auto& token : paragraph.tokens)
                {
                    hash.add(token.cn);
                    hash.add(token.sv);
                    hash.add(token.vn);
                }
            }
        }
        convert_ms.push_back(ms_since(start));
    }
    const Memory after_convert = process_memory();

    start = Clock::now();
    hanvi::dictionary = hanvi::Dictionary();
    const double teardown_ms = ms_since(start);

    const double mchars = static_cast<double>(total_chars) / 1e6;
    const auto report = [&](const char* name, const std::vector<double>& ms)
    {
        const double med = median(ms);
        std::println("{:<14} {:9.1f} ms   min {:.1f} ms, {:.2f} M units/s", name, med,
                     *std::ranges::min_element(ms), mchars / (med / 1000.0));
    };
    std::println("{:<14} {:9.1f} ms   memory +{:.1f} MB, resident {:.1f} MB", "load", load_ms,
                 after_load.current_mb - before_load.current_mb, after_load.current_mb);
    report("normalize", normalize_ms);
    report("convert_plain", plain_ms);
    report("convert", convert_ms);
    std::println("{:<14} {:9.1f} ms", "teardown", teardown_ms);
    std::println("{:<14} {:9.1f} MB", "peak memory", after_convert.peak_mb);
    std::println("{:<14} {:016x}", "output hash", hash.value);
    // One line per run, for collecting results.
    std::println("csv,{},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{:.1f},{:016x}", compiler_name(), load_ms,
                 after_load.current_mb - before_load.current_mb, median(normalize_ms), median(plain_ms),
                 median(convert_ms), teardown_ms, after_convert.peak_mb, mchars, hash.value);
    return 0;
}
