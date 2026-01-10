#pragma once

#include <QVector>
#include <QString>
#include <QStringList>
#include <QHash>
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

struct TrieNode {
    QVector<std::pair<QChar, TrieNode*>> children;

    std::unique_ptr<QStringList> phrase_translations;
    std::unique_ptr<QString> name_translation;
    std::unique_ptr<std::vector<Rule>> rules;

    ~TrieNode() {
        for (const auto& val : children | std::views::values) {
            delete val;
        }
    }

    [[nodiscard]] TrieNode* find_child(const QChar ch) const {
        for (const auto& [chr, node] : children) {
            if (chr == ch) return node;
        }
        return nullptr;
    }

    void add_child(QChar ch, TrieNode* node) {
        children.append({ch, node});
    }
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
    bool operator==(const Dictionary& dictionary) const = default;
    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Dictionary(Dictionary&& other) noexcept;
    Dictionary& operator=(Dictionary&& other) noexcept;

    [[nodiscard]] Match find(const QStringView& text, int startPos) const;
    [[nodiscard]] std::pair<QString*, QStringList*> find_exact(const QString& key) const;

    void insert(const QString& key, const QString& value, Priority priority) const;
    void insert_bulk(const QString& key, Priority priority, const QString& value) const;

    void remove(const QString& key, Priority priority) const;
    void remove_meaning(const QString& key, const QString& value) const;

    void reorder(const QString& key, const QStringList& new_order) const;

    void insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const;
    const Rule* find_exact_rule(const QString& start, const QString& end) const;
    void edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const;
    void remove_rule(const QString& start, const QString& end) const;

private:
    TrieNode* root;
    [[nodiscard]] TrieNode* walk_node(const QString& key) const;
};

struct NameSet
{
    int index;
    QString title;
};