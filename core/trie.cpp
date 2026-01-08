#include "trie.h"

Dictionary::Dictionary() {
    root = new TrieNode();
}

Dictionary::~Dictionary() {
    delete root;
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

    if (priority == NAME) {
        node->name_translation = std::make_unique<QString>(value);
    }
    else {
        if (!node->phrase_translations) {
            node->phrase_translations = std::make_unique<QStringList>();
        }
        QStringList& list = *node->phrase_translations;
        list.removeAll(value);
        list.prepend(value);
    }
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

    if (priority == NAME) {
        node->name_translation = std::make_unique<QString>(value);
    }
    else {
        if (!node->phrase_translations) {
            node->phrase_translations = std::make_unique<QStringList>();
        }
        QStringList& list = *node->phrase_translations;
        for (const auto items = value.split('\x1F'); const auto& item : items) {
            list.append(item);
        }
    }
}

std::pair<QString*, QStringList*> Dictionary::find_exact(const QString& key) const
{
    const TrieNode* node = walk_node(key);

    if (!node) {
        return {nullptr, nullptr};
    }

    return {node->name_translation.get(), node->phrase_translations.get()};
}

void Dictionary::reorder(const QString& key, const QStringList& new_order) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (!node->phrase_translations) {
        node->phrase_translations = std::make_unique<QStringList>();
    }

    *node->phrase_translations = new_order;
}

Dictionary::Dictionary(Dictionary&& other) noexcept : root(other.root) {
    other.root = nullptr;
}

Dictionary& Dictionary::operator=(Dictionary&& other) noexcept {
    if (this != &other) {
        delete root;
        root = other.root;
        other.root = nullptr;
    }
    return *this;
}

Match Dictionary::find(const QStringView& text, const int startPos) const
{
    const TrieNode* node = root;
    int best_len_found = 0;
    QString translated;
    Priority priority = NONE;

    for (int i = startPos; i < text.length(); ++i) {
        const QChar ch = text[i];

        node = node->find_child(ch);
        if (!node) break;

        if (node->name_translation) {
            best_len_found = i - startPos + 1;
            translated = *node->name_translation;
            priority = NAME;
        }
        else if (node->phrase_translations && !node->phrase_translations->isEmpty()) {
            if ((i - startPos + 1) > best_len_found) {
                best_len_found = i - startPos + 1;
                translated = node->phrase_translations->first();
                priority = PHRASE;
            }
        }
    }

    return {best_len_found, priority, translated};
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

    if (priority == NAME) {
        node->name_translation.reset();
    } else if (priority == PHRASE) {
        node->phrase_translations.reset();
    }
}

void Dictionary::remove_meaning(const QString& key, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    auto& ptr = node->phrase_translations;
    if (!ptr) return;

    ptr->removeAll(value);
    if (ptr->isEmpty()) {
        ptr.reset();
    }
}