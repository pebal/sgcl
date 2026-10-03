[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches.md)

# sgcl::txt::fold_matches::fold_matches

```cpp
fold_matches() noexcept;                                                             // (1)
fold_matches(const string& text, const string& pattern) noexcept;                    // (2)
fold_matches(const string& text, searcher_type pattern) noexcept;                    // (3)
fold_matches(const slice<const char>& text, searcher_type pattern);                  // (4)
template<size_t N> fold_matches(const char (&text)[N], searcher_type pattern);       // (5)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
fold_matches(P text, searcher_type pattern);                                         // (6)
```

Constructs the range of the occurrences of a pattern in a text: maps the text once — folds it for `fold_matches`,
decomposes it for `normalized_matches`, whose constructors are the same — and keeps it with the text and the
pattern in one tracked object.

1. An empty range: an empty text and an empty pattern, held as the other constructors hold theirs.
2. A text and a pattern as strings; the pattern is mapped here. A literal text with a literal pattern comes here.
3. A text as a string and a pattern mapped once before, a [fold_searcher](../fold_searcher.md) or a
   [normalized_searcher](../fold_searcher.md).
4. A piece of a text, kept as that piece, which copies those bytes: the positions the range answers with are the
   piece's own.
5. An array of `char` up to its first NUL or its end, copied into a string the range holds.
6. The characters at a pointer up to their NUL, copied into a string the range holds.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the pattern to look for |

## Complexity

- (1) Constant.
- (2–6) Linear in the length of the text, and (2) of the pattern.

## Exceptions

- (1–3) None.
- (4–6) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher ss("SS");
    for (string t : {"Straße", "Klasse", "Rose"}) {
        print("{} ", txt::fold_matches(t, ss).count());
    }
    println();
    string log = "błąd: X; BŁĄD: Y";
    for (auto m : txt::fold_matches(log.as_slice(6), txt::fold_searcher("błąd"))) {
        println("[{}]", m);
    }
}
```

Output:

```text
1 1 0 
[BŁĄD]
```

## See also

- [begin](begin.md): the first occurrence
- [sgcl::txt::fold_matches, normalized_matches](../fold_matches.md)
