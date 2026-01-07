#pragma once

#include <QVector>
#include <QString>
#include <QStringList>
#include <QHash>
#include <memory>

inline QHash<QChar, QString> sv_readings;
inline QHash<QChar, QChar> punctuations;

enum Priority { NONE, PHRASE, NAME };

struct TrieNode {
    QVector<std::pair<QChar, TrieNode*>> children;

    std::unique_ptr<QStringList> phrase_translations;
    std::unique_ptr<QStringList> name_translations;

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
    QString translation;
};

class Dictionary {
public:
    explicit Dictionary();
    ~Dictionary();

    [[nodiscard]] Match find(const QStringView& text, int startPos) const;
    [[nodiscard]] std::pair<QStringList*, QStringList*> find_exact(const QString& key) const;

    void insert(const QString& key, const QString& value, Priority priority) const;
    void insert_bulk(const QString& key, Priority priority, const QString& value) const;

    void remove(const QString& key, Priority priority) const;
    void remove_meaning(const QString& key, const QString& value, Priority priority) const;

    void reorder(const QString& key, const QStringList& new_order, Priority priority) const;
private:
    TrieNode* root;
    [[nodiscard]] TrieNode* walk_node(const QString& key) const;

    static QStringList& get_list(TrieNode* node, Priority priority);
    static bool has_list(const TrieNode* node, Priority priority);
};

inline Dictionary dictionary;
inline Dictionary name_set_dictionary;
inline int current_name_set_id = -1;

struct NameSet
{
    int index;
    QString title;
};

inline std::vector<NameSet> name_sets;