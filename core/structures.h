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

// Translations are kept as UTF-8 in the dictionary's TextArena, a third smaller than UTF-16
// (most Vietnamese letters are ASCII). The text is only ever read back as UTF-16.
class ArenaText
{
public:
    constexpr ArenaText() noexcept = default;
    constexpr ArenaText(const char* str, const size_t len) noexcept : text(str, len) {}

    [[nodiscard]] bool isNull() const noexcept { return text.data() == nullptr; }
    [[nodiscard]] bool isEmpty() const noexcept { return text.empty(); }

    [[nodiscard]] std::u16string str() const;
    // sep must be ASCII.
    [[nodiscard]] std::vector<std::u16string> split(char sep) const;

private:
    std::string_view text;
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

    // Stores str as UTF-8 after two uint16_t byte lengths: the whole text, and up to the first
    // \x1F (the first of a phrase's meanings).
    const char* allocate(std::u16string_view str);
    // The same for text that is UTF-8 already, as SQLite stores it.
    const char* allocate(std::string_view utf8);
    void clear();

private:
    char* reserve(size_t max_len);
    const char* commit(char* header, const char* first_end, const char* end);

    static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024;
    std::vector<std::unique_ptr<char[]>> blocks;
    size_t current_block_offset = BLOCK_SIZE;
    char* current_block_ptr = nullptr;
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

    [[nodiscard]] ArenaText get_name() const;
    [[nodiscard]] ArenaText get_first_phrase() const;
    [[nodiscard]] ArenaText get_full_phrase() const;
    [[nodiscard]] std::vector<Rule>* get_rules() const;

    void set_name_ptr(const char* ptr);
    void set_phrase_ptr(const char* ptr);
    void add_rule(const Rule& rule);

    void remove_name();
    void remove_phrases();

private:
    void ensure_complex();
    void free_data();
};

struct ExactResult
{
    ArenaText name;
    // All meanings, separated by \x1F, and the first of them.
    ArenaText full_phrase;
    ArenaText phrase;

    [[nodiscard]] bool isNull() const noexcept { return name.isNull() && full_phrase.isNull(); }
};

struct Match
{
    int length = 0;
    Priority priority = Priority::NONE;
    std::vector<Rule>* rules = nullptr;
    ArenaText translation;
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
    // For loading: value is UTF-8, straight from SQLite.
    void insert_bulk(std::u16string_view key, Priority priority, std::string_view value);

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
using hanvi::ArenaText;
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
