#include "structures.h"
#include <algorithm>
#include <cstring>
#include <ranges>

namespace hanvi {

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

    struct ChildHeader
    {
        uint16_t capacity;
        uint16_t count;

        [[nodiscard]] char16_t* chars()
        {
            return reinterpret_cast<char16_t*>(this + 1);
        }

        [[nodiscard]] const char16_t* chars() const
        {
            return reinterpret_cast<const char16_t*>(this + 1);
        }

        [[nodiscard]] TrieNode** nodes()
        {
            const size_t char_bytes = capacity * sizeof(char16_t);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
            return reinterpret_cast<TrieNode**>(reinterpret_cast<char*>(this) + aligned_offset);
        }

        [[nodiscard]] TrieNode* const* nodes() const
        {
            const size_t char_bytes = capacity * sizeof(char16_t);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
            return reinterpret_cast<TrieNode* const*>(reinterpret_cast<const char*>(this) + aligned_offset);
        }

        static size_t allocation_size(const size_t cap)
        {
            const size_t char_bytes = cap * sizeof(char16_t);
            const size_t aligned_offset = (sizeof(ChildHeader) + char_bytes + 7) & ~static_cast<size_t>(7);
            return aligned_offset + cap * sizeof(TrieNode*);
        }
    };

    std::vector<std::u16string_view> split_u16(std::u16string_view str, char16_t sep)
    {
        std::vector<std::u16string_view> result;
        size_t start = 0;
        while (start < str.size())
        {
            size_t pos = str.find(sep, start);
            if (pos == std::u16string_view::npos)
            {
                result.push_back(str.substr(start));
                break;
            }
            result.push_back(str.substr(start, pos - start));
            start = pos + 1;
        }
        return result;
    }

    std::u16string join_u16(const std::vector<std::u16string>& parts, char16_t sep)
    {
        if (parts.empty()) return {};
        std::u16string res = parts[0];
        for (size_t i = 1; i < parts.size(); ++i)
        {
            res.push_back(sep);
            res += parts[i];
        }
        return res;
    }
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

const char16_t* TextArena::allocate(std::u16string_view str)
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
        std::memcpy(current_block_ptr + offset, str.data(), total_len * sizeof(char16_t));
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

TrieNode* TrieNode::find_child(const char16_t ch) const
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

void TrieNode::add_child(const char16_t ch, TrieNode* node)
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

        constexpr uint16_t initial_cap = 4;
        auto* header = static_cast<ChildHeader*>(operator new(ChildHeader::allocation_size(initial_cap)));
        header->capacity = initial_cap;
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
    auto* ch_ptr = header->chars();
    auto* nd_ptr = header->nodes();
    const uint16_t n = header->count;

    const auto it = std::lower_bound(ch_ptr, ch_ptr + n, ch);
    const int idx = static_cast<int>(it - ch_ptr);

    if (it != ch_ptr + n && *it == ch)
    {
        nd_ptr[idx] = node;
        return;
    }

    if (n == header->capacity)
    {
        const uint16_t new_cap = header->capacity < 8 ? header->capacity * 2 : header->capacity + 8;
        auto* new_header = static_cast<ChildHeader*>(operator new(ChildHeader::allocation_size(new_cap)));
        new_header->capacity = new_cap;
        new_header->count = n + 1;

        auto* new_ch_ptr = new_header->chars();
        auto* new_nd_ptr = new_header->nodes();

        std::memcpy(new_ch_ptr, ch_ptr, idx * sizeof(char16_t));
        std::memcpy(new_nd_ptr, nd_ptr, idx * sizeof(TrieNode*));

        new_ch_ptr[idx] = ch;
        new_nd_ptr[idx] = node;

        std::memcpy(new_ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(char16_t));
        std::memcpy(new_nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(TrieNode*));

        operator delete(header);
        first_child = reinterpret_cast<TrieNode*>(new_header);
        child_count = n + 1;
        return;
    }

    std::memmove(ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(char16_t));
    std::memmove(nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(TrieNode*));

    ch_ptr[idx] = ch;
    nd_ptr[idx] = node;
    header->count = n + 1;
    child_count = n + 1;
}

StringView TrieNode::get_name() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NAME)
    {
        const auto* ptr = reinterpret_cast<const char16_t*>(data & ~TAG_MASK);
        const uint16_t len = ptr[-2];
        return StringView(ptr, len);
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK);
        if (comp->name)
        {
            const uint16_t len = comp->name[-2];
            return StringView(comp->name, len);
        }
    }
    return {};
}

StringView TrieNode::get_first_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        const auto* ptr = reinterpret_cast<const char16_t*>(data & ~TAG_MASK);
        const uint16_t first_len = ptr[-1];
        return StringView(ptr, first_len);
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK);
        if (comp->phrase)
        {
            const uint16_t first_len = comp->phrase[-1];
            return StringView(comp->phrase, first_len);
        }
    }
    return {};
}

StringView TrieNode::get_full_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        const auto* ptr = reinterpret_cast<const char16_t*>(data & ~TAG_MASK);
        const uint16_t total_len = ptr[-2];
        return StringView(ptr, total_len);
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK);
        if (comp->phrase)
        {
            const uint16_t total_len = comp->phrase[-2];
            return StringView(comp->phrase, total_len);
        }
    }
    return {};
}

std::vector<Rule>* TrieNode::get_rules() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_COMPLEX)
    {
        auto* comp = reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK);
        if (!comp->rules.empty()) return &comp->rules;
    }
    return nullptr;
}

void TrieNode::ensure_complex()
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_COMPLEX) return;

    auto* comp = new ComplexNodeData();
    if (tag == TAG_NAME)
    {
        comp->name = reinterpret_cast<const char16_t*>(data & ~TAG_MASK);
    }
    else if (tag == TAG_PHRASE)
    {
        comp->phrase = reinterpret_cast<const char16_t*>(data & ~TAG_MASK);
    }
    data = reinterpret_cast<uintptr_t>(comp) | TAG_COMPLEX;
}

void TrieNode::set_name_ptr(const char16_t* ptr)
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
    }
    else if (tag == TAG_NAME)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
    }
    else
    {
        ensure_complex();
        reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK)->name = ptr;
    }
}

void TrieNode::set_phrase_ptr(const char16_t* ptr)
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NULL)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
    }
    else if (tag == TAG_PHRASE)
    {
        data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
    }
    else
    {
        ensure_complex();
        reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK)->phrase = ptr;
    }
}

void TrieNode::add_rule(const Rule& rule)
{
    ensure_complex();
    reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK)->rules.push_back(rule);
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
        reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK)->name = nullptr;
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
        reinterpret_cast<ComplexNodeData*>(data & ~TAG_MASK)->phrase = nullptr;
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

void Dictionary::insert(std::u16string_view key, std::u16string_view value, const Priority priority)
{
    TrieNode* node = root;
    for (const char16_t ch : key)
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
        if (existing_full.empty())
        {
            node->set_phrase_ptr(text_arena.allocate(value));
        }
        else
        {
            const auto list = split_u16(existing_full, u'\x1F');
            std::vector<std::u16string> str_list;
            str_list.reserve(list.size() + 1);
            str_list.emplace_back(value);
            for (const auto& item : list)
            {
                if (item != value)
                {
                    str_list.emplace_back(item);
                }
            }
            const std::u16string combined = join_u16(str_list, u'\x1F');
            node->set_phrase_ptr(text_arena.allocate(combined));
        }
    }
}

void Dictionary::insert_bulk(std::u16string_view key, const Priority priority, std::u16string_view value)
{
    TrieNode* node = root;
    for (const char16_t ch : key)
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

ExactResult Dictionary::find_exact(std::u16string_view key) const
{
    const TrieNode* node = walk_node(key);
    if (!node) return {};

    ExactResult result;
    result.name = node->get_name();
    result.full_phrase = node->get_full_phrase();
    return result;
}

void Dictionary::reorder(std::u16string_view key, const std::vector<std::u16string>& new_order) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    if (new_order.empty())
    {
        node->remove_phrases();
        return;
    }

    const std::u16string combined = join_u16(new_order, u'\x1F');
    node->set_phrase_ptr(text_arena.allocate(combined));
}

void Dictionary::remove_meaning(std::u16string_view key, std::u16string_view value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    const auto existing_full = node->get_full_phrase();
    if (existing_full.empty()) return;

    const auto list = split_u16(existing_full, u'\x1F');
    std::vector<std::u16string> remaining;
    for (const auto& item : list)
    {
        if (item != value)
        {
            remaining.emplace_back(item);
        }
    }

    if (remaining.empty())
    {
        node->remove_phrases();
    }
    else
    {
        const std::u16string combined = join_u16(remaining, u'\x1F');
        node->set_phrase_ptr(text_arena.allocate(combined));
    }
}

Match Dictionary::find(std::u16string_view text, const int startPos) const
{
    const TrieNode* node = root;
    int best_len_found = 0;
    StringView translated;
    Priority priority = NONE;

    std::vector<Rule>* rules = nullptr;

    for (size_t i = startPos; i < text.length(); ++i)
    {
        const char16_t ch = text[i];

        node = node->find_child(ch);
        if (!node) break;

        if (auto* r = node->get_rules())
        {
            rules = r;
        }

        if (const auto name = node->get_name(); !name.isNull())
        {
            best_len_found = static_cast<int>(i - startPos + 1);
            translated = name;
            priority = NAME;
        }
        else if (const auto phrase = node->get_first_phrase(); !phrase.isNull())
        {
            if (static_cast<int>(i - startPos + 1) > best_len_found)
            {
                best_len_found = static_cast<int>(i - startPos + 1);
                translated = phrase;
                priority = PHRASE;
            }
        }
    }

    return {.length = best_len_found, .priority = priority, .rules = rules, .translation = translated};
}

void Dictionary::insert_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end)
{
    TrieNode* node = root;
    for (const char16_t ch : start)
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
        .original_start = std::u16string(start),
        .original_end = std::u16string(end),
        .translation_start = std::u16string(t_start),
        .translation_end = std::u16string(t_end)
    });
}

const Rule* Dictionary::find_exact_rule(std::u16string_view start, std::u16string_view end) const
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

void Dictionary::remove_rule(std::u16string_view start, std::u16string_view end)
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

void Dictionary::edit_rule(std::u16string_view start, std::u16string_view end, std::u16string_view t_start, std::u16string_view t_end) const
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

TrieNode* Dictionary::walk_node(std::u16string_view key) const
{
    TrieNode* node = root;
    for (const char16_t ch : key)
    {
        node = node->find_child(ch);
        if (!node) return nullptr;
    }
    return node;
}

void Dictionary::remove(std::u16string_view key, const Priority priority) const
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

} // namespace hanvi
