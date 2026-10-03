[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::all

```cpp
regex_matches all(const slice<const char>& text) const noexcept;                     // (1)
regex_matches all(const string& text) const noexcept;                                // (2)
template<size_t N> regex_matches all(const char (&text)[N]) const;                   // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
regex_matches all(P text) const;                                                     // (4)
```

Returns every match of the pattern in the text, one after another and never overlapping, as a range of the library
([regex_matches](../regex_matches/README.md)): decided as it is walked rather than gathered into a container first, like
[words](../words/README.md) and [graphemes](../graphemes/README.md). Python's `re.finditer`, Go's `FindAllStringSubmatchIndex`.

1. The text as a slice; the range holds it.
2. The text as a string; the range holds it.
3. An array of `char` up to its first NUL or its end, copied into a string the range holds.
4. The characters at a pointer up to their NUL, copied into a string the range holds.

The next match is looked for from the end of the last. A match of no width — what `x*` or `\b` finds — moves the
search on by one code point, or the walk would stand still.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

The range of the matches.

## Complexity

Constant: nothing is searched until the range is walked. A walk is linear in the length of the text times the
length of the pattern.

## Exceptions

- (1–2) None.
- (3–4) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string/README.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex tag("#(\\w+)");
    for (const auto& m : tag.all("#zima w #Zakopanem, #góry")) {
        print("{}@{} ", m[1], m.begin_at());
    }
    println();
    for (const auto& m : txt::regex("x*").all("aż")) {
        print("[{}-{}]", m.begin_at(), m.end_at());
    }
    println();
}
```

Output:

```text
zima@0 Zakopanem@8 góry@20 
[0-0][1-1][3-3]
```

## See also

- [regex_matches](../regex_matches/README.md): the range
- [find](find.md): the first match
- [count](count.md): the number of matches
- [sgcl::txt::regex](README.md)
