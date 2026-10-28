#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>

namespace hanvi {

enum Priority { NONE, PHRASE, NAME };

struct Rule
{
    std::u16string original_start;
    std::u16string original_end;
    std::u16string translation_start;
    std::u16string translation_end;
};

struct StringView : public std::u16string_view
{
    constexpr StringView() noexcept = default;
    constexpr StringView(const char16_t* str, const size_t len) noexcept : std::u16string_view(str, len) {}
    constexpr StringView(const std::u16string_view sv) noexcept : std::u16string_view(sv) {}

    [[nodiscard]] bool isNull() const noexcept { return data() == nullptr; }
    [[nodiscard]] bool isEmpty() const noexcept { return empty(); }

    [[nodiscard]] std::vector<StringView> split(const char16_t sep) const {
        std::vector<StringView> parts;
        size_t start = 0;
        while (start < size()) {
            size_t pos = find(sep, start);
            if (pos == npos) {
                parts.emplace_back(substr(start));
                break;
            }
            parts.emplace_back(substr(start, pos - start));
            start = pos + 1;
        }
        return parts;
    }
};

struct TrieNode;

class NodePool
{
public:
    NodePool() = default;

    NodePool(NodePool&& other) noexcept
        : blocks(std::move(other.blocks)),
          current_block_offset(other.current_block_offset),
          current_block_ptr(other.current_block_ptr)
    {
        other.current_block_offset = BLOCK_SIZE;
        other.current_block_ptr = nullptr;
    }

    NodePool& operator=(NodePool&& other) noexcept
    {
        if (this != &other)
        {
            blocks = std::move(other.blocks);
            current_block_offset = other.current_block_offset;
            current_block_ptr = other.current_block_ptr;
            other.current_block_offset = BLOCK_SIZE;
            other.current_block_ptr = nullptr;
        }
        return *this;
    }

    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;

    TrieNode* allocate();
    void clear();
    ~NodePool();

private:
    static constexpr size_t BLOCK_SIZE = 65520;
    std::vector<std::unique_ptr<char[]>> blocks;
    size_t current_block_offset = BLOCK_SIZE;
    char* current_block_ptr = nullptr;
};

class TextArena
{
public:
    TextArena() = default;
    ~TextArena() = default;

    TextArena(TextArena&& other) noexcept
        : blocks(std::move(other.blocks)),
          current_block_offset(other.current_block_offset),
          current_block_ptr(other.current_block_ptr)
    {
        other.current_block_offset = BLOCK_SIZE;
        other.current_block_ptr = nullptr;
    }

    TextArena& operator=(TextArena&& other) noexcept
    {
        if (this != &other)
        {
            blocks = std::move(other.blocks);
            current_block_offset = other.current_block_offset;
            current_block_ptr = other.current_block_ptr;
            other.current_block_offset = BLOCK_SIZE;
            other.current_block_ptr = nullptr;
        }
        return *this;
    }

    TextArena(const TextArena&) = delete;
    TextArena& operator=(const TextArena&) = delete;

    const char16_t* allocate(std::u16string_view str);
    void clear();

private:
    static constexpr size_t BLOCK_SIZE = 2 * 1024 * 1024;
    std::vector<std::unique_ptr<char16_t[]>> blocks;
    size_t current_block_offset = BLOCK_SIZE;
    char16_t* current_block_ptr = nullptr;
};

struct TrieNode
{
    uintptr_t data = 0;
    TrieNode* first_child = nullptr;
    char16_t single_child_char = 0;
    uint16_t child_count = 0;
    uint32_t reserved = 0;

    TrieNode() = default;

    TrieNode(const TrieNode&) = delete;
    TrieNode& operator=(const TrieNode&) = delete;

    ~TrieNode();

    [[nodiscard]] TrieNode* find_child(char16_t ch) const;
    void add_child(char16_t ch, TrieNode* node);

    [[nodiscard]] StringView get_name() const;
    [[nodiscard]] StringView get_first_phrase() const;
    [[nodiscard]] StringView get_full_phrase() const;
    [[nodiscard]] std::vector<Rule>* get_rules() const;

    void set_name_ptr(const char16_t* ptr);
    void set_phrase_ptr(const char16_t* ptr);
    void add_rule(const Rule& rule);

    void remove_name();
    void remove_phrases();

private:
    void ensure_complex();
    void free_data();
};

struct ExactResult
{
    StringView name;
    StringView full_phrase;

    [[nodiscard]] StringView phrase() const noexcept
    {
        if (full_phrase.isNull()) return {};
        const auto* raw = full_phrase.data();
        const uint16_t first_len = raw[-1];
        return StringView(raw, first_len);
    }

    [[nodiscard]] bool isNull() const noexcept { return name.isNull() && full_phrase.isNull(); }
};

struct Match
{
    int length = 0;
    Priority priority = Priority::NONE;
    std::vector<Rule>* rules = nullptr;
    StringView translation;
};

class Dictionary
{
public:
    explicit Dictionary();
    ~Dictionary();

    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Dictionary(Dictionary&& other) noexcept;
    Dictionary& operator=(Dictionary&& other) noexcept;

    [[nodiscard]] Match find(std::u16string_view text, int startPos) const;
    [[nodiscard]] ExactResult find_exact(std::u16string_view key) const;

    void insert(std::u16string_view key, std::u16string_view value, Priority priority);
    void insert_bulk(std::u16string_view key, Priority priority, std::u16string_view value);

    void remove(std::u16string_view key, Priority priority) const;
    void remove_meaning(std::u16string_view key, std::u16string_view value) const;

    void reorder(std::u16string_view key, const std::vector<std::u16string>& new_order) const;

    void insert_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end);
    [[nodiscard]] const Rule* find_exact_rule(std::u16string_view start, std::u16string_view end) const;
    void edit_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end) const;
    void remove_rule(std::u16string_view start, std::u16string_view end) const;

private:
    TrieNode* root;
    NodePool pool;
    mutable TextArena text_arena;

    [[nodiscard]] TrieNode* walk_node(std::u16string_view key) const;
};

struct NameSet
{
    int index = -1;
    std::u16string title;

    NameSet() = default;
    NameSet(const int idx, std::u16string t) : index(idx), title(std::move(t)) {}
};

struct Entry
{
    std::u16string original;
    std::u16string translated;
};

} // namespace hanvi

using hanvi::Priority;
using hanvi::Rule;
using hanvi::StringView;
using hanvi::NodePool;
using hanvi::TextArena;
using hanvi::TrieNode;
using hanvi::ExactResult;
using hanvi::Match;
using hanvi::Dictionary;
using hanvi::NameSet;
using hanvi::Entry;
using hanvi::NONE;
using hanvi::PHRASE;
using hanvi::NAME;
