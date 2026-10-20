#include "io.h"
#include "db.h"
#include "dict.h"

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

void io_insert(const int id, std::u16string_view key, std::u16string_view value, const Priority priority)
{
    if (id == -1)
    {
        dictionary.insert(key, value, priority);
        db_insert(key, value, priority);
    }
    else
    {
        name_set_dictionary.insert(key, value, priority);
        nameset_db_insert(key, value);
    }
}

void io_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order)
{
    dictionary.reorder(key, new_order);
    db_reorder(key, new_order);
}

void io_remove(const int id, std::u16string_view key, const Priority priority)
{
    if (id == -1)
    {
        dictionary.remove(key, priority);
        db_remove(key, priority);
    }
    else
    {
        name_set_dictionary.remove(key, priority);
        nameset_db_remove(key);
    }
}

void io_remove_meaning(std::u16string_view key, std::u16string_view value)
{
    dictionary.remove_meaning(key, value);
    db_remove_meaning(key, value);
}

} // namespace hanvi
