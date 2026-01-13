#pragma once

#include <QStringList>
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
struct NodeData;

class NodePool {
public:
    NodePool() = default;

    NodePool(NodePool&&) = default;
    NodePool& operator=(NodePool&&) = default;

    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;

    TrieNode* allocate();
    void clear();
    ~NodePool();

private:
    static constexpr size_t BLOCK_SIZE = 4096;
    std::vector<std::unique_ptr<char[]>> blocks;
    size_t current_block_offset = BLOCK_SIZE;
    char* current_block_ptr = nullptr;
};

struct TrieNode {
    // Tagged pointer for data.
    // Tags (Low 2 bits):
    // 00: nullptr (No data)
    // 01: QString* (Name translation only)
    // 10: QStringList* (Phrase translations only)
    // 11: ComplexNodeData* (Rules, or mixed data)
    uintptr_t data = 0;
    void* children_block = nullptr;

    TrieNode() = default;

    TrieNode(const TrieNode&) = delete;
    TrieNode& operator=(const TrieNode&) = delete;

    ~TrieNode();

    [[nodiscard]] TrieNode* find_child(QChar ch) const;
    void add_child(QChar ch, TrieNode* node);

    [[nodiscard]] const QString* get_name() const;
    [[nodiscard]] const QStringList* get_phrases() const;
    [[nodiscard]] std::vector<Rule>* get_rules() const;

    void set_name(const QString& value);
    void add_phrase(const QString& value);
    void set_phrases(const QStringList& list);
    void add_rule(const Rule& rule);

    void remove_name();
    void remove_phrases();

private:
    void ensure_complex(); 
    void free_data();
};

struct Match {
    int length;
    Priority priority;
    std::vector<Rule>* rules;
    const QString* translation;
};

class Dictionary {
public:
    explicit Dictionary();
    ~Dictionary();
    
    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Dictionary(Dictionary&& other) noexcept;
    Dictionary& operator=(Dictionary&& other) noexcept;

    [[nodiscard]] Match find(const QStringView& text, int startPos) const;
    [[nodiscard]] std::pair<QString*, QStringList*> find_exact(const QStringView& key) const;

    void insert(const QString& key, const QString& value, Priority priority) const;
    void insert_bulk(const QString& key, Priority priority, const QString& value) const;

    void remove(const QString& key, Priority priority) const;
    void remove_meaning(const QString& key, const QString& value) const;

    void reorder(const QString& key, const QStringList& new_order) const;

    void insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const;
    [[nodiscard]] const Rule* find_exact_rule(const QString& start, const QString& end) const;
    void edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const;
    void remove_rule(const QString& start, const QString& end) const;

private:
    TrieNode* root;
    NodePool pool;
    
    [[nodiscard]] TrieNode* walk_node(const QStringView& key) const;
};

struct NameSet
{
    int index;
    QString title;
};