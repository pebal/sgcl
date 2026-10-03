[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::replace_first

```cpp
string replace_first(const string& text, const string& with) const;                          // (1)
string replace_first(const slice<const char>& text, const slice<const char>& with) const;    // (2)
template<size_t N, size_t M>
string replace_first(const char (&text)[N], const char (&with)[M]) const;                    // (3)
template<class P, class Q>
requires (std::same_as<P, const char*> || std::same_as<P, char*>)
      && (std::same_as<Q, const char*> || std::same_as<Q, char*>)
string replace_first(const P& text, const Q& with) const;                                    // (4)
```

Returns the text with the first match of the pattern replaced and the rest as it was. The replacement is read as
[replace](replace.md) reads it: `$1`, `${1}` and `${name}` for a group, `$0` for the whole match, `$$` for a
dollar, and nothing for a group the pattern does not have or that took no part.

1. The text and the replacement as strings.
2. The text and the replacement as slices.
3. Two arrays of `char`, each up to its first NUL or its end.
4. Two pointers, each up to its NUL. An array beside a pointer goes through `string`, (1).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `with` | the replacement, with `$` references to the groups |

## Return value

A new string: the text with its first match replaced, or a copy of the text when there is no match.

## Complexity

Linear in the length of the text times the length of the pattern.

## Exceptions

`length_error` when the result is longer than 4294967295 bytes, the most a [string](../../core/string.md) holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex first_word("^(\\w)(\\w*)");
    println("{}", first_word.replace_first("łódź i kraków", "[$1]$2"));
    println("{}", txt::regex("a").replace_first("banana", "A"));
}
```

Output:

```text
[ł]ódź i kraków
bAnana
```

## See also

- [replace](replace.md): every match replaced
- [sgcl::txt::regex](../regex.md)
