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

    struct ComplexNodeData {
        const char *name = nullptr;
        const char *phrase = nullptr;
        std::vector<Rule> rules;
    };

    namespace {
        // TextArena::allocate puts the text's byte length, then its first meaning's, before it.
        constexpr size_t TEXT_HEADER = 2 * sizeof(uint16_t);

        ArenaText whole_text(const char *text) {
            uint16_t len;
            std::memcpy(&len, text - TEXT_HEADER, sizeof(len));
            return {text, len};
        }

        ArenaText first_meaning(const char *text) {
            uint16_t len;
            std::memcpy(&len, text - sizeof(uint16_t), sizeof(len));
            return {text, len};
        }

        // A child array of n entries: n chars, padding, then n node pointers.
        size_t child_nodes_offset(const size_t n) {
            return (n * sizeof(char16_t) + alignof(TrieNode *) - 1) & ~(alignof(TrieNode *) - 1);
        }

        size_t child_array_size(const size_t n) {
            return child_nodes_offset(n) + n * sizeof(TrieNode *);
        }

        char16_t *child_chars(void *array) {
            return static_cast<char16_t *>(array);
        }

        TrieNode **child_nodes(void *array, const size_t n) {
            return reinterpret_cast<TrieNode **>(static_cast<char *>(array) + child_nodes_offset(n));
        }

        std::u16string join_u16(const std::vector<std::u16string> &parts, const char16_t sep) {
            if (parts.empty()) return {};
            std::u16string res = parts[0];
            for (size_t i = 1; i < parts.size(); ++i) {
                res.push_back(sep);
                res += parts[i];
            }
            return res;
        }
    }

    TrieNode *NodePool::allocate() {
        return new(allocate_bytes(sizeof(TrieNode))) TrieNode();
    }

    void *NodePool::allocate_bytes(size_t size) {
        static_assert(alignof(TrieNode *) <= alignof(TrieNode));
        size = (size + alignof(TrieNode) - 1) & ~(alignof(TrieNode) - 1);

        // Blocks are not zeroed: nodes are constructed and child arrays written in full.
        if (size > BLOCK_SIZE) {
            // The root's array can be larger than a block; it gets its own.
            return blocks.emplace_back(std::make_unique_for_overwrite<char[]>(size)).get();
        }
        if (current_block_offset + size > BLOCK_SIZE) {
            auto new_block = std::make_unique_for_overwrite<char[]>(BLOCK_SIZE);
            current_block_ptr = new_block.get();
            blocks.push_back(std::move(new_block));
            current_block_offset = 0;
        }

        void *ptr = current_block_ptr + current_block_offset;
        current_block_offset += size;
        return ptr;
    }

    ComplexNodeData *NodePool::allocate_complex() {
        return complex_data.emplace_back(new ComplexNodeData());
    }

    void NodePool::clear() {
        for (const ComplexNodeData *data: complex_data) delete data;
        complex_data.clear();
        blocks.clear();
        current_block_offset = BLOCK_SIZE;
        current_block_ptr = nullptr;
    }

    NodePool::~NodePool() {
        clear();
    }

    const char *TextArena::allocate(const std::u16string_view str) {
        char *header = reserve(max_utf8_size(str.length()));
        char *text = header + TEXT_HEADER;
        const size_t sep = std::min(str.find(u'\x1F'), str.length());
        char *first_end = encode_utf8(str.substr(0, sep), text);
        return commit(header, first_end, encode_utf8(str.substr(sep), first_end));
    }

    const char *TextArena::allocate(const std::string_view utf8) {
        char *header = reserve(utf8.size());
        char *text = header + TEXT_HEADER;
        if (!utf8.empty()) std::memcpy(text, utf8.data(), utf8.size());
        const size_t sep = std::min(utf8.find('\x1F'), utf8.size());
        return commit(header, text + sep, text + utf8.size());
    }

    // Room for a header and max_len bytes of text after it; returns where the header goes.
    char *TextArena::reserve(const size_t max_len) {
        // Texts start 4-byte aligned: trie nodes keep a tag in the pointer's low two bits.
        size_t offset = (current_block_offset + TEXT_HEADER + 3) & ~static_cast<size_t>(3);

        if (current_block_ptr == nullptr || offset + max_len > BLOCK_SIZE) {
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
    const char *TextArena::commit(char *header, const char *first_end, const char *end) {
        const char *text = header + TEXT_HEADER;
        const auto total_len = static_cast<uint16_t>(end - text);
        const auto first_len = static_cast<uint16_t>(first_end - text);
        std::memcpy(header, &total_len, sizeof(total_len));
        std::memcpy(header + sizeof(uint16_t), &first_len, sizeof(first_len));

        current_block_offset = static_cast<size_t>(end - current_block_ptr);
        return text;
    }

    namespace {
        std::u16string decode(const std::string_view text) {
            // UTF-8 never takes fewer bytes than UTF-16 takes units.
            std::u16string out;
            out.resize_and_overwrite(text.size(), [text](char16_t *buffer, size_t) {
                return static_cast<size_t>(decode_utf8(text, buffer) - buffer);
            });
            return out;
        }
    }

    std::u16string ArenaText::str() const {
        return decode(text);
    }

    std::vector<std::u16string> ArenaText::split(const char sep) const {
        std::vector<std::u16string> parts;
        size_t start = 0;
        while (start < text.size()) {
            const size_t pos = std::min(text.find(sep, start), text.size());
            parts.push_back(decode(text.substr(start, pos - start)));
            start = pos + 1;
        }
        return parts;
    }

    void TextArena::clear() {
        blocks.clear();
        current_block_offset = BLOCK_SIZE;
        current_block_ptr = nullptr;
    }

    TrieNode *TrieNode::find_child(const char16_t ch) const {
        if (child_count == 1) {
            return single_child_char == ch ? first_child : nullptr;
        }
        if (child_count == 0) {
            return nullptr;
        }

        const uint16_t n = child_count;
        const auto *ch_ptr = child_chars(first_child);
        const auto *nd_ptr = child_nodes(first_child, n);

        if (n <= 8) {
            for (uint16_t i = 0; i < n; ++i) {
                if (ch_ptr[i] == ch) return nd_ptr[i];
            }
            return nullptr;
        }

        const auto it = std::lower_bound(ch_ptr, ch_ptr + n, ch);
        if (it != ch_ptr + n && *it == ch) {
            return nd_ptr[it - ch_ptr];
        }
        return nullptr;
    }

    void TrieNode::add_child(const char16_t ch, TrieNode *node, NodePool &pool) {
        if (child_count == 0) {
            single_child_char = ch;
            first_child = node;
            child_count = 1;
            return;
        }

        // A single child is held in the node itself; viewed here as an array of one.
        const uint16_t n = child_count;
        char16_t *ch_ptr = n == 1 ? &single_child_char : child_chars(first_child);
        TrieNode **nd_ptr = n == 1 ? &first_child : child_nodes(first_child, n);

        const auto idx = static_cast<size_t>(std::lower_bound(ch_ptr, ch_ptr + n, ch) - ch_ptr);
        if (idx < n && ch_ptr[idx] == ch) {
            nd_ptr[idx] = node;
            return;
        }

        void *array = pool.allocate_bytes(child_array_size(n + 1));
        auto *new_ch_ptr = child_chars(array);
        auto *new_nd_ptr = child_nodes(array, n + 1);

        std::memcpy(new_ch_ptr, ch_ptr, idx * sizeof(char16_t));
        std::memcpy(new_nd_ptr, nd_ptr, idx * sizeof(TrieNode *));
        new_ch_ptr[idx] = ch;
        new_nd_ptr[idx] = node;
        std::memcpy(new_ch_ptr + idx + 1, ch_ptr + idx, (n - idx) * sizeof(char16_t));
        std::memcpy(new_nd_ptr + idx + 1, nd_ptr + idx, (n - idx) * sizeof(TrieNode *));

        first_child = static_cast<TrieNode *>(array);
        child_count = n + 1;
    }

    void TrieNode::set_children(const std::span<const std::pair<char16_t, TrieNode *>> children, NodePool &pool) {
        const size_t n = children.size();
        if (n == 0) return;
        if (n == 1) {
            single_child_char = children[0].first;
            first_child = children[0].second;
            child_count = 1;
            return;
        }

        void *array = pool.allocate_bytes(child_array_size(n));
        auto *ch_ptr = child_chars(array);
        auto *nd_ptr = child_nodes(array, n);
        for (size_t i = 0; i < n; ++i) {
            ch_ptr[i] = children[i].first;
            nd_ptr[i] = children[i].second;
        }
        first_child = static_cast<TrieNode *>(array);
        child_count = static_cast<uint16_t>(n);
    }

    ArenaText TrieNode::get_name() const {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_NAME) {
            return whole_text(reinterpret_cast<const char *>(data & ~TAG_MASK));
        }
        if (tag == TAG_COMPLEX) {
            const auto *comp = reinterpret_cast<const ComplexNodeData *>(data & ~TAG_MASK);
            if (comp->name) {
                return whole_text(comp->name);
            }
        }
        return {};
    }

    ArenaText TrieNode::get_first_phrase() const {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_PHRASE) {
            return first_meaning(reinterpret_cast<const char *>(data & ~TAG_MASK));
        }
        if (tag == TAG_COMPLEX) {
            const auto *comp = reinterpret_cast<const ComplexNodeData *>(data & ~TAG_MASK);
            if (comp->phrase) {
                return first_meaning(comp->phrase);
            }
        }
        return {};
    }

    ArenaText TrieNode::get_full_phrase() const {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_PHRASE) {
            return whole_text(reinterpret_cast<const char *>(data & ~TAG_MASK));
        }
        if (tag == TAG_COMPLEX) {
            if (const auto *comp = reinterpret_cast<const ComplexNodeData *>(data & ~TAG_MASK); comp->phrase) {
                return whole_text(comp->phrase);
            }
        }
        return {};
    }

    std::vector<Rule> *TrieNode::get_rules() const {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_COMPLEX) {
            auto *comp = reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK);
            if (!comp->rules.empty()) return &comp->rules;
        }
        return nullptr;
    }

    void TrieNode::ensure_complex(NodePool &pool) {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_COMPLEX) return;

        auto *comp = pool.allocate_complex();
        if (tag == TAG_NAME) {
            comp->name = reinterpret_cast<const char *>(data & ~TAG_MASK);
        } else if (tag == TAG_PHRASE) {
            comp->phrase = reinterpret_cast<const char *>(data & ~TAG_MASK);
        }
        data = reinterpret_cast<uintptr_t>(comp) | TAG_COMPLEX;
    }

    void TrieNode::set_name_ptr(const char *ptr, NodePool &pool) {
        if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NULL) {
            data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
        } else if (tag == TAG_NAME) {
            data = reinterpret_cast<uintptr_t>(ptr) | TAG_NAME;
        } else {
            ensure_complex(pool);
            reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK)->name = ptr;
        }
    }

    void TrieNode::set_phrase_ptr(const char *ptr, NodePool &pool) {
        if (const uintptr_t tag = data & TAG_MASK; tag == TAG_NULL) {
            data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
        } else if (tag == TAG_PHRASE) {
            data = reinterpret_cast<uintptr_t>(ptr) | TAG_PHRASE;
        } else {
            ensure_complex(pool);
            reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK)->phrase = ptr;
        }
    }

    void TrieNode::add_rule(const Rule &rule, NodePool &pool) {
        ensure_complex(pool);
        reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK)->rules.push_back(rule);
    }

    void TrieNode::remove_name() {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_NAME) {
            data = TAG_NULL;
        } else if (tag == TAG_COMPLEX) {
            reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK)->name = nullptr;
        }
    }

    void TrieNode::remove_phrases() {
        const uintptr_t tag = data & TAG_MASK;
        if (tag == TAG_PHRASE) {
            data = TAG_NULL;
        } else if (tag == TAG_COMPLEX) {
            reinterpret_cast<ComplexNodeData *>(data & ~TAG_MASK)->phrase = nullptr;
        }
    }

    Dictionary::Dictionary() {
        root = pool.allocate();
    }

    // Nodes need no destructor: the pool frees them with everything they point to.
    Dictionary::~Dictionary() = default;

    Dictionary::Dictionary(Dictionary &&other) noexcept
        : root(other.root), root_index(std::move(other.root_index)), pool(std::move(other.pool)),
          text_arena(std::move(other.text_arena)) {
        other.root = nullptr;
    }

    Dictionary &Dictionary::operator=(Dictionary &&other) noexcept {
        if (this != &other) {
            pool = std::move(other.pool);
            text_arena = std::move(other.text_arena);
            root_index = std::move(other.root_index);
            root = other.root;

            other.root = nullptr;
        }
        return *this;
    }

    void Dictionary::insert(const std::u16string_view key, std::u16string_view value, const Priority priority) {
        TrieNode *node = make_path(root, key);

        if (priority == NAME) {
            node->set_name_ptr(text_arena.allocate(value), pool);
        } else {
            const auto existing_full = node->get_full_phrase();
            if (existing_full.isEmpty()) {
                node->set_phrase_ptr(text_arena.allocate(value), pool);
            } else {
                const auto list = existing_full.split('\x1F');
                std::vector<std::u16string> str_list;
                str_list.reserve(list.size() + 1);
                str_list.emplace_back(value);
                for (const auto &item: list) {
                    if (item != value) {
                        str_list.emplace_back(item);
                    }
                }
                const std::u16string combined = join_u16(str_list, u'\x1F');
                node->set_phrase_ptr(text_arena.allocate(combined), pool);
            }
        }
    }

    TrieNode *Dictionary::make_path(TrieNode *node, const std::u16string_view key) {
        for (const char16_t ch: key) {
            TrieNode *next = node->find_child(ch);
            if (!next) {
                next = pool.allocate();
                node->add_child(ch, next, pool);
                if (node == root && root_index) root_index[ch] = next;
            }
            node = next;
        }
        return node;
    }

    void Dictionary::set_bulk_value(TrieNode *node, const Priority priority, const std::string_view value) {
        const char *ptr = text_arena.allocate(value);
        if (priority == NAME) {
            node->set_name_ptr(ptr, pool);
        } else {
            node->set_phrase_ptr(ptr, pool);
        }
    }

    DictionaryLoader::DictionaryLoader(Dictionary &dictionary) : dict(dictionary) {
        path.push_back(dict.root);
        pending.emplace_back();
        // Only an empty dictionary can be built a node at a time.
        in_order = dict.root->child_count == 0;
    }

    DictionaryLoader::~DictionaryLoader() {
        finish();
    }

    void DictionaryLoader::add(const std::string_view key_utf8, const Priority priority, const std::string_view value) {
        key16.resize_and_overwrite(key_utf8.size(), [key_utf8](char16_t *buffer, size_t) {
            return static_cast<size_t>(decode_utf8(key_utf8, buffer) - buffer);
        });
        const std::u16string_view key = key16;

        if (!in_order) {
            dict.set_bulk_value(dict.make_path(dict.root, key), priority, value);
            return;
        }

        const auto common = static_cast<size_t>(std::ranges::mismatch(key, previous).in1 - key.begin());
        if (compare_code_points(key, previous) < 0) {
            add_out_of_order(key, common, priority, value);
            return;
        }

        close_deeper_than(common);
        for (size_t d = common; d < key.size(); ++d) {
            TrieNode *child = dict.pool.allocate();
            pending[d].emplace_back(key[d], child);
            path.push_back(child);
            if (pending.size() <= d + 1) pending.emplace_back();
        }
        dict.set_bulk_value(path[key.size()], priority, value);
        previous = key;
    }

    // A key before the previous one (SQLite sorts unpaired surrogates apart from UTF-16) goes in
    // beside the nodes still being built, which stay open. Its own nodes below path[common] are
    // finished ones, or new, so it is inserted into them one child at a time.
    void DictionaryLoader::add_out_of_order(const std::u16string_view key, const size_t common, const Priority priority,
                                            const std::string_view value) {
        TrieNode *node = path[common];
        size_t d = common;
        if (d < key.size()) {
            auto &children = pending[d];
            if (const auto it = std::ranges::find(children, key[d], &std::pair<char16_t, TrieNode *>::first);
                it != children.end()) {
                node = it->second;
            } else {
                node = dict.pool.allocate();
                children.emplace_back(key[d], node);
            }
            ++d;
        }
        dict.set_bulk_value(dict.make_path(node, key.substr(d)), priority, value);
    }

    void DictionaryLoader::finish() {
        if (!in_order) return;
        in_order = false;
        close_deeper_than(0);
        auto &children = pending[0];
        std::ranges::sort(children, {}, &std::pair<char16_t, TrieNode *>::first);
        dict.root->set_children(children, dict.pool);
        if (dict.root->child_count >= ROOT_INDEX_MIN_CHILDREN) dict.index_root();
    }

    void DictionaryLoader::close_deeper_than(const size_t depth) {
        while (path.size() > depth + 1) {
            const size_t d = path.size() - 1;
            auto &children = pending[d];
            // Code point order puts surrogates after U+E000-U+FFFF; the arrays need code unit order.
            std::ranges::sort(children, {}, &std::pair<char16_t, TrieNode *>::first);
            path[d]->set_children(children, dict.pool);
            children.clear();
            path.pop_back();
        }
    }

    ExactResult Dictionary::find_exact(const std::u16string_view key) const {
        const TrieNode *node = walk_node(key);
        if (!node) return {};

        ExactResult result;
        result.name = node->get_name();
        result.full_phrase = node->get_full_phrase();
        result.phrase = node->get_first_phrase();
        return result;
    }

    void Dictionary::reorder(const std::u16string_view key, const std::vector<std::u16string> &new_order) {
        TrieNode *node = walk_node(key);
        if (!node) return;

        if (new_order.empty()) {
            node->remove_phrases();
            return;
        }

        const std::u16string combined = join_u16(new_order, u'\x1F');
        node->set_phrase_ptr(text_arena.allocate(combined), pool);
    }

    void Dictionary::remove_meaning(const std::u16string_view key, const std::u16string_view value) {
        TrieNode *node = walk_node(key);
        if (!node) return;

        const auto existing_full = node->get_full_phrase();
        if (existing_full.isEmpty()) return;

        const auto list = existing_full.split('\x1F');
        std::vector<std::u16string> remaining;
        remaining.reserve(list.size());
        for (const auto &item: list) {
            if (item != value) {
                remaining.emplace_back(item);
            }
        }

        if (remaining.empty()) {
            node->remove_phrases();
        } else {
            const std::u16string combined = join_u16(remaining, u'\x1F');
            node->set_phrase_ptr(text_arena.allocate(combined), pool);
        }
    }

    Match Dictionary::find(const std::u16string_view text, const int startPos) const {
        const TrieNode *node = root;
        int best_len_found = 0;
        ArenaText translated;
        Priority priority = NONE;

        std::vector<Rule> *rules = nullptr;

        for (size_t i = startPos; i < text.length(); ++i) {
            const char16_t ch = text[i];

            node = node == root ? root_child(ch) : node->find_child(ch);
            if (!node) break;

            if (auto *r = node->get_rules()) {
                rules = r;
            }

            if (const auto name = node->get_name(); !name.isNull()) {
                best_len_found = static_cast<int>(i - startPos + 1);
                translated = name;
                priority = NAME;
            } else if (const auto phrase = node->get_first_phrase(); !phrase.isNull()) {
                if (static_cast<int>(i - startPos + 1) > best_len_found) {
                    best_len_found = static_cast<int>(i - startPos + 1);
                    translated = phrase;
                    priority = PHRASE;
                }
            }
        }

        return {.length = best_len_found, .priority = priority, .rules = rules, .translation = translated};
    }

    void Dictionary::insert_rule(const std::u16string_view start, const std::u16string_view end,
                                 const std::u16string_view t_start, const std::u16string_view t_end) {
        TrieNode *node = make_path(root, start);

        node->add_rule({
                           .original_start = std::u16string(start),
                           .original_end = std::u16string(end),
                           .translation_start = std::u16string(t_start),
                           .translation_end = std::u16string(t_end)
                       }, pool);
    }

    const Rule *Dictionary::find_exact_rule(const std::u16string_view start, std::u16string_view end) const {
        const TrieNode *node = walk_node(start);
        if (!node) return nullptr;

        if (const auto *rules = node->get_rules()) {
            const auto &rule_vector = *rules;
            const auto it = std::ranges::find_if(rule_vector.begin(), rule_vector.end(), [end](const Rule &r) {
                return r.original_end == end;
            });
            if (it != rule_vector.end()) {
                return &*it;
            }
        }
        return nullptr;
    }

    void Dictionary::remove_rule(const std::u16string_view start, const std::u16string_view end) const {
        const TrieNode *node = walk_node(start);
        if (!node) return;

        if (auto *rules = node->get_rules()) {
            const auto it = std::ranges::remove_if(*rules, [&](const Rule &r) {
                return r.original_end == end;
            }).begin();

            if (it != rules->end()) {
                rules->erase(it, rules->end());
            }
        }
    }

    void Dictionary::edit_rule(const std::u16string_view start, const std::u16string_view end,
                               const std::u16string_view t_start, const std::u16string_view t_end) const {
        const TrieNode *node = walk_node(start);
        if (!node) return;

        if (auto *rules = node->get_rules()) {
            const auto it = std::ranges::find_if(*rules, [&](const Rule &r) {
                return r.original_end == end;
            });

            if (it != rules->end()) {
                it->translation_start = t_start;
                it->translation_end = t_end;
            }
        }
    }

    TrieNode *Dictionary::walk_node(const std::u16string_view key) const {
        TrieNode *node = root;
        for (const char16_t ch: key) {
            node = node == root ? root_child(ch) : node->find_child(ch);
            if (!node) return nullptr;
        }
        return node;
    }

    TrieNode *Dictionary::root_child(const char16_t ch) const {
        return root_index ? root_index[ch] : root->find_child(ch);
    }

    void Dictionary::index_root() {
        // Every char16_t value: 512 KB with 64-bit pointers, 256 KB in WebAssembly.
        root_index = std::make_unique<TrieNode *[]>(size_t{1} << 16);
        for (unsigned ch = 0; ch < (1u << 16); ++ch) {
            root_index[ch] = root->find_child(static_cast<char16_t>(ch));
        }
    }

    void Dictionary::remove(const std::u16string_view key, const Priority priority) const {
        TrieNode *node = walk_node(key);
        if (!node) return;

        if (priority == NAME) {
            node->remove_name();
        } else if (priority == PHRASE) {
            node->remove_phrases();
        }
    }
} // namespace hanvi
