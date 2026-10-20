#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace hanvi {

inline bool is_high_surrogate(char16_t c) noexcept {
    return c >= 0xD800 && c <= 0xDBFF;
}

inline bool is_low_surrogate(char16_t c) noexcept {
    return c >= 0xDC00 && c <= 0xDFFF;
}

inline bool is_closer_char(char16_t c) noexcept {
    return c == u'.' || c == u',' || c == u'!' || c == u'?' ||
           c == u':' || c == u';' || c == u'…' ||
           c == u')' || c == u']' || c == u'}' || c == u'>' ||
           c == u'”' || c == u'’';
}

inline bool is_opener_char(char16_t c) noexcept {
    return c == u'(' || c == u'[' || c == u'{' || c == u'<' ||
           c == u'“' || c == u'‘';
}

inline bool is_digit(char16_t c) noexcept {
    return (c >= u'0' && c <= u'9') || (c >= 0xFF10 && c <= 0xFF19);
}

inline bool is_latin(char16_t c) noexcept {
    if ((c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z')) return true;
    if ((c >= 0xFF21 && c <= 0xFF3A) || (c >= 0xFF41 && c <= 0xFF5A)) return true;
    if ((c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x00FF)) return true;
    if (c >= 0x0100 && c <= 0x024F) return true;
    if (c >= 0x1EA0 && c <= 0x1EF9) return true;
    if ((c >= 0x2C60 && c <= 0x2C7F) || (c >= 0xA720 && c <= 0xA7FF) || (c >= 0xAB30 && c <= 0xAB6F)) return true;
    return false;
}

inline bool is_latin_or_digit(char16_t c) noexcept {
    return is_digit(c) || is_latin(c);
}

inline char16_t to_upper_char16(char16_t c) noexcept {
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

inline bool is_lower_char16(char16_t c) noexcept {
    return to_upper_char16(c) != c;
}

inline std::string to_utf8(std::u16string_view u16) {
    std::string out;
    out.reserve(u16.size() * 3 / 2);
    for (size_t i = 0; i < u16.size(); ++i) {
        char32_t cp = u16[i];
        if (is_high_surrogate(static_cast<char16_t>(cp)) && i + 1 < u16.size() && is_low_surrogate(u16[i + 1])) {
            cp = 0x10000 + (((cp & 0x3FF) << 10) | (u16[i + 1] & 0x3FF));
            ++i;
        }
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

inline std::u16string to_utf16(std::string_view u8) {
    std::u16string out;
    out.reserve(u8.size());
    size_t i = 0;
    while (i < u8.size()) {
        const uint8_t c = static_cast<uint8_t>(u8[i++]);
        char32_t cp = 0;
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
            const uint8_t next = static_cast<uint8_t>(u8[i++]);
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

