#include "trie.h"

Dictionary::Dictionary() {
    root = new TrieNode();
}

Dictionary::~Dictionary() {
    delete root;
}

QStringList& Dictionary::get_list(TrieNode* node, const Priority priority) {
    auto& ptr = (priority == NAME) ? node->name_translations : node->phrase_translations;
    if (!ptr) {
        ptr = std::make_unique<QStringList>();
    }
    return *ptr;
}

bool Dictionary::has_list(const TrieNode* node, const Priority priority) {
    const auto& ptr = (priority == NAME) ? node->name_translations : node->phrase_translations;
    return ptr && !ptr->isEmpty();
}

void Dictionary::insert(const QString& key, const QString& value, const Priority priority) const
{
    TrieNode* node = root;
    for (const QChar ch : key) {
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = new TrieNode();
            node->add_child(ch, next);
        }
        node = next;
    }

    QStringList& list = get_list(node, priority);
    list.removeAll(value);
    list.prepend(value);
}

void Dictionary::insert_bulk(const QString& key, const Priority priority, const QString& value) const
{
    TrieNode* node = root;
    for (const QChar ch : key) {
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = new TrieNode();
            node->add_child(ch, next);
        }
        node = next;
    }

    QStringList& list = get_list(node, priority);
    for (const auto items = value.split('\x1F'); const auto& item : items) {
        list.append(item);
    }
}

std::pair<QStringList*, QStringList*> Dictionary::find_exact(const QString& key) const
{
    const TrieNode* node = walk_node(key);

    if (!node) {
        return {};
    }

    return {node->name_translations.get(), node->phrase_translations.get()};
}

void Dictionary::reorder(const QString& key, const QStringList& new_order, const Priority priority) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    QStringList& list = get_list(node, priority);

    list.clear();
    list.append(new_order);
}

Match Dictionary::find(const QStringView& text, const int startPos) const
{
    const TrieNode* node = root;
    int bestLen = 0;
    QString translated;
    Priority priority = NONE;

    for (int i = startPos; i < text.length(); ++i) {
        const QChar ch = text[i];

        node = node->find_child(ch);
        if (!node) break;

        if (has_list(node, NAME)) {
            bestLen = i - startPos + 1;
            translated = node->name_translations->first();
            priority = NAME;
        }
        else if (has_list(node, PHRASE)) {
            if ((i - startPos + 1) > bestLen) {
                bestLen = i - startPos + 1;
                translated = node->phrase_translations->first();
                priority = PHRASE;
            }
        }
    }

    return {bestLen, priority, translated};
}

TrieNode* Dictionary::walk_node(const QString& key) const
{
    TrieNode* node = root;
    for (const QChar ch : key) {
        node = node->find_child(ch);
        if (!node) return nullptr;
    }
    return node;
}

void Dictionary::remove(const QString& key, const Priority priority) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (priority == NAME && node->name_translations) {
        node->name_translations.reset();
    } else if (priority == PHRASE && node->phrase_translations) {
        node->phrase_translations.reset();
    }
}

void Dictionary::remove_meaning(const QString& key, const QString& value, const Priority priority) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    auto& ptr = priority == NAME ? node->name_translations : node->phrase_translations;

    if (!ptr) return;

    ptr->removeAll(value);

    if (ptr->isEmpty()) {
        ptr.reset();
    }
}