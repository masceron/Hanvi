#include "structures.h"
#include <algorithm>
#include <ranges>

static constexpr uintptr_t TAG_MASK = 0x3;
static constexpr uintptr_t TAG_NULL = 0x0;
static constexpr uintptr_t TAG_NAME = 0x1;
static constexpr uintptr_t TAG_PHRASE = 0x2;
static constexpr uintptr_t TAG_COMPLEX = 0x3;

namespace {
    struct ComplexNodeData
    {
        const char16_t* name = nullptr;
        const char16_t* phrase = nullptr;
        std::vector<Rule> rules;
    };
}

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
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
            return reinterpret_cast<TrieNode**>(reinterpret_cast<char*>(this) + aligned_offset);
        }

        [[nodiscard]] TrieNode* const* nodes() const
        {
            const size_t char_bytes = capacity * sizeof(QChar);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
            return reinterpret_cast<TrieNode* const*>(reinterpret_cast<const char*>(this) + aligned_offset);
        }

        static size_t allocation_size(const size_t cap)
        {
            const size_t char_bytes = cap * sizeof(QChar);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
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

const char16_t* TextArena::allocate(const QStringView& str)
{
    const size_t len = str.length();
    int sep = -1;
    for (size_t i = 0; i < len; ++i)
    {
        if (str[i] == u'\x1F')
        {
            sep = static_cast<int>(i);
            break;
        }
    }
    const uint16_t first_len = sep >= 0 ? static_cast<uint16_t>(sep) : static_cast<uint16_t>(len);
    const auto total_len = static_cast<uint16_t>(len);

    size_t offset = (current_block_offset + 2 + 3) & ~static_cast<size_t>(3);

    if (current_block_ptr == nullptr || offset + total_len + 1 > BLOCK_SIZE)
    {
        const size_t alloc_size = std::max(BLOCK_SIZE, static_cast<size_t>(8 + total_len + 1));
        auto new_block = std::make_unique<char16_t[]>(alloc_size);
        current_block_ptr = new_block.get();
        blocks.push_back(std::move(new_block));
        offset = 4;
    }

    current_block_ptr[offset - 2] = static_cast<char16_t>(total_len);
    current_block_ptr[offset - 1] = static_cast<char16_t>(first_len);
    if (total_len > 0)
    {
        std::memcpy(current_block_ptr + offset, str.utf16(), total_len * sizeof(char16_t));
    }
    current_block_ptr[offset + total_len] = u'\0';

    current_block_offset = offset + total_len + 1;
    return current_block_ptr + offset;
}

void TextArena::clear()
{
    blocks.clear();
    current_block_offset = BLOCK_SIZE;
    current_block_ptr = nullptr;
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
    if (tag == TAG_COMPLEX)
    {
        delete reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
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

void TrieNode::add_child(const QChar ch, TrieNode* node)
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

QStringView TrieNode::get_name() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_NAME)
    {
        const auto* raw = reinterpret_cast<const char16_t*>(ptr_val);
        const uint16_t total_len = raw[-2];
        return {raw, total_len};
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* c = reinterpret_cast<const ComplexNodeData*>(ptr_val);
        if (c->name)
        {
            const uint16_t total_len = c->name[-2];
            return {c->name, total_len};
        }
    }
    return {};
}

QStringView TrieNode::get_first_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_PHRASE)
    {
        const auto* raw = reinterpret_cast<const char16_t*>(ptr_val);
        const uint16_t first_len = raw[-1];
        return {raw, first_len};
    }
    if (tag == TAG_COMPLEX)
    {
        if (const auto* c = reinterpret_cast<const ComplexNodeData*>(ptr_val); c->phrase)
        {
            const uint16_t first_len = c->phrase[-1];
            return {c->phrase, first_len};
        }
    }
    return {};
}

QStringView TrieNode::get_full_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_PHRASE)
    {
        const auto* raw = reinterpret_cast<const char16_t*>(ptr_val);
        const uint16_t total_len = raw[-2];
        return {raw, total_len};
    }
    if (tag == TAG_COMPLEX)
    {
        if (const auto* c = reinterpret_cast<const ComplexNodeData*>(ptr_val); c->phrase)
        {
            const uint16_t total_len = c->phrase[-2];
            return {c->phrase, total_len};
        }
    }
    return {};
}

std::vector<Rule>* TrieNode::get_rules() const
{
    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
        return &c->rules;
    }
    return nullptr;
}

void TrieNode::ensure_complex()
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_COMPLEX) return;

    auto* complex = new ComplexNodeData();
    const uintptr_t ptr_val = data & ~TAG_MASK;

    if (tag == TAG_NAME)
    {
        complex->name = reinterpret_cast<const char16_t*>(ptr_val);
    }
    else if (tag == TAG_PHRASE)
    {
        complex->phrase = reinterpret_cast<const char16_t*>(ptr_val);
    }

    data = reinterpret_cast<uintptr_t>(complex) | TAG_COMPLEX;
}

void TrieNode::set_name_ptr(const char16_t* ptr)
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NULL || tag == TAG_NAME)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
        return;
    }

    ensure_complex();
    auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
    c->name = ptr;
}

void TrieNode::set_phrase_ptr(const char16_t* ptr)
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NULL || tag == TAG_PHRASE)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
        return;
    }

    ensure_complex();
    auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
    c->phrase = ptr;
}

void TrieNode::add_rule(const Rule& rule)
{
    ensure_complex();
    auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
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
        data = TAG_NULL;
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
        c->name = nullptr;
    }
}

void TrieNode::remove_phrases()
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        data = TAG_NULL;
    }
    else if (tag == TAG_COMPLEX)
    {
        auto* c = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
        c->phrase = nullptr;
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
    : root(other.root), pool(std::move(other.pool)), text_arena(std::move(other.text_arena))
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
        text_arena = std::move(other.text_arena);
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
        node->set_name_ptr(text_arena.allocate(value));
    }
    else
    {
        const auto existing_full = node->get_full_phrase();
        if (existing_full.isNull() || existing_full.isEmpty())
        {
            node->set_phrase_ptr(text_arena.allocate(value));
        }
        else
        {
            const auto list = existing_full.split(u'\x1F');
            QStringList str_list;
            str_list.reserve(list.size() + 1);
            str_list.append(value);
            for (const auto& item : list)
            {
                if (item != value)
                {
                    str_list.append(item.toString());
                }
            }
            const QString combined = str_list.join(u'\x1F');
            node->set_phrase_ptr(text_arena.allocate(combined));
        }
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

    const char16_t* ptr = text_arena.allocate(value);
    if (priority == NAME)
    {
        node->set_name_ptr(ptr);
    }
    else
    {
        node->set_phrase_ptr(ptr);
    }
}

ExactResult Dictionary::find_exact(const QStringView& key) const
{
    const TrieNode* node = walk_node(key);
    if (!node) return {};

    ExactResult result;
    result.name = node->get_name();
    result.full_phrase = node->get_full_phrase();
    return result;
}

void Dictionary::reorder(const QString& key, const QStringList& new_order) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (new_order.isEmpty())
    {
        node->remove_phrases();
        return;
    }

    const QString combined = new_order.join(u'\x1F');
    node->set_phrase_ptr(text_arena.allocate(combined));
}

void Dictionary::remove_meaning(const QString& key, const QString& value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    const auto existing_full = node->get_full_phrase();
    if (existing_full.isNull() || existing_full.isEmpty()) return;

    const auto list = existing_full.split(u'\x1F');
    QStringList remaining;
    for (const auto& item : list)
    {
        if (item != value)
        {
            remaining.append(item.toString());
        }
    }

    if (remaining.isEmpty())
    {
        node->remove_phrases();
    }
    else
    {
        const QString combined = remaining.join(u'\x1F');
        node->set_phrase_ptr(text_arena.allocate(combined));
    }
}

Match Dictionary::find(const QStringView& text, const int startPos) const
{
    const TrieNode* node = root;
    int best_len_found = 0;
    QStringView translated;
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

        if (const auto name = node->get_name(); !name.isNull())
        {
            best_len_found = i - startPos + 1;
            translated = name;
            priority = NAME;
        }
        else if (const auto phrase = node->get_first_phrase(); !phrase.isNull())
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
            return r.original_end == end;
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
