[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::find

```cpp
optional<match> find(const slice<const char>& text, size_t from = 0) const noexcept;      // (1)
optional<match> find(const string& text, size_t from = 0) const noexcept;                 // (2)
template<size_t N> optional<match> find(const char (&text)[N], size_t from = 0) const;    // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
optional<match> find(P text, size_t from = 0) const;                                      // (4)
```

Finds the first match that begins at or after the byte `from`: the leftmost one, and of those beginning there the
one the order of the pattern prefers — the first branch of an alternation, a greedy quantifier taking what it can, a
lazy one what it must.

1. The text as a slice; the match holds it.
2. The text as a string; the match holds it.
3. An array of `char` up to its first NUL or its end, copied into a string the match holds.
4. The characters at a pointer up to their NUL, copied into a string the match holds.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `from` | the byte the search starts at; a byte inside a code point starts it at the next code point, since a match never begins inside one; past the end of the text there is no match |

## Return value

The [match](../match/README.md), or an empty `optional` when there is none at or after `from`.

## Complexity

Linear in the length of the text after `from` times the length of the pattern.

## Exceptions

- (1–2) None.
- (3–4) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string/README.md)
  holds.

## Notes

A match holds the text it was found in, so it outlives the string it came from and a match of a temporary is safe
to keep — which a `std::smatch` over a `std::string_view` is not. A loop over every match is [all](all.md), which
keeps one machine for the whole walk.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex price("(\\d+),(\\d{2}) zł");
    string text = "chleb 4,50 zł, masło 7,99 zł";
    auto first = price.find(text);
    println("{} at {}", first->text(), first->begin_at());
    auto second = price.find(text, first->end_at());
    println("{} złotych {} groszy", (*second)[1], (*second)[2]);
    println("{}", price.find(text, second->end_at()).has_value());
}
```

Output:

```text
4,50 zł at 6
7 złotych 99 groszy
false
```

## See also

- [all](all.md): every match, as a range
- [contains](contains.md): whether there is a match
- [match](../match/README.md): what a match holds
- [sgcl::txt::regex](README.md)
