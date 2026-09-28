#include "structures.h"
#include <algorithm>
#include <ranges>

static constexpr uintptr_t TAG_MASK = 0x7;
static constexpr uintptr_t TAG_NULL = 0x0;
static constexpr uintptr_t TAG_NAME = 0x1;
static constexpr uintptr_t TAG_PHRASE = 0x2;
static constexpr uintptr_t TAG_MULTI_PHRASE = 0x3;
static constexpr uintptr_t TAG_COMPLEX = 0x4;

struct NodeData
{
    std::unique_ptr<QString> name;
    std::unique_ptr<QString> single_phrase;
    std::unique_ptr<QStringList> phrases;
    std::vector<Rule> rules;
};

namespace
{
    struct ChildHeader
    {
        uint16_t capacity;
        uint16_t count;

        [[nodiscard]] QChar* chars()
        {
            return reinterpret_cast<QChar*>(this + 1);
        }

        [[nodiscard]] const QChar* chars() const
        {
            return reinterpret_cast<const QChar*>(this + 1);
        }

        [[nodiscard]] TrieNode** nodes()
        {
            const size_t char_bytes = capacity * sizeof(QChar);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~size_t(7);
            return reinterpret_cast<TrieNode**>(reinterpret_cast<char*>(this) + aligned_offset);
        }

        [[nodiscard]] TrieNode* const* nodes() const
        {
            const size_t char_bytes = capacity * sizeof(QChar);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~size_t(7);
            return reinterpret_cast<TrieNode* const*>(reinterpret_cast<const char*>(this) + aligned_offset);
        }

        static size_t allocation_size(const size_t cap)
        {
            const size_t char_bytes = cap * sizeof(QChar);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~size_t(7);
            return aligned_offset + cap * sizeof(TrieNode*);
        }
    };
}

TrieNode* NodePool::allocate()
{
    if (current_block_offset + sizeof(TrieNode) > BLOCK_SIZE)
    {
        auto new_block = std::make_unique<char[]>(BLOCK_SIZE);
        current_block_ptr = new_block.get();
        blocks.push_back(std::move(new_block));
        current_block_offset = 0;
    }

    const auto node = new(current_block_ptr + current_block_offset) TrieNode();
    current_block_offset += sizeof(TrieNode);
    return node;
}

void NodePool::clear()
{
    blocks.clear();
    current_block_offset = BLOCK_SIZE;
    current_block_ptr = nullptr;
}

NodePool::~NodePool()
{
    clear();
}

QString* StringPool::allocate(const QString& val)
{
    if (current_block_offset + sizeof(QString) > BLOCK_SIZE)
    {
        auto new_block = std::make_unique<char[]>(BLOCK_SIZE);
        current_block_ptr = new_block.get();
        blocks.push_back(std::move(new_block));
        current_block_offset = 0;
    }

    auto* ptr = reinterpret_cast<QString*>(current_block_ptr + current_block_offset);
    new(ptr) QString(val);
    current_block_offset += sizeof(QString);
    return ptr;
}

void StringPool::clear()
{
    if (blocks.empty()) return;

    for (size_t b = 0; b + 1 < blocks.size(); ++b)
    {
        auto* qstr = reinterpret_cast<QString*>(blocks[b].get());
        constexpr size_t count = BLOCK_SIZE / sizeof(QString);
        for (size_t i = 0; i < count; ++i)
        {
            qstr[i].~QString();
        }
    }

    auto* last_qstr = reinterpret_cast<QString*>(blocks.back().get());
    const size_t last_count = current_block_offset / sizeof(QString);
    for (size_t i = 0; i < last_count; ++i)
    {
        last_qstr[i].~QString();
    }

    blocks.clear();
    current_block_offset = BLOCK_SIZE;
    current_block_ptr = nullptr;
}

StringPool::~StringPool()
{
    clear();
}

TrieNode::~TrieNode()
{
    if (child_count > 1 && first_child)
    {
        operator delete(first_child);
    }

    free_data();
}

void TrieNode::free_data()
{
    const uintptr_t tag = data & TAG_MASK;

    if (const auto ptr = reinterpret_cast<void*>(data & ~TAG_MASK))
    {
        if (tag == TAG_MULTI_PHRASE)
        {
            delete static_cast<QStringList*>(ptr);
        }
        else if (tag == TAG_COMPLEX)
        {
            delete static_cast<NodeData*>(ptr);
        }
    }
    data = TAG_NULL;
}

TrieNode* TrieNode::find_child(const QChar ch) const
{
    if (child_count == 1)
    {
        return single_child_char == ch ? first_child : nullptr;
    }
    if (child_count == 0)
    {
        return nullptr;
    }

    const auto* header = reinterpret_cast<const ChildHeader*>(first_child);
    const auto* ch_ptr = header->chars();
    const auto* nd_ptr = header->nodes();
    const uint16_t n = header->count;

    if (n <= 8)
    {
        for (uint16_t i = 0; i < n; ++i)
        {
            if (ch_ptr[i] == ch) return nd_ptr[i];
        }
        return nullptr;
    }

    const auto it = std::lower_bound(ch_ptr, ch_ptr + n, ch);
    if (it != ch_ptr + n && *it == ch)
    {
        return nd_ptr[it - ch_ptr];
    }
    return nullptr;
}

void TrieNode::add_child(QChar ch, TrieNode* node)
{
    if (child_count == 0)
    {
        single_child_char = ch;
        first_child = node;
        child_count = 1;
        return;
    }

    if (child_count == 1)
    {
        if (single_child_char == ch)
        {
            first_child = node;
            return;
        }

        constexpr size_t initial_cap = 2;
        const size_t size_bytes = ChildHeader::allocation_size(initial_cap);
        void* mem = operator new(size_bytes);
        auto* header = new(mem) ChildHeader;
        header->capacity = static_cast<uint16_t>(initial_cap);
        header->count = 2;

        auto* ch_ptr = header->chars();
        auto* nd_ptr = header->nodes();

        if (single_child_char < ch)
        {
            ch_ptr[0] = single_child_char;
            nd_ptr[0] = first_child;
            ch_ptr[1] = ch;
            nd_ptr[1] = node;
        }
        else
        {
            ch_ptr[0] = ch;
            nd_ptr[0] = node;
            ch_ptr[1] = single_child_char;
            nd_ptr[1] = first_child;
        }

        first_child = reinterpret_cast<TrieNode*>(header);
        child_count = 2;
        return;
    }

    auto* header = reinterpret_cast<ChildHeader*>(first_child);

    if (header->count == header->capacity)
    {
        const size_t new_cap = header->capacity * 2;
        const size_t size_bytes = ChildHeader::allocation_size(new_cap);

        void* mem = operator new(size_bytes);
        auto* new_header = new(mem) ChildHeader;
        new_header->capacity = static_cast<uint16_t>(new_cap);
        new_header->count = header->count;

        std::memcpy(new_header->chars(), header->chars(), header->count * sizeof(QChar));
        std::memcpy(new_header->nodes(), header->nodes(), header->count * sizeof(TrieNode*));

        operator delete(header);
        first_child = reinterpret_cast<TrieNode*>(new_header);
        header = new_header;
    }

    auto* ch_ptr = header->chars();
    auto* nd_ptr = header->nodes();
    const uint16_t n = header->count;

    const auto it = std::lower_bound(ch_ptr, ch_ptr + n, ch);
    const size_t idx = it - ch_ptr;

    if (idx < n && ch_ptr[idx] == ch)
    {
        nd_ptr[idx] = node;
        return;
    }

    if (idx < n)
    {
        std::memmove(ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(QChar));
        std::memmove(nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(TrieNode*));
    }

    ch_ptr[idx] = ch;
    nd_ptr[idx] = node;
    header->count++;
    child_count = header->count;
}

QString* TrieNode::get_name() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_NAME) return reinterpret_cast<QString*>(ptr_val);
    if (tag == TAG_COMPLEX)
    {
        const auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->name.get();
    }
    return nullptr;
}

const QString* TrieNode::get_first_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_PHRASE) return reinterpret_cast<const QString*>(ptr_val);
    if (tag == TAG_MULTI_PHRASE)
    {
        const auto* list = reinterpret_cast<const QStringList*>(ptr_val);
        return list->isEmpty() ? nullptr : &list->first();
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* c = reinterpret_cast<const NodeData*>(ptr_val);
        if (c->single_phrase) return c->single_phrase.get();
        if (c->phrases && !c->phrases->isEmpty()) return &c->phrases->first();
    }
    return nullptr;
}

QStringList* TrieNode::get_phrases() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_MULTI_PHRASE) return reinterpret_cast<QStringList*>(ptr_val);
    if (tag == TAG_COMPLEX)
    {
        const auto* c = reinterpret_cast<NodeData*>(ptr_val);
        return c->phrases.get();
    }
    return nullptr;
}

std::vector<Rule>* TrieNode::get_rules() const
{
    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        return &c->rules;
    }
    return nullptr;
}

void TrieNode::ensure_complex()
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_COMPLEX) return;

    auto* complex = new NodeData();

    if (tag == TAG_NAME)
    {
        complex->name = std::make_unique<QString>(*reinterpret_cast<QString*>(ptr_val));
        *reinterpret_cast<QString*>(ptr_val) = QString();
    }
    else if (tag == TAG_PHRASE)
    {
        complex->single_phrase = std::make_unique<QString>(*reinterpret_cast<QString*>(ptr_val));
        *reinterpret_cast<QString*>(ptr_val) = QString();
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        complex->phrases = std::unique_ptr<QStringList>(reinterpret_cast<QStringList*>(ptr_val));
    }

    data = reinterpret_cast<uintptr_t>(complex) | TAG_COMPLEX;
}

void TrieNode::set_name(const QString& value)
{
    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_NAME;
        return;
    }

    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NAME)
    {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = value;
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name = std::make_unique<QString>(value);
    }
}

void TrieNode::set_name_ptr(QString* ptr)
{
    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
        return;
    }

    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NAME)
    {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = *ptr;
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name = std::make_unique<QString>(*ptr);
    }
}

void TrieNode::set_single_phrase(const QString& value)
{
    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_PHRASE;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = value;
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        delete reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_PHRASE;
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases.reset();
        c->single_phrase = std::make_unique<QString>(value);
    }
}

void TrieNode::set_single_phrase_ptr(QString* ptr)
{
    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = *ptr;
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        delete reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->phrases.reset();
        c->single_phrase = std::make_unique<QString>(*ptr);
    }
}

void TrieNode::add_phrase(const QString& value)
{
    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(new QString(value)) | TAG_PHRASE;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        auto* old_str = reinterpret_cast<QString*>(data & ~TAG_MASK);
        if (*old_str == value) return;
        auto* list = new QStringList();
        list->append(value);
        list->append(*old_str);
        *old_str = QString();
        data = reinterpret_cast<uintptr_t>(list) | TAG_MULTI_PHRASE;
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        auto* list = reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        list->removeAll(value);
        list->prepend(value);
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        if (!c->phrases && !c->single_phrase)
        {
            c->single_phrase = std::make_unique<QString>(value);
        }
        else if (c->single_phrase)
        {
            if (*c->single_phrase != value)
            {
                c->phrases = std::make_unique<QStringList>();
                c->phrases->append(value);
                c->phrases->append(*c->single_phrase);
                c->single_phrase.reset();
            }
        }
        else
        {
            c->phrases->removeAll(value);
            c->phrases->prepend(value);
        }
    }
}

void TrieNode::set_phrases(const QStringList& list_val)
{
    if (list_val.isEmpty())
    {
        remove_phrases();
        return;
    }

    if (list_val.size() == 1)
    {
        set_single_phrase(list_val.first());
        return;
    }

    if (data == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(new QStringList(list_val)) | TAG_MULTI_PHRASE;
        return;
    }

    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        *reinterpret_cast<QString*>(data & ~TAG_MASK) = QString();
        data = reinterpret_cast<uintptr_t>(new QStringList(list_val)) | TAG_MULTI_PHRASE;
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        *reinterpret_cast<QStringList*>(data & ~TAG_MASK) = list_val;
    }
    else
    {
        ensure_complex();
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->single_phrase.reset();
        c->phrases = std::make_unique<QStringList>(list_val);
    }
}

void TrieNode::add_rule(const Rule& rule)
{
    ensure_complex();
    auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
    c->rules.push_back(rule);

    std::ranges::sort(c->rules, [](const Rule& a, const Rule& b)
    {
        return a.original_end.length() > b.original_end.length();
    });
}

void TrieNode::remove_name()
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NAME)
    {
        auto* str = reinterpret_cast<QString*>(data & ~TAG_MASK);
        *str = QString();
        data = TAG_NULL;
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->name.reset();
    }
}

void TrieNode::remove_phrases()
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        auto* str = reinterpret_cast<QString*>(data & ~TAG_MASK);
        *str = QString();
        data = TAG_NULL;
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        delete reinterpret_cast<QStringList*>(data & ~TAG_MASK);
        data = TAG_NULL;
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<NodeData*>(data & ~TAG_MASK);
        c->single_phrase.reset();
        c->phrases.reset();
    }
}

Dictionary::Dictionary()
{
    root = pool.allocate();
}

Dictionary::~Dictionary()
{
    if (root)
    {
        auto destroy_recursive = [&](auto&& self, TrieNode* n) -> void
        {
            if (!n) return;
            if (n->child_count == 1)
            {
                self(self, n->first_child);
            }
            else if (n->child_count > 1 && n->first_child)
            {
                const auto* h = reinterpret_cast<const ChildHeader*>(n->first_child);
                const auto* nd_ptr = h->nodes();
                for (int i = 0; i < h->count; ++i)
                {
                    self(self, nd_ptr[i]);
                }
            }
            n->~TrieNode();
        };
        destroy_recursive(destroy_recursive, root);
    }
}

Dictionary::Dictionary(Dictionary&& other) noexcept
    : root(other.root), pool(std::move(other.pool)), string_pool(std::move(other.string_pool))
{
    other.root = nullptr;
}

Dictionary& Dictionary::operator=(Dictionary&& other) noexcept
{
    if (this != &other)
    {
        if (root)
        {
            auto destroy_recursive = [&](auto&& self, TrieNode* n) -> void
            {
                if (!n) return;
                if (n->child_count == 1)
                {
                    self(self, n->first_child);
                }
                else if (n->child_count > 1 && n->first_child)
                {
                    const auto* h = reinterpret_cast<const ChildHeader*>(n->first_child);
                    const auto* nd_ptr = h->nodes();
                    for (int i = 0; i < h->count; ++i)
                    {
                        self(self, nd_ptr[i]);
                    }
                }
                n->~TrieNode();
            };
            destroy_recursive(destroy_recursive, root);
        }

        pool = std::move(other.pool);
        string_pool = std::move(other.string_pool);
        root = other.root;

        other.root = nullptr;
    }
    return *this;
}

void Dictionary::insert(const QString& key, const QString& value, const Priority priority)
{
    TrieNode* node = root;
    for (const QChar ch : key)
    {
        TrieNode* next = node->find_child(ch);
        if (!next)
        {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME)
    {
        node->set_name_ptr(string_pool.allocate(value));
    }
    else
    {
        node->add_phrase(value);
    }
}

void Dictionary::insert_bulk(const QString& key, const Priority priority, const QString& value)
{
    TrieNode* node = root;
    for (const QChar ch : key)
    {
        TrieNode* next = node->find_child(ch);
        if (!next)
        {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    if (priority == NAME)
    {
        node->set_name_ptr(string_pool.allocate(value));
    }
    else
    {
        if (value.contains(u'\x1F'))
        {
            QStringList list = value.split(u'\x1F');
            node->set_phrases(list);
        }
        else
        {
            node->set_single_phrase_ptr(string_pool.allocate(value));
        }
    }
}

ExactResult Dictionary::find_exact(const QStringView& key) const
{
    const TrieNode* node = walk_node(key);

    if (!node)
    {
        return {};
    }

    const uintptr_t tag = node->data & TAG_MASK;
    const uintptr_t ptr_val = node->data & ~TAG_MASK;

    ExactResult result;
    if (tag == TAG_NAME)
    {
        result.name = reinterpret_cast<QString*>(ptr_val);
    }
    else if (tag == TAG_PHRASE)
    {
        result.single_phrase = reinterpret_cast<QString*>(ptr_val);
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        result.phrases = reinterpret_cast<QStringList*>(ptr_val);
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<NodeData*>(ptr_val);
        result.name = c->name.get();
        result.single_phrase = c->single_phrase.get();
        result.phrases = c->phrases.get();
    }
    return result;
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

    for (int i = startPos; i < text.length(); ++i)
    {
        const QChar ch = text[i];

        node = node->find_child(ch);
        if (!node) break;

        if (auto* r = node->get_rules())
        {
            rules = r;
        }

        if (auto* name = node->get_name())
        {
            best_len_found = i - startPos + 1;
            translated = name;
            priority = NAME;
        }
        else if (const auto* phrase = node->get_first_phrase())
        {
            if (i - startPos + 1 > best_len_found)
            {
                best_len_found = i - startPos + 1;
                translated = phrase;
                priority = PHRASE;
            }
        }
    }

    return {.length = best_len_found, .priority = priority, .rules = rules, .translation = translated};
}

void Dictionary::insert_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end)
{
    TrieNode* node = root;
    for (const QChar ch : start)
    {
        TrieNode* next = node->find_child(ch);
        if (!next)
        {
            next = pool.allocate();
            node->add_child(ch, next);
        }
        node = next;
    }

    node->add_rule({
        .original_start = start, .original_end = end, .translation_start = t_start, .translation_end = t_end
    });
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
            return &*it;
        }
    }
    return nullptr;
}

void Dictionary::remove_rule(const QString& start, const QString& end) const
{
    const TrieNode* node = walk_node(start);
    if (!node) return;

    if (auto* rules = node->get_rules())
    {
        const auto it = std::ranges::remove_if(*rules, [&](const Rule& r)
        {
            return r.translation_end == end;
        }).begin();

        if (it != rules->end())
        {
            rules->erase(it, rules->end());
        }
    }
}

void Dictionary::edit_rule(const QString& start, const QString& end, const QString& t_start, const QString& t_end) const
{
    const TrieNode* node = walk_node(start);
    if (!node) return;

    if (auto* rules = node->get_rules())
    {
        const auto it = std::ranges::find_if(*rules, [&](const Rule& r)
        {
            return r.original_end == end;
        });

        if (it != rules->end())
        {
            it->translation_start = t_start;
            it->translation_end = t_end;
        }
    }
}

TrieNode* Dictionary::walk_node(const QStringView& key) const
{
    TrieNode* node = root;
    for (const QChar ch : key)
    {
        node = node->find_child(ch);
        if (!node) return nullptr;
    }
    return node;
}

void Dictionary::remove(const QString& key, const Priority priority) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (priority == NAME)
    {
        node->remove_name();
    }
    else if (priority == PHRASE)
    {
        node->remove_phrases();
    }
}

void Dictionary::remove_meaning(const QString& key, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    const uintptr_t tag = node->data & TAG_MASK;
    const uintptr_t ptr_val = node->data & ~TAG_MASK;

    if (tag == TAG_PHRASE)
    {
        if (*reinterpret_cast<QString*>(ptr_val) == value)
        {
            node->remove_phrases();
        }
    }
    else if (tag == TAG_MULTI_PHRASE)
    {
        auto* list = reinterpret_cast<QStringList*>(ptr_val);
        list->removeAll(value);
        if (list->isEmpty())
        {
            node->remove_phrases();
        }
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<NodeData*>(ptr_val);
        if (c->single_phrase && *c->single_phrase == value)
        {
            c->single_phrase.reset();
        }
        if (c->phrases)
        {
            c->phrases->removeAll(value);
            if (c->phrases->isEmpty())
            {
                c->phrases.reset();
            }
        }
    }
}
