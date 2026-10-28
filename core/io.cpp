#include "io.h"
#include "db.h"
#include "dict.h"
#include "unicode_utils.h"

namespace hanvi {

std::vector<std::u16string_view> paginate(const std::u16string_view input_text, const int min_length)
{
    std::vector<std::u16string_view> pages;
    size_t cursor = 0;
    const size_t length = input_text.length();

    while (cursor < length)
    {
        const size_t target_end = cursor + min_length;

        if (target_end >= length)
        {
            pages.push_back(input_text.substr(cursor));
            break;
        }
        size_t cutoff = input_text.find(u'\n', target_end);
        if (cutoff == std::u16string_view::npos)
        {
            pages.push_back(input_text.substr(cursor));
            break;
        }
        else
        {
            const size_t chunk_len = cutoff + 1 - cursor;
            pages.push_back(input_text.substr(cursor, chunk_len));

            cursor = cutoff + 1;
        }
    }

    return pages;
}

void io_insert(const int id, const std::u16string_view key, const std::u16string_view value, const Priority priority)
{
    if (id == -1)
    {
        dictionary.insert(key, value, priority);
        db_insert(key, value, priority);
    }
    else
    {
        if (id == current_name_set_id) name_set_dictionary.insert(key, value, priority);
        nameset_db_insert(id, key, value);
    }
}

void io_reorder(const std::u16string_view key, const std::vector<std::u16string>& new_order)
{
    dictionary.reorder(key, new_order);
    db_reorder(key, new_order);
}

void io_remove(const int id, const std::u16string_view key, const Priority priority)
{
    if (id == -1)
    {
        dictionary.remove(key, priority);
        db_remove(key, priority);
    }
    else
    {
        if (id == current_name_set_id) name_set_dictionary.remove(key, priority);
        nameset_db_remove(id, key);
    }
}

void io_remove_meaning(const std::u16string_view key, const std::u16string_view value)
{
    dictionary.remove_meaning(key, value);
    db_remove_meaning(key, value);
}

void io_insert_variant(const std::u16string_view original, const std::u16string_view canonical)
{
    if (original.empty() || canonical.empty()) return;
    char32_t cp = original[0];
    if (is_high_surrogate(original[0]) && original.size() >= 2 && is_low_surrogate(original[1]))
    {
        cp = to_code_point(original[0], original[1]);
    }
    variants.insert_or_assign(cp, std::u16string(canonical));
    db_insert_variant(original, canonical);
}

void io_remove_variant(const std::u16string_view original)
{
    if (original.empty()) return;
    char32_t cp = original[0];
    if (is_high_surrogate(original[0]) && original.size() >= 2 && is_low_surrogate(original[1]))
    {
        cp = to_code_point(original[0], original[1]);
    }
    variants.erase(cp);
    db_remove_variant(original);
}

std::optional<int> io_create_name_set(const std::u16string_view title)
{
    const auto id = nameset_db_create(title);
    if (id) name_sets.emplace_back(*id, std::u16string(title));
    return id;
}

std::optional<int> io_import_name_set(const std::u16string_view title, const std::vector<Entry>& entries)
{
    const auto id = nameset_db_import(title, entries);
    if (id) name_sets.emplace_back(*id, std::u16string(title));
    return id;
}

bool io_rename_name_set(const int id, const std::u16string_view title)
{
    if (!nameset_db_rename(id, title)) return false;
    for (auto& name_set : name_sets)
    {
        if (name_set.index == id) name_set.title = title;
    }
    return true;
}

void io_delete_name_set(const int id)
{
    nameset_db_delete(id);
    std::erase_if(name_sets, [id](const NameSet& name_set) { return name_set.index == id; });
    if (id == current_name_set_id) load_name_set(-1);
}

} // namespace hanvi
