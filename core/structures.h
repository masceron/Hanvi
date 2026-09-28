#pragma once

#include <QStringList>
#include <QStringView>
#include <memory>
#include <vector>

enum Priority { NONE, PHRASE, NAME };

struct Rule
{
    QString original_start;
    QString original_end;
    QString translation_start;
    QString translation_end;
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

    const char16_t* allocate(const QStringView& str);
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
    QChar single_child_char;
    uint16_t child_count = 0;
    uint32_t reserved = 0;

    TrieNode() = default;

    TrieNode(const TrieNode&) = delete;
    TrieNode& operator=(const TrieNode&) = delete;

    ~TrieNode();

    [[nodiscard]] TrieNode* find_child(QChar ch) const;
    void add_child(QChar ch, TrieNode* node);

    [[nodiscard]] QStringView get_name() const;
    [[nodiscard]] QStringView get_first_phrase() const;
    [[nodiscard]] QStringView get_full_phrase() const;
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
    QStringView name;
    QStringView full_phrase;

    [[nodiscard]] QStringView phrase() const noexcept
    {
        if (full_phrase.isNull() || full_phrase.isEmpty()) return {};
        const auto* raw = full_phrase.utf16();
        const uint16_t first_len = raw[-1];
        return QStringView(raw, first_len);
    }
};

struct Match
{
    int length;
    Priority priority;
    std::vector<Rule>* rules;
    QStringView translation;
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

    [[nodiscard]] Match find(const QStringView& text, int startPos) const;
    [[nodiscard]] ExactResult find_exact(const QStringView& key) const;

    void insert(const QString& key, const QString& value, Priority priority);
    void insert_bulk(const QString& key, Priority priority, const QString& value);

    void remove(const QString& key, Priority priority) const;
    void remove_meaning(const QString& key, const QString& value) const;

    void reorder(const QString& key, const QStringList& new_order) const;

    void insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end);
    [[nodiscard]] const Rule* find_exact_rule(const QString& start, const QString& end) const;
    void edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const;
    void remove_rule(const QString& start, const QString& end) const;

private:
    TrieNode* root;
    NodePool pool;
    mutable TextArena text_arena;

    [[nodiscard]] TrieNode* walk_node(const QStringView& key) const;
};

struct NameSet
{
    int index;
    QString title;
};
