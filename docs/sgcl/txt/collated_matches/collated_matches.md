[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](README.md)

# sgcl::txt::collated_matches::collated_matches

```cpp
collated_matches() noexcept;                                                                   // (1)
collated_matches(const collator& by, const string& text, const string& pattern) noexcept;      // (2)
collated_matches(const collator& by, const string& text, searcher_type pattern) noexcept;      // (3)
collated_matches(const collator& by, const slice<const char>& text, searcher_type pattern);    // (4)
template<size_t N>
collated_matches(const collator& by, const char (&text)[N], searcher_type pattern);            // (5)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
collated_matches(const collator& by, P text, searcher_type pattern);                           // (6)
```

Constructs the range of the occurrences of a pattern in a text by a collator: weighs the text once and keeps it
with the pattern in one tracked object.

1. An empty range: an empty text and an empty pattern under the root collator, held as the other constructors
   hold theirs.
2. A text and a pattern as strings; the pattern is weighed here. A literal text with a literal pattern comes here.
3. A text as a string and a pattern weighed once before; one weighed by another collator is weighed again here,
   with `by`.
4. A piece of a text, kept as a copy of that piece: the positions the range answers with are the piece's own.
5. An array of `char` up to its first NUL or its end, copied into a string.
6. The characters at a pointer up to their NUL, copied into a string.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the collator whose equality the search counts by |
| `text` | the text to search, UTF-8 |
| `pattern` | the pattern to look for |

## Complexity

- (1) Constant.
- (2–6) Linear in the length of the text, and of the pattern where it is weighed here.

## Exceptions

- (1–3) None.
- (4–6) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string/README.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator primary{txt::strength::primary};
    txt::collated_searcher lodz(primary, "lodz");
    for (string line : {"Łódź, Lodz", "do Łodzi", "LÓDŹ"}) {
        print("{} ", txt::collated_matches(primary, line, lodz).count());
    }
    println();
}
```

Output:

```text
2 1 1 
```

## See also

- [begin](begin.md): the first occurrence
- [sgcl::txt::collated_matches](README.md)
