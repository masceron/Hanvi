#include "include/trie.h"

Dictionary::Dictionary(const QString& path) {
    root = new TrieNode();
}

Dictionary::~Dictionary() {
    delete root;
}

void Dictionary::insert(const QString& key, const QString& value, const Priority priority) const
{
    TrieNode* node = root;
    for (QChar ch : key) {
        if (!node->children.contains(ch)) {
            node->children[ch] = new TrieNode();
        }
        node = node->children[ch];
    }

    if (priority == NAME) {
        node->name_translation = value;
    } else {
        node->phrase_translation = value;
    }
}

void Dictionary::edit(const QString& key, Priority priority, const QString& value) const
{
    TrieNode* node = root;

    for (const QChar &ch : key) {
        node = node->children[ch];
    }

    if (priority == NAME) {
        node->name_translation = value;
    } else {
        node->phrase_translation = value;
    }
}

Match Dictionary::find(const QString& text, const int startPos) const
{
    TrieNode* node = root;

    int max_len_found = 0;
    QString translated;
    Priority priority = NONE;

    for (int i = startPos; i < text.length(); ++i) {
        QChar ch = text[i];
        if (!node->children.contains(ch)) break;
        node = node->children[ch];

        if (!node->name_translation.isEmpty() || !node->phrase_translation.isEmpty()) {
            QString cur;

            if (!node->name_translation.isEmpty())
            {
                cur = node->name_translation;
                priority = NAME;
            }
            else
            {
                cur = node->phrase_translation;
                priority = PHRASE;
            }

            max_len_found = i - startPos + 1;
            translated = cur;
        }
    }

    return {max_len_found, priority, translated};
}

void Dictionary::remove(const QString& key, const Priority priority) const
{
    remover(root, key, 0, priority);
}

bool Dictionary::remover(TrieNode* node, const QString& key, const int depth, const Priority priority) {
    if (depth == key.length()) {
        if (priority == NAME) {
            node->name_translation.clear();
        } else {
            node->phrase_translation.clear();
        }

        return node->isEmpty();
    }

    const QChar ch = key[depth];
    if (!node->children.contains(ch)) return false;

    if (TrieNode* child = node->children[ch]; remover(child, key, depth + 1, priority)) {
        delete child;
        node->children.remove(ch);

        return node->isEmpty();
    }

    return false;
}
