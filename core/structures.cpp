#include "structures.h"
#include <algorithm>
#include <ranges>

static constexpr uintptr_t TAG_MASK = 0x3;
static constexpr uintptr_t TAG_NULL = 0x0;
static constexpr uintptr_t TAG_NAME = 0x1;
static constexpr uintptr_t TAG_PHRASE = 0x2;
static constexpr uintptr_t TAG_COMPLEX = 0x3;

struct NodeData {
    std::unique_ptr<QString> name;
    std::unique_ptr<QStringList> phrases;
    std::vector<Rule> rules;
};

struct ChildHeader {
    uint16_t capacity;
    uint16_t count;
    std::pair<QChar, TrieNode*> entries[]; 
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
    if (children_block) {
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
    if (!children_block) return nullptr;

    const auto* header = static_cast<ChildHeader*>(children_block);
    const auto* begin = header->entries;
    const auto* end = header->entries + header->count;

    const auto it = std::lower_bound(begin, end, ch,
        [](const std::pair<QChar, TrieNode*>& p, const QChar val) {
            return p.first < val;
        });

    if (it != end && it->first == ch) {
        return it->second;
    }
    return nullptr;
}

void TrieNode::add_child(QChar ch, TrieNode* node) {
    auto header = static_cast<ChildHeader*>(children_block);

    if (!header) {
        constexpr size_t initial_cap = 2;
        constexpr size_t size_bytes = sizeof(ChildHeader) + initial_cap * sizeof(std::pair<QChar, TrieNode*>);
        void* mem = ::operator new(size_bytes);
        header = new (mem) ChildHeader;
        header->capacity = static_cast<uint16_t>(initial_cap);
        header->count = 0;
        children_block = header;
    } 
    else if (header->count == header->capacity) {
        const size_t new_cap = header->capacity * 2;
        const size_t size_bytes = sizeof(ChildHeader) + new_cap * sizeof(std::pair<QChar, TrieNode*>);
        
        void* mem = ::operator new(size_bytes);
        auto* new_header = new (mem) ChildHeader;
        new_header->capacity = static_cast<uint16_t>(new_cap);
        new_header->count = header->count;

        std::uninitialized_copy_n(header->entries, header->count, new_header->entries);
        
        ::operator delete(children_block);
        children_block = new_header;
        header = new_header;
    }

    auto* begin = header->entries;
    auto* end = header->entries + header->count;
    
    const auto it = std::lower_bound(begin, end, ch,
        [](const std::pair<QChar, TrieNode*>& p, const QChar val) {
            return p.first < val;
        });

    if (it != end) {
        std::move_backward(it, end, end + 1);
    }
    
    *it = {ch, node};
    header->count++;
}

const QString* TrieNode::get_name() const {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;
    
    if (tag == TAG_NAME) return reinterpret_cast<QString*>(ptr_val);
    if (tag == TAG_COMPLEX) {
        const auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->name.get();
    }
    return nullptr;
}

const QStringList* TrieNode::get_phrases() const {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_PHRASE) return reinterpret_cast<QStringList*>(ptr_val);
    if (tag == TAG_COMPLEX) {
        const auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->phrases.get();
    }
    return nullptr;
}

std::vector<Rule>* TrieNode::get_rules() const {
    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        return &c->rules;
    }
    return nullptr;
}

void TrieNode::ensure_complex() {
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_COMPLEX) return;

    auto* complex = new NodeData();

    if (tag == TAG_NAME) {
        complex->name = std::unique_ptr<QString>(reinterpret_cast<QString*>(ptr_val));
    } else if (tag == TAG_PHRASE) {
        complex->phrases = std::unique_ptr<QStringList>(reinterpret_cast<QStringList*>(ptr_val));
    }

    data = reinterpret_cast<uintptr_t>(complex) | TAG_COMPLEX;
}

void TrieNode::set_name(const QString& value) {
    if (data == TAG_NULL) {
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_NAME;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    
    if (tag == TAG_NAME) {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = value;
    } 
    else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name = std::make_unique<QString>(value);
    }
}

void TrieNode::add_phrase(const QString& value) {
    if (data == TAG_NULL) {
        auto* list = new QStringList();
        list->prepend(value);
        data = reinterpret_cast<uintptr_t>(list) | TAG_PHRASE;
        return;
    }

    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_PHRASE) {
        auto* list = reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        list->removeAll(value);
        list->prepend(value);
    }
    else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        if (!c->phrases) c->phrases = std::make_unique<QStringList>();
        c->phrases->removeAll(value);
        c->phrases->prepend(value);
    }
}

void TrieNode::set_phrases(const QStringList& list_val) {
     if (data == TAG_NULL) {
        data = reinterpret_cast<uintptr_t>(new QStringList(list_val)) | TAG_PHRASE;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE) {
        *reinterpret_cast<QStringList*>(data & ~TAG_MASK) = list_val;
    } else {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases = std::make_unique<QStringList>(list_val);
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
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NAME) {
        delete reinterpret_cast<QString*>(data & ~TAG_MASK);
        data = TAG_NULL;
    } else if (tag == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name.reset();
    }
}

void TrieNode::remove_phrases() {
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE) {
        delete reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        data = TAG_NULL;
    } else if (tag == TAG_COMPLEX) {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases.reset();
    }
}

Dictionary::Dictionary() {
    root = pool.allocate();
}

Dictionary::~Dictionary() {
    if (root) {
        auto destroy_recursive = [&](auto&& self, TrieNode* n) -> void {
            if (!n) return;
            if (n->children_block) {
                const auto h = static_cast<ChildHeader*>(n->children_block);
                for (int i = 0; i < h->count; ++i) {
                    self(self, h->entries[i].second);
                }
            }
            n->~TrieNode();
        };
        destroy_recursive(destroy_recursive, root);
    }
}

Dictionary::Dictionary(Dictionary&& other) noexcept
    : root(other.root), pool(std::move(other.pool))
{
    other.root = nullptr;
}

Dictionary& Dictionary::operator=(Dictionary&& other) noexcept {
    if (this != &other) {
        if (root) {
            auto destroy_recursive = [&](auto&& self, TrieNode* n) -> void {
                if (!n) return;
                if (n->children_block) {
                    auto* h = static_cast<ChildHeader*>(n->children_block);
                    for (int i = 0; i < h->count; ++i) {
                        self(self, h->entries[i].second);
                    }
                }
                n->~TrieNode();
            };
            destroy_recursive(destroy_recursive, root);
        }

        pool = std::move(other.pool);
        root = other.root;

        other.root = nullptr;
    }
    return *this;
}

void Dictionary::insert(const QString& key, const QString& value, const Priority priority) const
{
    auto* mutable_this = const_cast<Dictionary*>(this);
    
    TrieNode* node = root;
    for (const QChar ch : key) {
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = mutable_this->pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME) {
        node->set_name(value);
    }
    else {
        node->add_phrase(value);
    }
}

void Dictionary::insert_bulk(const QString& key, const Priority priority, const QString& value) const
{
    auto* mutable_this = const_cast<Dictionary*>(this);

    TrieNode* node = root;
    for (const QChar ch : key) {
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = mutable_this->pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME) {
        node->set_name(value);
    }
    else {
        QStringList list;
        for (const auto items = value.split('\x1F'); const auto& item : items) {
            list.append(item);
        }
        node->set_phrases(list);
    }
}

std::pair<QString*, QStringList*> Dictionary::find_exact(const QStringView& key) const
{
    const TrieNode* node = walk_node(key);

    if (!node) {
        return {nullptr, nullptr};
    }

    return { const_cast<QString*>(node->get_name()), const_cast<QStringList*>(node->get_phrases()) };
}

void Dictionary::reorder(const QString& key, const QStringList& new_order) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;
    
    node->set_phrases(new_order);
}

Match Dictionary::find(const QStringView& text, const int startPos) const
{
    const TrieNode* node = root;
    int best_len_found = 0;
    const QString* translated = nullptr;
    Priority priority = NONE;

    std::vector<Rule>* rules = nullptr;

    for (int i = startPos; i < text.length(); ++i) {
        const QChar ch = text[i];

        node = node->find_child(ch);
        if (!node) break;

        if (auto* r = node->get_rules())
        {
            rules = r;
        }

        if (auto* name = node->get_name()) {
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

void Dictionary::insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const
{
    auto* mutable_this = const_cast<Dictionary*>(this);

    TrieNode* node = root;
    for (const QChar ch : start) {
        TrieNode* next = node->find_child(ch);
        if (!next) {
            next = mutable_this->pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    node->add_rule({start, end, t_start, t_end});
}

const Rule* Dictionary::find_exact_rule(const QString& start, const QString& end) const
{
    const TrieNode* node = walk_node(start);
    if (!node) return nullptr;

    if (const auto* rules = node->get_rules())
    {
        const auto& rule_vector = *rules;
        const auto it = std::ranges::find_if(rule_vector.begin(), rule_vector.end(), [end](const Rule& r)
        {
            return r.original_end == end;
        });
        if (it != rule_vector.end())
        {
            return &(*it);
        }
    }
    return nullptr;
}

void Dictionary::remove_rule(const QString& start, const QString& end) const
{
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

void Dictionary::edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const
{
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

TrieNode* Dictionary::walk_node(const QStringView& key) const
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
        node->remove_name();
    } else if (priority == PHRASE) {
        node->remove_phrases();
    }
}

void Dictionary::remove_meaning(const QString& key, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (const auto list = const_cast<QStringList*>(node->get_phrases())) {
        list->removeAll(value);
        if (list->isEmpty()) {
            node->remove_phrases();
        }
    }
}