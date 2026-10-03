[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::operator[]

```cpp
const CharT& operator[](size_type i) const noexcept;
```

Returns the character at the position `i`, read only: a string is never modified. `i` may be `size()`, which gives the
terminator, `CharT()`, as `std::string` gives it.

A position past `size()` is undefined behaviour, as in `std`; a debug build asserts it. The checked access is `at`,
which throws `out_of_range` for a position not below `size()` ([mixin::text](../mixin/text/README.md)).

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the character, at most `size()` |

## Return value

A reference to the character, valid while some string holds the object.

## Complexity

Constant.

## Exceptions

None.

## Notes

In a `string` a position is a byte: `s[i]` of a UTF-8 text is a byte of a code point, which for a letter outside
ASCII is one of two to four. The code points are walked by `runes()` ([runes](../runes/README.md)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string word = "level";
    bool palindrome = true;
    for (int i : range(int(word.size()) / 2)) {
        palindrome = palindrome && word[i] == word[word.size() - 1 - i];
    }
    println("{} {} {}", word[0], palindrome, word[word.size()] == '\0');

    try {
        word.at(10);
    } catch (const out_of_range&) {
        println("out_of_range");
    }
}
```

Output:

```text
l true true
out_of_range
```

## See also

- [front](front.md), [back](back.md): the first and the last character
- [data](data.md): the characters, terminated
- [sgcl::string](README.md)
