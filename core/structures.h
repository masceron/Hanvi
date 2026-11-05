#pragma once

#include <memory>
#include <span>
#include <utility>
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
struct ComplexNodeData;

// Owns everything a trie is made of, so a trie is freed without visiting its nodes.
class NodePool
{
public:
    NodePool() = default;

    NodePool(NodePool&& other) noexcept
        : blocks(std::move(other.blocks)),
          complex_data(std::move(other.complex_data)),
          current_block_offset(other.current_block_offset),
          current_block_ptr(other.current_block_ptr)
    {
        other.complex_data.clear();
        other.current_block_offset = BLOCK_SIZE;
        other.current_block_ptr = nullptr;
    }

    NodePool& operator=(NodePool&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            blocks = std::move(other.blocks);
            complex_data = std::move(other.complex_data);
            current_block_offset = other.current_block_offset;
            current_block_ptr = other.current_block_ptr;
            other.complex_data.clear();
            other.current_block_offset = BLOCK_SIZE;
            other.current_block_ptr = nullptr;
        }
        return *this;
    }

    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;

    TrieNode* allocate();
    // Room for a node's child array, which lives as long as the pool.
    void* allocate_bytes(size_t size);
    // Data for the few nodes holding more than one value or rules.
    ComplexNodeData* allocate_complex();
    void clear();
    ~NodePool();

private:
    static constexpr size_t BLOCK_SIZE = 65520;
    std::vector<std::unique_ptr<char[]>> blocks;
    std::vector<ComplexNodeData*> complex_data;
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

    TrieNode() = default;

    TrieNode(const TrieNode&) = delete;
    TrieNode& operator=(const TrieNode&) = delete;

    [[nodiscard]] TrieNode* find_child(char16_t ch) const;
    // Two or more children are kept in an exactly sized array from the pool: chars in order,
    // then the nodes. Adding one copies them to a new array.
    void add_child(char16_t ch, TrieNode* node, NodePool& pool);
    // Replaces the children of a node that has none; children are sorted by char.
    void set_children(std::span<const std::pair<char16_t, TrieNode*>> children, NodePool& pool);

    [[nodiscard]] ArenaText get_name() const;
    [[nodiscard]] ArenaText get_first_phrase() const;
    [[nodiscard]] ArenaText get_full_phrase() const;
    [[nodiscard]] std::vector<Rule>* get_rules() const;

    void set_name_ptr(const char* ptr, NodePool& pool);
    void set_phrase_ptr(const char* ptr, NodePool& pool);
    void add_rule(const Rule& rule, NodePool& pool);

    void remove_name();
    void remove_phrases();

private:
    void ensure_complex(NodePool& pool);
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

    void remove(std::u16string_view key, Priority priority) const;
    void remove_meaning(std::u16string_view key, std::u16string_view value);

    void reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);

    void insert_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end);
    [[nodiscard]] const Rule* find_exact_rule(std::u16string_view start, std::u16string_view end) const;
    void edit_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end) const;
    void remove_rule(std::u16string_view start, std::u16string_view end) const;

private:
    friend class DictionaryLoader;

    TrieNode* root;
    // The root's children by char, for dictionaries big enough that every lookup would otherwise
    // start with a binary search over thousands of them; null for small ones.
    std::unique_ptr<TrieNode*[]> root_index;
    NodePool pool;
    TextArena text_arena;

    [[nodiscard]] TrieNode* root_child(char16_t ch) const;
    [[nodiscard]] TrieNode* walk_node(std::u16string_view key) const;
    // The node for key below node, created along with any missing on the way.
    TrieNode* make_path(TrieNode* node, std::u16string_view key);
    void set_bulk_value(TrieNode* node, Priority priority, std::string_view value);
    void index_root();
};

// Fills a new, empty Dictionary from keys in the order SQLite returns TEXT keys in (code point
// order). A node's children are all known once the keys move past it, so each gets an exactly
// sized array. The odd key out of order is still inserted, one child at a time.
class DictionaryLoader
{
public:
    explicit DictionaryLoader(Dictionary& dictionary);
    ~DictionaryLoader();

    DictionaryLoader(const DictionaryLoader&) = delete;
    DictionaryLoader& operator=(const DictionaryLoader&) = delete;

    // key and value are UTF-8, straight from SQLite.
    void add(std::string_view key, Priority priority, std::string_view value);
    // Gives the nodes still open their children; called by the destructor if not before. Keys
    // added afterwards go in one by one.
    void finish();

private:
    // Gives the nodes deeper than depth on the previous key's path their children.
    void close_deeper_than(size_t depth);
    void add_out_of_order(std::u16string_view key, size_t common, Priority priority, std::string_view value);

    // Below this many children the root is searched like any node.
    static constexpr uint16_t ROOT_INDEX_MIN_CHILDREN = 1024;

    Dictionary& dict;
    std::u16string key16;
    std::u16string previous;
    // path[d] is the node for the previous key's first d units, pending[d] its children so far.
    std::vector<TrieNode*> path;
    std::vector<std::vector<std::pair<char16_t, TrieNode*>>> pending;
    // False once finished, or for a dictionary that was not empty.
    bool in_order = true;
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
using hanvi::DictionaryLoader;
using hanvi::NameSet;
using hanvi::Entry;
using hanvi::NONE;
using hanvi::PHRASE;
using hanvi::NAME;
