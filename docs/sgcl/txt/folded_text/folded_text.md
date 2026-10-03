[sgcl](../../README.md) › [txt](../README.md) › [folded_text](../folded_text.md)

# sgcl::txt::folded_text::folded_text

```cpp
folded_text() noexcept;                                                              // (1)
explicit folded_text(const string& text) noexcept;                                   // (2)
explicit folded_text(const slice<const char>& text) noexcept;                        // (3)
template<size_t N> explicit folded_text(const char (&text)[N]);                      // (4)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
explicit folded_text(P text);                                                        // (5)
```

Maps a text once: a `folded_text` folds it by the full folding, a `normalized_text` decomposes it and puts its
marks in canonical order. The constructors of `normalized_text` are the same.

1. An empty text: it answers as a text built from `""`.
2. The text of a string, held as a slice of it.
3. The bytes of a slice, held as that slice.
4. An array of `char` up to its first NUL or its end, copied into a string the object holds: the positions are
   slices of it.
5. The characters at a pointer up to their NUL, copied into a string the object holds.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Complexity

- (1) Constant.
- (2–5) Linear in the length of the text.

## Exceptions

- (1–3) None.
- (4–5) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string line = "Grüße aus Köln";
    txt::folded_text folded(line);
    txt::normalized_text decomposed(line);
    println("{} bytes, {} folded code points, {} decomposed", line.size(), folded.size(),
            decomposed.size());
}
```

Output:

```text
17 bytes, 15 folded code points, 16 decomposed
```

## See also

- [size](size.md): the code points the text mapped to
- [sgcl::txt::folded_text, normalized_text](../folded_text.md)
