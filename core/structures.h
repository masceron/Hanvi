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

struct TrieNode {
    std::vector<std::pair<QChar, TrieNode*>> children;
    std::unique_ptr<QStringList> phrase_translations;
    std::unique_ptr<QString> name_translation;
    std::unique_ptr<std::vector<Rule>> rules;

    ~TrieNode() {
        for (const auto& val : children | std::views::values) {
            delete val;
        }
    }

    [[nodiscard]] TrieNode* find_child(const QChar ch) const {
        const auto it = std::lower_bound(children.begin(), children.end(), ch,
            [](const std::pair<QChar, TrieNode*>& p, const QChar val) {
                return p.first < val;
            });

        if (it != children.end() && it->first == ch) {
            return it->second;
        }
        return nullptr;
    }

    void add_child(QChar ch, TrieNode* node) {
        const auto it = std::lower_bound(children.begin(), children.end(), ch,
            [](const std::pair<QChar, TrieNode*>& p, const QChar val) {
                return p.first < val;
            });

        children.insert(it, {ch, node});
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
    [[nodiscard]] const Rule* find_exact_rule(const QString& start, const QString& end) const;
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