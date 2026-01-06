#pragma once

#include <QMap>
#include <QHash>

inline QHash<QChar, QString> sv_readings;
inline QHash<QChar, QChar> punctuations;

enum Priority
{
    NONE, PHRASE, NAME
};

struct TrieNode {
    QMap<QChar, TrieNode*> children;
    Priority priority = NONE;

    QStringList phrase_translations;
    QStringList name_translations;

    ~TrieNode()
    {
        qDeleteAll(children);
    }

    [[nodiscard]] bool isEmpty() const {
        return phrase_translations.isEmpty() && name_translations.isEmpty() && children.isEmpty();
    }
};

struct Match
{
    int length;
    Priority priority;
    QString translation;
};

class Dictionary
{
public:
    explicit Dictionary(const QString& path);
    ~Dictionary();
    void insert(const QString& key, const QString& value, Priority priority) const;
    void edit(const QString& key, Priority priority, const QString& value) const;
    void choose_meaning(const QString& key, Priority priority, const QString& value) const;
    [[nodiscard]] Match find(const QStringView& text, int startPos) const;
    void remove(const QString& key, Priority priority) const;
    void insert_bulk(const QString& key, Priority priority, const QString& value) const;
private:
    TrieNode* root;
    static bool remover(TrieNode* node, const QString& key, int depth, Priority priority);
    [[nodiscard]] TrieNode* walk_node(const QString& key) const;
};

inline Dictionary dictionary("data.bin");