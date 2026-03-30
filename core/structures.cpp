#include "structures.h"
#include <algorithm>
#include <ranges>

static constexpr uintptr_t TAG_MASK = 0x3;
static constexpr uintptr_t TAG_NULL = 0x0;
static constexpr uintptr_t TAG_NAME = 0x1;
static constexpr uintptr_t TAG_PHRASE = 0x2;
static constexpr uintptr_t TAG_COMPLEX = 0x3;

using ChildEntry = std::pair<QChar, TrieNode*>;

struct alignas(ChildEntry) ChildHeader {
    uint16_t capacity;

    ChildEntry* entries() {
        return reinterpret_cast<ChildEntry*>(this + 1);
    }
    [[nodiscard]] const ChildEntry* entries() const {
        return reinterpret_cast<const ChildEntry*>(this + 1);
    }
};

TrieNode* NodePool::allocate() {
    if (current_block_offset + sizeof(TrieNode) > BLOCK_SIZE) {
        auto new_block = std::make_unique<char[]>(BLOCK_SIZE);
        current_block_ptr = new_block.get();
        blocks.push_back(std::move(new_block));
        current_block_offset = 0;
    }

    const auto node = new (current_block_ptr + current_block_offset) TrieNode();
    current_block_offset += sizeof(TrieNode);
    return node;
}

void NodePool::clear() {
    blocks.clear();
    current_block_offset = BLOCK_SIZE;
    current_block_ptr = nullptr;
}

NodePool::~NodePool() {
    clear();
}

TrieNode::~TrieNode() {
    if (child_count > 1 && children_block) {
        ::operator delete(children_block);
    }
    free_data();
}

void TrieNode::free_data() {
    const uintptr_t tag = data & TAG_MASK;

    if (const auto ptr = reinterpret_cast<void*>(data & ~TAG_MASK)) {
        if (tag == TAG_NAME) {
            delete static_cast<QString*>(ptr);
        } else if (tag == TAG_PHRASE) {
            delete static_cast<QStringList*>(ptr);
        } else if (tag == TAG_COMPLEX) {
            delete static_cast<NodeData*>(ptr);
        }
    }
    data = TAG_NULL;
}

TrieNode* TrieNode::find_child(const QChar ch) const {
    if (child_count == 1) {
        return (single_char == ch) ? single_child : nullptr;
    }
    if (child_count == 0) return nullptr;

    const auto* header = static_cast<const ChildHeader*>(children_block);
    const auto* begin = header->entries();
    const auto* end = begin + child_count;

    if (child_count <= 16) {
        for (const auto* it = begin; it != end; ++it) {
            if (it->first == ch) return it->second;
        }
        return nullptr;
    }

    const auto it = std::lower_bound(begin, end, ch,
        [](const ChildEntry& p, const QChar val) {
            return p.first < val;
        });

    if (it != end && it->first == ch) {
        return it->second;
    }
    return nullptr;
}

void TrieNode::add_child(QChar ch, TrieNode* node) {
    if (child_count == 0) {
        single_char = ch;
        single_child = node;
        child_count = 1;
        return;
    }

    ChildHeader* header;

    if (child_count == 1) {
        constexpr size_t initial_cap = 2;
        constexpr size_t size_bytes = sizeof(ChildHeader) + initial_cap * sizeof(ChildEntry);
        void* mem = ::operator new(size_bytes);
        header = new (mem) ChildHeader{initial_cap};

        header->entries()[0] = {single_char, single_child};
        children_block = header;
    }
    else {
        header = static_cast<ChildHeader*>(children_block);
        if (child_count == header->capacity) {
            const size_t new_cap = header->capacity * 2;
            const size_t size_bytes = sizeof(ChildHeader) + new_cap * sizeof(ChildEntry);

            void* mem = ::operator new(size_bytes);
            auto* new_header = new (mem) ChildHeader{static_cast<uint16_t>(new_cap)};

            std::uninitialized_copy_n(header->entries(), child_count, new_header->entries());

            ::operator delete(children_block);
            children_block = new_header;
            header = new_header;
        }
    }

    auto* begin = header->entries();
    auto* end = begin + child_count;

    auto it = begin;
    if (child_count <= 16) {
        while (it != end && it->first < ch) { ++it; }
    } else {
        it = std::lower_bound(begin, end, ch,
            [](const ChildEntry& p, const QChar val) {
                return p.first < val;
            });
    }

    if (it != end) {
        std::move_backward(it, end, end + 1);
    }

    *it = {ch, node};
    child_count++;
}

QString* TrieNode::get_name() const {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_NAME) return reinterpret_cast<QString*>(ptr_val);
    if (tag == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->has_name ? &c->name : nullptr;
    }
    return nullptr;
}

QStringList* TrieNode::get_phrases() const {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_PHRASE) return reinterpret_cast<QStringList*>(ptr_val);
    if (tag == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->has_phrases ? &c->phrases : nullptr;
    }
    return nullptr;
}

std::vector<Rule>* TrieNode::get_rules() const {
    if ((data & TAG_MASK) == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        return c->rules.empty() ? nullptr : &c->rules;
    }
    return nullptr;
}

void TrieNode::ensure_complex() {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_COMPLEX) return;

    auto* complex = new NodeData();

    if (tag == TAG_NAME) {
        auto* old_name = reinterpret_cast<QString*>(ptr_val);
        complex->name = std::move(*old_name);
        complex->has_name = true;
        delete old_name;
    } else if (tag == TAG_PHRASE) {
        auto* old_phrases = reinterpret_cast<QStringList*>(ptr_val);
        complex->phrases = std::move(*old_phrases);
        complex->has_phrases = true;
        delete old_phrases;
    }

    data = reinterpret_cast<uintptr_t>(complex) | TAG_COMPLEX;
}

void TrieNode::set_name(const QString& value) {
    if (data == TAG_NULL) {
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_NAME;
        return;
    }

    if ((data & TAG_MASK) == TAG_NAME) {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = value;
    } else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name = value;
        c->has_name = true;
    }
}

void TrieNode::add_phrase(const QString& value) {
    if (data == TAG_NULL) {
        auto* list = new QStringList();
        list->prepend(value);
        data = reinterpret_cast<uintptr_t>(list) | TAG_PHRASE;
        return;
    }

    if ((data & TAG_MASK) == TAG_PHRASE) {
        auto* list = reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        list->removeAll(value);
        list->prepend(value);
    } else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases.removeAll(value);
        c->phrases.prepend(value);
        c->has_phrases = true;
    }
}

void TrieNode::set_phrases(const QStringList& list_val) {
     if (data == TAG_NULL) {
        data = reinterpret_cast<uintptr_t>(new QStringList(list_val)) | TAG_PHRASE;
        return;
    }

    if ((data & TAG_MASK) == TAG_PHRASE) {
        *reinterpret_cast<QStringList*>(data & ~TAG_MASK) = list_val;
    } else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases = list_val;
        c->has_phrases = true;
    }
}

void TrieNode::add_rule(const Rule& rule) {
    ensure_complex();
    auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
    c->rules.push_back(rule);

    std::ranges::sort(c->rules, [](const Rule& a, const Rule& b) {
        return a.original_end.length() > b.original_end.length();
    });
}

void TrieNode::remove_name() {
    if ((data & TAG_MASK) == TAG_NAME) {
        delete reinterpret_cast<QString*>(data & ~TAG_MASK);
        data = TAG_NULL;
    } else if ((data & TAG_MASK) == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name.clear();
        c->has_name = false;
    }
}

void TrieNode::remove_phrases() {
    if ((data & TAG_MASK) == TAG_PHRASE) {
        delete reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        data = TAG_NULL;
    } else if ((data & TAG_MASK) == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases.clear();
        c->has_phrases = false;
    }
}

Dictionary::~Dictionary() {
    auto destroy_recursive = [&](auto&& self, TrieNode* n) -> void {
        if (!n) return;
        if (n->child_count == 1) {
            self(self, n->single_child);
        } else if (n->child_count > 1 && n->children_block) {
            const auto* h = static_cast<const ChildHeader*>(n->children_block);
            for (int i = 0; i < n->child_count; ++i) {
                self(self, h->entries()[i].second);
            }
        }
        n->~TrieNode();
    };

    for (auto* node : root_array) {
        if (node) destroy_recursive(destroy_recursive, node);
    }
}

Dictionary::Dictionary(Dictionary&& other) noexcept
    : pool(std::move(other.pool))
{
    std::ranges::copy(other.root_array, std::begin(root_array));
    std::ranges::fill(other.root_array, nullptr);
}

Dictionary& Dictionary::operator=(Dictionary&& other) noexcept {
    if (this != &other) {
        this->~Dictionary();
        pool = std::move(other.pool);
        std::ranges::copy(other.root_array, std::begin(root_array));
        std::ranges::fill(other.root_array, nullptr);
    }
    return *this;
}

void Dictionary::insert(const QString& key, const QString& value, const Priority priority) {
    if (key.isEmpty()) return;

    const ushort root_idx = key[0].unicode();
    TrieNode* node = root_array[root_idx];
    if (!node) {
        node = pool.allocate();
        root_array[root_idx] = node;
    }

    for (int i = 1; i < key.length(); ++i) {
        const QChar ch = key[i];
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME) node->set_name(value);
    else node->add_phrase(value);
}

void Dictionary::insert_bulk(const QString& key, const Priority priority, const QString& value) {
    if (key.isEmpty()) return;

    const ushort root_idx = key[0].unicode();
    TrieNode* node = root_array[root_idx];
    if (!node) {
        node = pool.allocate();
        root_array[root_idx] = node;
    }

    for (int i = 1; i < key.length(); ++i) {
        const QChar ch = key[i];
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME) {
        node->set_name(value);
    } else {
        node->set_phrases(value.split('\x1F'));
    }
}

std::pair<QString*, QStringList*> Dictionary::find_exact(const QStringView& key) const {
    const TrieNode* node = walk_node(key);
    if (!node) return {nullptr, nullptr};
    return { (node->get_name()), (node->get_phrases()) };
}

void Dictionary::reorder(const QString& key, const QStringList& new_order) const {
    TrieNode* node = walk_node(key);
    if (!node) return;
    node->set_phrases(new_order);
}

Match Dictionary::find(const QStringView& text, const int startPos) const {
    if (startPos >= text.length()) return {0, NONE, nullptr, nullptr};

    const TrieNode* node = root_array[text[startPos].unicode()];
    if (!node) return {0, NONE, nullptr, nullptr};

    int best_len_found = 0;
    const QString* translated = nullptr;
    Priority priority = NONE;
    std::vector<Rule>* rules = nullptr;

    for (int i = startPos; i < text.length(); ++i) {
        if (i > startPos) {
            node = node->find_child(text[i]);
            if (!node) break;
        }

        if (auto* r = node->get_rules()) {
            rules = r;
        }

        if (const auto* name = node->get_name()) {
            best_len_found = i - startPos + 1;
            translated = name;
            priority = NAME;
        }
        else if (auto* phrases = node->get_phrases()) {
            if (!phrases->isEmpty()) {
                if ((i - startPos + 1) > best_len_found) {
                    best_len_found = i - startPos + 1;
                    translated = &phrases->first();
                    priority = PHRASE;
                }
            }
        }
    }

    return {best_len_found, priority, rules, translated};
}

void Dictionary::insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) {
    if (start.isEmpty()) return;

    const ushort root_idx = start[0].unicode();
    TrieNode* node = root_array[root_idx];
    if (!node) {
        node = pool.allocate();
        root_array[root_idx] = node;
    }

    for (int i = 1; i < start.length(); ++i) {
        const QChar ch = start[i];
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    node->add_rule({start, end, t_start, t_end});
}

const Rule* Dictionary::find_exact_rule(const QString& start, const QString& end) const {
    const TrieNode* node = walk_node(start);
    if (!node) return nullptr;

    if (const auto* rules = node->get_rules()) {
        const auto& rule_vector = *rules;
        const auto it = std::ranges::find_if(rule_vector.begin(), rule_vector.end(), [end](const Rule& r) {
            return r.original_end == end;
        });
        if (it != rule_vector.end()) {
            return &(*it);
        }
    }
    return nullptr;
}

void Dictionary::remove_rule(const QString& start, const QString& end) const {
    const TrieNode* node = walk_node(start);
    if (!node) return;

    if (auto* rules = node->get_rules()) {
        const auto it = std::ranges::remove_if(*rules, [&](const Rule& r) {
            return r.translation_end == end;
        }).begin();

        if (it != rules->end()) {
            rules->erase(it, rules->end());
        }
    }
}

void Dictionary::edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const {
    const TrieNode* node = walk_node(start);
    if (!node) return;

    if (auto* rules = node->get_rules()) {
        const auto it = std::ranges::find_if(*rules, [&](const Rule& r) {
            return r.original_end == end;
        });

        if (it != rules->end()) {
            it->translation_start = t_start;
            it->translation_end = t_end;
        }
    }
}

TrieNode* Dictionary::walk_node(const QStringView& key) const {
    if (key.isEmpty()) return nullptr;

    TrieNode* node = root_array[key[0].unicode()];
    if (!node) return nullptr;

    for (int i = 1; i < key.length(); ++i) {
        node = node->find_child(key[i]);
        if (!node) return nullptr;
    }
    return node;
}

void Dictionary::remove(const QString& key, const Priority priority) const {
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (priority == NAME) {
        node->remove_name();
    } else if (priority == PHRASE) {
        node->remove_phrases();
    }
}

void Dictionary::remove_meaning(const QString& key, const QString& value) const {
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (const auto list = node->get_phrases()) {
        list->removeAll(value);
        if (list->isEmpty()) {
            node->remove_phrases();
        }
    }
}