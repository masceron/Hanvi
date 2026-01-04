#include "include/convert.h"

#include "../match/include/trie.h"

void convert()
{

}

void tokenize()
{
    int i = 0;
    while (i < input.length()) {
        if (Match result = dictionary.find(input, i); result.length > 0) {
            output.append(result.translation);
            i += result.length;
        } else {
            output.append(sv_readings[input[i]]);
            i++;
        }
    }
}
