# CoreLogic (Hanvi Core Engine)

`CoreLogic` is a high-performance, standalone C++23 translation and text alignment engine for Chinese-to-Vietnamese translation. It powers the Hanvi reader and CLI tools, and is designed to be easily embedded into custom frontends (e.g., Qt/QML, Flutter, native Android via JNI/NDK, or CLI/server daemons).

---

## 1. Key Characteristics

- **Zero Framework Dependencies**: No dependencies on Qt or OS-specific GUI toolkits. Standard C++23 only.
- **Embedded Storage**: Uses an embedded SQLite 3 database (`dict.db`) via SQLite's native C API with WAL mode.
- **Traditional-to-Simplified Support**: Bundles OpenCC for accurate conversion from Traditional to Simplified Chinese before processing.
- **Synchronized Multi-Role Alignment**: Provides token-level alignment between Chinese source text, Sino-Vietnamese readings (Hán Việt), and Vietnamese translations.
- **High Performance**: In-memory prefix Trie with custom TextArena block allocator; processes millions of characters in hundreds of milliseconds.

---

## 2. CMake Integration

Add `CoreLogic` to your target in `CMakeLists.txt`:

```cmake
target_link_libraries(YourTarget PRIVATE CoreLogic)
target_include_directories(YourTarget PRIVATE "${HANVI_REPO_ROOT}")
```

Your compiler must support **C++23** (Clang 17+, GCC 13+, or MSVC 19.38+). Include paths are rooted at the repository directory:

```cpp
#include "core/converter.h"
#include "core/dict.h"
#include "core/aligned_document.h"
#include "core/io.h"
#include "core/unicode_utils.h"
```

---

## 3. Required Runtime Assets

Any application running `CoreLogic` must provide access to two asset groups:

1. **`dict.db`**:
   The SQLite database operates with `PRAGMA journal_mode = WAL; PRAGMA foreign_keys = ON; PRAGMA synchronous = NORMAL;` and contains 7 core tables:
   - `phrases`: `(original TEXT PRIMARY KEY, translated TEXT) WITHOUT ROWID` — Multi-word dictionary definitions (`\x1F` / ASCII Unit Separator `0x1F` separated for multiple ordered meanings, where the first meaning is the default).
   - `names`: `(original TEXT PRIMARY KEY, translated TEXT) WITHOUT ROWID` — Proper noun dictionary definitions.
   - `sv_readings`: `(original CHAR(1) PRIMARY KEY, translated TEXT) WITHOUT ROWID` — Character-by-character Sino-Vietnamese readings (Hán Việt). Loaded in memory by `char32_t` code points (supporting BMP and surrogate pairs).
   - `punctuations`: `(original CHAR(1) PRIMARY KEY, normalized CHAR(1)) WITHOUT ROWID` — Mapping Chinese full-width punctuations (e.g., `，`, `。`, `！`, `？`, `：`) to standard Latin equivalents.
   - `grammar_rules`: `(original_start, original_end, translated_start, translated_end, PRIMARY KEY(original_start, original_end)) WITHOUT ROWID` — Multi-part grammatical transformation rules (handling split patterns like `被...` or `把...`).
   - `name_sets`: `(id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT UNIQUE NOT NULL COLLATE NOCASE)` — User-defined per-novel character name sets.
   - `name_set_entries`: `(original TEXT NOT NULL, translated TEXT NOT NULL, set_id INTEGER, PRIMARY KEY(set_id, original), FOREIGN KEY(set_id) REFERENCES name_sets(id) ON DELETE CASCADE) WITHOUT ROWID` — Book-specific name translations associated with a `name_sets.id`.
   - `variants`: `(original TEXT PRIMARY KEY, canonical TEXT NOT NULL) WITHOUT ROWID` — Mapping variant, archaic, or non-standard Chinese characters to canonical Simplified equivalents. Used during text normalization and as a fallback lookup for Sino-Vietnamese readings.

2. **OpenCC Assets** (`t2s.json` and `*.ocd2` dictionaries):
   - `t2s.json` (OpenCC configuration file)
   - `TSPhrases.ocd2`
   - `TSCharacters.ocd2`
   - `TSCharactersExt.ocd2`
   - `CJK_Compatibility_Ideographs.ocd2`

   *Lookup strategy*:
   - Can be explicitly specified at runtime via `hanvi::init_opencc("path/to/t2s.json")` (recommended for mobile/Android internal app storage).
   - If unconfigured, automatically searches:
     1. Relative paths: `./opencc/t2s.json` or `./t2s.json` (relative to current working directory).
     2. Executable directory: `<exe_dir>/opencc/t2s.json` or `<exe_dir>/t2s.json` (on Windows).

---

## 4. Architecture & String Encodings

- **Internal Encoding (UTF-16)**:
  `CoreLogic` operates internally on `std::u16string` and `std::u16string_view`. Because BMP characters (standard Chinese and Latin characters) occupy exactly one 16-bit code unit, this avoids multi-byte scanning overhead.
- **External UTF-8 Convenience APIs**:
  High-level UTF-8 wrappers (`convert_plain_utf8`, `normalize_text_utf8`, `to_simplified_chinese_utf8`) are provided for straightforward JNI and CLI integration.
- **`StringView` Null vs. Empty Semantics**:
  Dictionary translations can be valid but **empty** (e.g. grammatical particle `的` translating to `""` to omit it in certain contexts).
  - `view.isNull()` $\rightarrow$ Checks `data() == nullptr` (entry does not exist).
  - `view.empty()` / `view.isEmpty()` $\rightarrow$ Checks `size() == 0` (entry exists, but translation is empty string).
  - `view.split(char16_t sep)` $\rightarrow$ Utility returning `std::vector<StringView>` slices (e.g. `view.split(u'\x1F')` to unpack multi-meaning phrases without copying string memory).

---

## 5. API Reference & Code Examples

### A. Lifecycle & Dictionary Management (`core/dict.h`)

All dictionary queries access in-memory data structures loaded from `dict.db`.

```cpp
#include "core/dict.h"

// 1. Initialize and load dictionaries (blocking / synchronous)
hanvi::load_dict("path/to/dict.db");

// (Optional) Explicitly configure OpenCC t2s.json path (e.g. for Android internal storage)
// If not called, automatically searches standard relative and executable paths.
hanvi::init_opencc("path/to/opencc/t2s.json");

// Or asynchronous with a callback:
hanvi::load_dict("path/to/dict.db", []() {
    // Called when background worker threads finish loading Trie & maps
});

// 2. Select an active novel nameset (pass -1 to disable)
hanvi::load_name_set(active_nameset_id);

// 3. Inspect available novel namesets
for (const auto& [id, title] : hanvi::name_sets) {
    // id: int
    // title: std::u16string
}

// 4. Reload dictionaries (e.g. after mass external updates)
hanvi::reload_dict([]() {
    // Refresh complete
});

// 5. Shutdown and release SQLite connection
hanvi::close_db();
```

---

### B. Plain Text Conversion (`core/converter.h`)

Ideal for bulk conversion, chapter exports, and background translation jobs.

```cpp
#include "core/converter.h"

// --- UTF-8 API (convenient for JNI / CLI) ---
std::string raw_chinese = "伴随着房门突然被推开的响动...";

// Step 1: Normalize (converts Traditional -> Simplified Chinese & fixes full-width punctuation)
std::string normalized = hanvi::normalize_text_utf8(raw_chinese);

// Step 2: Convert to Vietnamese plain text
// (optional progress callback triggered every 2500 processed characters)
std::string vietnamese = hanvi::convert_plain_utf8(normalized, [](int chars_done) {
    // update progress bar
});

// --- UTF-16 API (zero conversion overhead) ---
std::u16string u16_raw = u"伴随着房门突然被推开的响动...";
std::u16string u16_norm = hanvi::normalize_text(u16_raw);
std::u16string u16_vn   = hanvi::convert_plain(u16_norm, nullptr);
```

---

### C. Interactive / Token-Aligned Conversion (`core/aligned_document.h`)

Ideal for interactive reader frontends where tapping on a Vietnamese or Chinese word highlights its corresponding translation and allows looking up alternative meanings.

```cpp
#include "core/converter.h"
#include "core/aligned_document.h"

// Produces a tokenized, aligned document
std::shared_ptr<hanvi::AlignedDocument> doc = hanvi::convert(u16_normalized_text);

// Traverse paragraphs and aligned tokens
for (size_t p = 0; p < doc->paragraphs.size(); ++p) {
    const auto& paragraph = doc->paragraphs[p];
    for (size_t t = 0; t < paragraph.tokens.size(); ++t) {
        const hanvi::Token& tok = paragraph.tokens[t];

        uint32_t id    = tok.id;                  // Unique sequential token ID (1-based)
        const auto& cn = tok.cn;                  // Original Chinese word/phrase
        const auto& sv = tok.sv;                  // Sino-Vietnamese reading (Hán Việt)
        const auto& vn = tok.vn;                  // Vietnamese translated meaning
        bool is_rule   = tok.is_rule();           // Matched a grammatical rule?
        bool space     = tok.has_trailing_space;  // Recommended trailing space
    }
}

// Token Lookups & Text Ranges:
// 1. Find token by its ID:
const hanvi::Token* tok = doc->find_token(token_id);

// 2. Find paragraph/token index position:
std::optional<hanvi::DocumentPosition> pos = doc->find_token_position(token_id);

// 3. Extract formatted text for a selected range:
hanvi::DocumentSelection selection{
    .start = { .paragraph = 0, .token = 0 },
    .end   = { .paragraph = 0, .token = 10 }
};

std::u16string vn_text = doc->get_text_range(selection, hanvi::LanguageRole::Vietnamese);
std::u16string cn_text = doc->get_text_range(selection, hanvi::LanguageRole::Chinese);
std::u16string sv_text = doc->get_text_range(selection, hanvi::LanguageRole::SinoVietnamese);
```

---

### D. Dictionary Editing & Persistent CRUD (`core/io.h` vs. `core/db.h`)

- **`core/io.h` (`io_*`)**: High-level transactional CRUD that updates **both** the in-memory Trie/maps and writes the change to `dict.db`. Always use this for interactive editing.
- **`core/db.h` (`db_*`)**: Low-level database operations that write directly to `dict.db` without altering in-memory state (useful for offline migrations or CLI batch import tools).

```cpp
#include "core/io.h"

// 1. Add or update a phrase or name:
// - id: -1 for global dictionary, or positive ID for a book-specific nameset
// - priority: hanvi::Priority::PHRASE or hanvi::Priority::NAME
hanvi::io_insert(
    /*id=*/ -1,
    /*key=*/ u"林恩",
    /*value=*/ u"Lâm Ân",
    /*priority=*/ hanvi::Priority::NAME
);

// 2. Reorder translations for a key (the first element becomes the primary meaning):
hanvi::io_reorder(u"的", { u"", u"như thế", u"đích" });

// 3. Remove a specific meaning from an entry:
hanvi::io_remove_meaning(u"测试", u"thử nghiệm");

// 4. Remove an entire entry:
hanvi::io_remove(/*id=*/ -1, u"林恩", hanvi::Priority::NAME);

// 5. Add or update character variant mapping:
hanvi::io_insert_variant(u"羣", u"群");

// 6. Remove a variant mapping:
hanvi::io_remove_variant(u"羣");
```

---

### E. Text Pagination (`core/io.h`)

Splits large books or chapter files into manageable page chunks at sentence/newline boundaries:

```cpp
#include "core/io.h"

// Pages input into slices of at least min_length (e.g. 5000 characters)
std::vector<std::u16string_view> pages = hanvi::paginate(u16_novel_text, /*min_length=*/ 5000);

for (std::u16string_view page : pages) {
    // Convert and render page individually
}
```

---

### F. Unicode Utilities (`core/unicode_utils.h`)

Pure C++ utilities for UTF conversions and character classification:

```cpp
#include "core/unicode_utils.h"

// Encodings:
std::u16string u16 = hanvi::to_utf16(utf8_string_view);
std::string   u8   = hanvi::to_utf8(u16_string_view);

// Character classification:
bool is_letter_or_num = hanvi::is_latin_or_digit(u'A');
bool is_quote_opener  = hanvi::is_opener_char(u'“');
bool is_quote_closer  = hanvi::is_closer_char(u'”');

// Case conversion & checks:
char16_t upper = hanvi::to_upper_char16(u'đ'); // Returns u'Đ'
bool is_lower  = hanvi::is_lower_char16(u'â'); // Returns true
```

---

## 6. Android NDK / JNI Implementation Notes

When porting to Android:
1. **Asset Extraction**: On first launch, copy `dict.db` and the `opencc/` asset folder from Android APK `assets` into the app's internal storage (`context.getFilesDir().getAbsolutePath()`).
2. **JNI Wrappers**: Expose JNI methods matching your UI needs:
   - `nativeInit(String dbPath)` $\rightarrow$ calls `hanvi::load_dict(dbPath)`.
   - `nativeConvertPlain(String text)` $\rightarrow$ calls `hanvi::convert_plain_utf8(hanvi::normalize_text_utf8(text))`.
   - `nativeConvertAligned(String text)` $\rightarrow$ calls `hanvi::convert(...)` and serializes tokens into a flat primitive array or Java/Kotlin data classes.
3. **Threading**:
   - `convert_plain` and `convert` are stateless and reentrant for read operations. They can be safely called from background coroutines/threads.
   - Dictionary mutation functions (`io_insert`, `io_remove`) use SQLite transactions and update shared memory state; avoid calling mutations concurrently from multiple threads without external synchronization.
4. **CMake Cross-Compilation**:
   - `core/CMakeLists.txt` automatically sets `HANVI_BUILD_OPENCC_DICTIONARIES=OFF` when cross-compiling (`ANDROID` or `CMAKE_CROSSCOMPILING`). This avoids attempting to run target-compiled binary dictionary tools on the host machine. Pre-built `.ocd2` files should be placed directly in the app's `assets/opencc/` bundle.

