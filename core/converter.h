#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include "aligned_document.h"

namespace hanvi {

void init_opencc(const std::string& config_path);

std::shared_ptr<AlignedDocument> convert(std::u16string_view input,
                                         const std::function<void(int)>& progress_callback = nullptr);
std::u16string convert_plain(std::u16string_view input,
                             const std::function<void(int)>& progress_callback = nullptr);

std::u16string to_simplified_chinese(std::u16string_view input);
std::u16string canonicalize_variants(std::u16string_view input);
std::u16string normalize_text(std::u16string_view input);

// UTF-8 helpers
std::string convert_plain_utf8(std::string_view input,
                               const std::function<void(int)>& progress_callback = nullptr);
std::string to_simplified_chinese_utf8(std::string_view input);
std::string normalize_text_utf8(std::string_view input);

} // namespace hanvi

using hanvi::convert;
using hanvi::convert_plain;
using hanvi::to_simplified_chinese;
using hanvi::canonicalize_variants;
using hanvi::normalize_text;
