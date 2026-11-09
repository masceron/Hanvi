#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <cstdint>

namespace hanvi {

inline bool is_high_surrogate(const char16_t c) noexcept {
    return c >= 0xD800 && c <= 0xDBFF;
}

inline bool is_low_surrogate(const char16_t c) noexcept {
    return c >= 0xDC00 && c <= 0xDFFF;
}

inline char32_t to_code_point(const char16_t high, const char16_t low) noexcept {
    return 0x10000 + (((static_cast<char32_t>(high) & 0x3FF) << 10) | (static_cast<char32_t>(low) & 0x3FF));
}

inline char32_t utf16_code_point_at(const std::u16string_view str, const size_t idx) noexcept {
    if (idx >= str.size()) return 0;
    const char16_t c = str[idx];
    if (is_high_surrogate(c) && idx + 1 < str.size() && is_low_surrogate(str[idx + 1])) {
        return to_code_point(c, str[idx + 1]);
    }
    return c;
}

// Compares UTF-16 strings in code point order, which is how SQLite sorts TEXT (by its UTF-8
// bytes): surrogates, which code points above U+FFFF use, sort after U+E000-U+FFFF.
inline int compare_code_points(const std::u16string_view a, const std::u16string_view b) noexcept {
    const auto weight = [](const char16_t c) -> uint32_t {
        return c < 0xD800 ? c : c < 0xE000 ? c + 0x2000u : c - 0x800u;
    };
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return weight(a[i]) < weight(b[i]) ? -1 : 1;
    }
    return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}

inline bool is_closer_char(const char16_t c) noexcept {
    return c == u'.' || c == u',' || c == u'!' || c == u'?' ||
           c == u':' || c == u';' || c == u'…' ||
           c == u')' || c == u']' || c == u'}' || c == u'>' ||
           c == u'”' || c == u'’';
}

inline bool is_opener_char(const char16_t c) noexcept {
    return c == u'(' || c == u'[' || c == u'{' || c == u'<' ||
           c == u'“' || c == u'‘';
}

inline bool is_digit(const char16_t c) noexcept {
    return (c >= u'0' && c <= u'9') || (c >= 0xFF10 && c <= 0xFF19);
}

inline bool is_latin(const char16_t c) noexcept {
    if ((c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z')) return true;
    if ((c >= 0xFF21 && c <= 0xFF3A) || (c >= 0xFF41 && c <= 0xFF5A)) return true;
    if ((c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x00FF)) return true;
    if (c >= 0x0100 && c <= 0x024F) return true;
    if (c >= 0x1EA0 && c <= 0x1EF9) return true;
    if ((c >= 0x2C60 && c <= 0x2C7F) || (c >= 0xA720 && c <= 0xA7FF) || (c >= 0xAB30 && c <= 0xAB6F)) return true;
    return false;
}

inline bool is_latin_or_digit(const char16_t c) noexcept {
    return is_digit(c) || is_latin(c);
}

inline char16_t to_upper_char16(const char16_t c) noexcept {
    if (c >= u'a' && c <= u'z') {
        return static_cast<char16_t>(c - u'a' + u'A');
    }
    if ((c >= 0x00E0 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x00FE)) {
        return static_cast<char16_t>(c - 0x0020);
    }
    if (c >= 0x1EA0 && c <= 0x1EF9) {
        if (c % 2 == 1) return static_cast<char16_t>(c - 1);
        return c;
    }
    if (c >= 0x0100 && c <= 0x017F) {
        if (c % 2 == 1) return static_cast<char16_t>(c - 1);
        return c;
    }
    if (c == 0x01A1) return 0x01A0; // ơ -> Ơ
    if (c == 0x01B0) return 0x01AF; // ư -> Ư
    if (c >= 0xFF41 && c <= 0xFF5A) {
        return static_cast<char16_t>(c - 0x0020);
    }
    return c;
}

inline bool is_lower_char16(const char16_t c) noexcept {
    return to_upper_char16(c) != c;
}

// Most bytes UTF-8 needs for a UTF-16 string: three per unit (a surrogate pair takes four for two).
constexpr size_t max_utf8_size(const size_t u16_size) noexcept {
    return u16_size * 3;
}

// Writes u16 as UTF-8 to out, which has room for max_utf8_size(u16.size()) bytes, and returns
// the end. Unpaired surrogates are kept as three-byte sequences, so decoding gives them back.
inline char* encode_utf8(const std::u16string_view u16, char* out) noexcept {
    for (size_t i = 0; i < u16.size(); ++i) {
        char32_t cp = u16[i];
        if (is_high_surrogate(static_cast<char16_t>(cp)) && i + 1 < u16.size() && is_low_surrogate(u16[i + 1])) {
            cp = to_code_point(static_cast<char16_t>(cp), u16[i + 1]);
            ++i;
        }
        if (cp <= 0x7F) {
            *out++ = static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            *out++ = static_cast<char>(0xC0 | (cp >> 6));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            *out++ = static_cast<char>(0xE0 | (cp >> 12));
            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            *out++ = static_cast<char>(0xF0 | (cp >> 18));
            *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// Writes what encode_utf8 wrote back as UTF-16 to out, which has room for u8.size() units, and
// returns the end. A sequence cut short at the end is dropped.
inline char16_t* decode_utf8(const std::string_view u8, char16_t* out) noexcept {
    const auto* p = reinterpret_cast<const unsigned char*>(u8.data());
    const auto* const end = p + u8.size();
    while (p < end) {
        const char32_t c = *p;
        if (c < 0x80) {
            *out++ = static_cast<char16_t>(c);
            ++p;
        } else if (c < 0xE0) {
            if (end - p < 2) break;
            *out++ = static_cast<char16_t>((c & 0x1F) << 6 | (p[1] & 0x3F));
            p += 2;
        } else if (c < 0xF0) {
            if (end - p < 3) break;
            *out++ = static_cast<char16_t>((c & 0x0F) << 12 | (p[1] & 0x3F) << 6 | (p[2] & 0x3F));
            p += 3;
        } else {
            if (end - p < 4) break;
            const char32_t cp = ((c & 0x07) << 18 | (p[1] & 0x3F) << 12 | (p[2] & 0x3F) << 6 | (p[3] & 0x3F)) - 0x10000;
            *out++ = static_cast<char16_t>(0xD800 + (cp >> 10));
            *out++ = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
            p += 4;
        }
    }
    return out;
}

inline std::string to_utf8(const std::u16string_view u16) {
    std::string out(max_utf8_size(u16.size()), '\0');
    out.resize(encode_utf8(u16, out.data()) - out.data());
    return out;
}

inline std::u16string to_utf16(const std::string_view u8) {
    std::u16string out;
    out.reserve(u8.size());
    size_t i = 0;
    while (i < u8.size()) {
        const auto c = static_cast<uint8_t>(u8[i++]);
        char32_t cp;
        int extra = 0;
        if (c <= 0x7F) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            continue;
        }
        while (extra-- > 0 && i < u8.size()) {
            const auto next = static_cast<uint8_t>(u8[i++]);
            if ((next & 0xC0) != 0x80) break;
            cp = (cp << 6) | (next & 0x3F);
        }
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

} // namespace hanvi

