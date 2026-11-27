#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include "aligned_document.h"

namespace hanvi {
    // How a line is cut into words.
    // - Greedy takes the longest entry from the left, with a one-word lookahead for names and longer
    //   overlapping entries.
    // - Frequency picks the likeliest cut of the whole line, from each entry's segment weight, with
    //   fewer words preferred; name set entries always win. A dictionary without segment_weights
    //   converts with Greedy (see frequency_segmentation_available()).
    // Each conversion reads it once, when it starts.
    enum class Segmentation { Greedy, Frequency };

    inline std::atomic<Segmentation> segmentation{Segmentation::Greedy};

    // Whether the loaded dictionary has the segment weights Frequency segmentation needs.
    bool frequency_segmentation_available();

    void init_opencc(const std::string &config_path);

    // Overrides to apply during a conversion. `entries` need not be sorted; offsets are into the full
    // text, and `text_offset` is where the converted input starts in it (a page's start, else 0).
    // Rule tokens (被...) are never overridden.
    struct OverrideSet {
        std::span<const TokenOverride> entries;
        size_t text_offset = 0;
    };

    // Token offsets are reported relative to the full text, i.e. shifted by overrides.text_offset.
    std::shared_ptr<AlignedDocument> convert(std::u16string_view input,
                                             const std::function<void(int)> &progress_callback = nullptr,
                                             const OverrideSet &overrides = {});

    std::u16string convert_plain(std::u16string_view input,
                                 const std::function<void(int)> &progress_callback = nullptr,
                                 const OverrideSet &overrides = {});

    std::u16string to_simplified_chinese(std::u16string_view input);

    std::u16string canonicalize_variants(std::u16string_view input);

    std::u16string normalize_text(std::u16string_view input);

    // UTF-8 helpers
    std::string convert_plain_utf8(std::string_view input,
                                   const std::function<void(int)> &progress_callback = nullptr);

    std::string to_simplified_chinese_utf8(std::string_view input);

    std::string normalize_text_utf8(std::string_view input);
} // namespace hanvi

using hanvi::Segmentation;
using hanvi::frequency_segmentation_available;
using hanvi::OverrideSet;
using hanvi::init_opencc;
using hanvi::convert;
using hanvi::convert_plain;
using hanvi::to_simplified_chinese;
using hanvi::canonicalize_variants;
using hanvi::normalize_text;
