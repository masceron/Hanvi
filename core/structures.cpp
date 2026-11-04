#include "structures.h"
#include "unicode_utils.h"
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
        const char* name = nullptr;
        const char* phrase = nullptr;
        std::vector<Rule> rules;
    };

    // TextArena::allocate puts the text's byte length, then its first meaning's, before it.
    constexpr size_t TEXT_HEADER = 2 * sizeof(uint16_t);

    ArenaText whole_text(const char* text)
    {
        uint16_t len;
        std::memcpy(&len, text - TEXT_HEADER, sizeof(len));
        return {text, len};
    }

    ArenaText first_meaning(const char* text)
    {
        uint16_t len;
        std::memcpy(&len, text - sizeof(uint16_t), sizeof(len));
        return {text, len};
    }

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

    std::u16string join_u16(const std::vector<std::u16string>& parts, const char16_t sep)
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

const char* TextArena::allocate(const std::u16string_view str)
{
    char* header = reserve(max_utf8_size(str.length()));
    char* text = header + TEXT_HEADER;
    const size_t sep = std::min(str.find(u'\x1F'), str.length());
    char* first_end = encode_utf8(str.substr(0, sep), text);
    return commit(header, first_end, encode_utf8(str.substr(sep), first_end));
}

const char* TextArena::allocate(const std::string_view utf8)
{
    char* header = reserve(utf8.size());
    char* text = header + TEXT_HEADER;
    if (!utf8.empty()) std::memcpy(text, utf8.data(), utf8.size());
    const size_t sep = std::min(utf8.find('\x1F'), utf8.size());
    return commit(header, text + sep, text + utf8.size());
}

// Room for a header and max_len bytes of text after it; returns where the header goes.
char* TextArena::reserve(const size_t max_len)
{
    // Texts start 4-byte aligned: trie nodes keep a tag in the pointer's low two bits.
    size_t offset = (current_block_offset + TEXT_HEADER + 3) & ~static_cast<size_t>(3);

    if (current_block_ptr == nullptr || offset + max_len > BLOCK_SIZE)
    {
        const size_t alloc_size = std::max(BLOCK_SIZE, TEXT_HEADER + max_len);
        // Every byte is written before it is read, so the block is not zeroed and its unused
        // tail never takes up memory.
        auto new_block = std::make_unique_for_overwrite<char[]>(alloc_size);
        current_block_ptr = new_block.get();
        blocks.push_back(std::move(new_block));
        offset = TEXT_HEADER;
    }
    return current_block_ptr + offset - TEXT_HEADER;
}

// Fills in the header reserve() made room for, once the text after it is written up to end.
const char* TextArena::commit(char* header, const char* first_end, const char* end)
{
    const char* text = header + TEXT_HEADER;
    const auto total_len = static_cast<uint16_t>(end - text);
    const auto first_len = static_cast<uint16_t>(first_end - text);
    std::memcpy(header, &total_len, sizeof(total_len));
    std::memcpy(header + sizeof(uint16_t), &first_len, sizeof(first_len));

    current_block_offset = static_cast<size_t>(end - current_block_ptr);
    return text;
}

namespace {
    std::u16string decode(const std::string_view text)
    {
        // UTF-8 never takes fewer bytes than UTF-16 takes units.
        std::u16string out;
        out.resize_and_overwrite(text.size(), [text](char16_t* buffer, size_t)
        {
            return static_cast<size_t>(decode_utf8(text, buffer) - buffer);
        });
        return out;
    }
}

std::u16string ArenaText::str() const
{
    return decode(text);
}

std::vector<std::u16string> ArenaText::split(const char sep) const
{
    std::vector<std::u16string> parts;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t pos = std::min(text.find(sep, start), text.size());
        parts.push_back(decode(text.substr(start, pos - start)));
        start = pos + 1;
    }
    return parts;
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
        std::memcpy(new_nd_ptr, nd_ptr, idx * sizeof(*nd_ptr));

        new_ch_ptr[idx] = ch;
        new_nd_ptr[idx] = node;

        std::memcpy(new_ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(char16_t));
        std::memcpy(new_nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(*nd_ptr));

        operator delete(header);
        first_child = reinterpret_cast<TrieNode*>(new_header);
        child_count = n + 1;
        return;
    }

    std::memmove(ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(char16_t));
    std::memmove(nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(*nd_ptr));

    ch_ptr[idx] = ch;
    nd_ptr[idx] = node;
    header->count = n + 1;
    child_count = n + 1;
}

ArenaText TrieNode::get_name() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_NAME)
    {
        return whole_text(reinterpret_cast<const char*>(data & ~TAG_MASK));
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK);
        if (comp->name)
        {
            return whole_text(comp->name);
        }
    }
    return {};
}

ArenaText TrieNode::get_first_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE) {
        return first_meaning(reinterpret_cast<const char*>(data & ~TAG_MASK));
    }
    if (tag == TAG_COMPLEX)
    {
        const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK);
        if (comp->phrase)
        {
            return first_meaning(comp->phrase);
        }
    }
    return {};
}

ArenaText TrieNode::get_full_phrase() const
{
    const uintptr_t tag = data & TAG_MASK;
    if (tag == TAG_PHRASE)
    {
        return whole_text(reinterpret_cast<const char*>(data & ~TAG_MASK));
    }
    if (tag == TAG_COMPLEX)
    {
        if (const auto* comp = reinterpret_cast<const ComplexNodeData*>(data & ~TAG_MASK); comp->phrase)
        {
            return whole_text(comp->phrase);
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
        comp->name = reinterpret_cast<const char*>(data & ~TAG_MASK);
    }
    else if (tag == TAG_PHRASE)
    {
        comp->phrase = reinterpret_cast<const char*>(data & ~TAG_MASK);
    }
    data = reinterpret_cast<uintptr_t>(comp) | TAG_COMPLEX;
}

void TrieNode::set_name_ptr(const char* ptr)
{
    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NULL)
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

void TrieNode::set_phrase_ptr(const char* ptr)
{
    if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NULL)
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

void Dictionary::insert(const std::u16string_view key, std::u16string_view value, const Priority priority)
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
        if (existing_full.isEmpty())
        {
            node->set_phrase_ptr(text_arena.allocate(value));
        }
        else
        {
            const auto list = existing_full.split('\x1F');
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

void Dictionary::insert_bulk(const std::u16string_view key, const Priority priority, const std::string_view value)
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

    const char* ptr = text_arena.allocate(value);
    if (priority == NAME)
    {
        node->set_name_ptr(ptr);
    }
    else
    {
        node->set_phrase_ptr(ptr);
    }
}

ExactResult Dictionary::find_exact(const std::u16string_view key) const
{
    const TrieNode* node = walk_node(key);
    if (!node) return {};

    ExactResult result;
    result.name = node->get_name();
    result.full_phrase = node->get_full_phrase();
    result.phrase = node->get_first_phrase();
    return result;
}

void Dictionary::reorder(const std::u16string_view key, const std::vector<std::u16string>& new_order) const
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

void Dictionary::remove_meaning(const std::u16string_view key, const std::u16string_view value) const
{
    TrieNode* node = walk_node(key);
    if (!node) return;

    const auto existing_full = node->get_full_phrase();
    if (existing_full.isEmpty()) return;

    const auto list = existing_full.split('\x1F');
    std::vector<std::u16string> remaining;
    remaining.reserve(list.size());
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

Match Dictionary::find(const std::u16string_view text, const int startPos) const
{
    const TrieNode* node = root;
    int best_len_found = 0;
    ArenaText translated;
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

void Dictionary::insert_rule(const std::u16string_view start, const std::u16string_view end, const std::u16string_view t_start, const std::u16string_view t_end)
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

const Rule* Dictionary::find_exact_rule(const std::u16string_view start, std::u16string_view end) const
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

void Dictionary::remove_rule(const std::u16string_view start, const std::u16string_view end) const {
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

void Dictionary::edit_rule(const std::u16string_view start, const std::u16string_view end, const std::u16string_view t_start, const std::u16string_view t_end) const
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

TrieNode* Dictionary::walk_node(const std::u16string_view key) const
{
    TrieNode* node = root;
    for (const char16_t ch : key)
    {
        node = node->find_child(ch);
        if (!node) return nullptr;
    }
    return node;
}

void Dictionary::remove(const std::u16string_view key, const Priority priority) const
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
