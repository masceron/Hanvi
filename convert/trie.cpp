#include "trie.h"

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

    QStringList* targetList = (priority == NAME) ? &node->name_translations : &node->phrase_translations;
    targetList->removeAll(value);
    targetList->prepend(value);
}

void Dictionary::edit(const QString& key, const Priority priority, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (QStringList* list = (priority == NAME) ? &node->name_translations : &node->phrase_translations; !list->isEmpty()) {
        (*list)[0] = value;
    } else {
        list->append(value);
    }
}

void Dictionary::choose_meaning(const QString& key, const Priority priority, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    QStringList* list = (priority == NAME) ? &node->name_translations : &node->phrase_translations;

    if (const auto idx = list->indexOf(value); idx != -1) {
        list->removeAt(idx);
        list->prepend(value);
    } else {
        list->prepend(value);
    }
}

Match Dictionary::find(const QStringView& text, const int startPos) const
{
    TrieNode* node = root;
    int bestLen = 0;
    QString translated;
    Priority priority = NONE;

    for (int i = startPos; i < text.length(); ++i) {
        QChar ch = text[i];
        if (!node->children.contains(ch)) break;
        node = node->children[ch];

        if (!node->name_translations.isEmpty()) {
            bestLen = i - startPos + 1;
            translated = node->name_translations.first();
            priority = NAME;
            continue;
        }

        if (!node->phrase_translations.isEmpty()) {
            if ((i - startPos + 1) > bestLen) {
                bestLen = i - startPos + 1;
                translated = node->phrase_translations.first();
                priority = PHRASE;
            }
        }
    }

    return {bestLen, priority, translated};
}

void Dictionary::remove(const QString& key, const Priority priority) const
{
    remover(root, key, 0, priority);
}

void Dictionary::insert_bulk(const QString& key, const Priority priority, const QString& value) const
{
    TrieNode* node = root;
    for (const QChar &ch : key) {
        if (!node->children.contains(ch)) {
            node->children[ch] = new TrieNode();
        }
        node = node->children[ch];
    }

    if (priority == NAME) {
        node->name_translations.append(value.split('\x1F'));
    } else {
        node->phrase_translations.append(value.split('\x1F'));
    }
}

bool Dictionary::remover(TrieNode* node, const QString& key, const int depth, const Priority priority) {
    if (depth == key.length()) {
        if (priority == NAME) {
            node->name_translations.clear();
        } else {
            node->phrase_translations.clear();
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

TrieNode* Dictionary::walk_node(const QString& key) const
{
    TrieNode* node = root;

    for (const QChar &ch : key) {
        if (!node->children.contains(ch)) {
            return nullptr; // Dead end: Key doesn't exist
        }

        node = node->children.value(ch);
    }

    return node;
}
