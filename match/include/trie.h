#pragma once

#include <QMap>
#include <QHash>

inline QHash<QChar, QString> sv_readings;

enum Priority
{
    NONE, PHRASE, NAME
};

struct TrieNode {
    QMap<QChar, TrieNode*> children;
    Priority priority = NONE;

    QString phrase_translation;
    QString name_translation;

    ~TrieNode()
    {
        qDeleteAll(children);
    }

    [[nodiscard]] bool isEmpty() const {
        return phrase_translation.isEmpty() && name_translation.isEmpty() && children.isEmpty();
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
    [[nodiscard]] Match find(const QString& text, int startPos) const;
    void remove(const QString& key, Priority priority) const;

private:
    TrieNode* root;
    static bool remover(TrieNode* node, const QString& key, int depth, Priority priority);
};

inline Dictionary dictionary("data.bin");