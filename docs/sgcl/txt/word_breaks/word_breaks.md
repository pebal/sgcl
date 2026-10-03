[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks.md)

# sgcl::txt::word_breaks::word_breaks

```cpp
word_breaks() noexcept = default;                                  // (1)
explicit word_breaks(const slice<const char>& text) noexcept;      // (2)
explicit word_breaks(const string& text) noexcept;                 // (3)
template<size_t N>
explicit word_breaks(const char (&text)[N]);                       // (4)
template<class P>
requires std::same_as<P, const char*> || std::same_as<P, char*>
explicit word_breaks(P text);                                      // (5)
```

Constructs the range of the text cut at every word boundary. `txt::word_breaks(s)` looks like a call and is a
construction, as [runes](../../core/runes.md) is. Nothing is found until the walk.

1. An empty range, over no text.
2. The segments of a slice of UTF-8 bytes, a piece of a buffer as much as a piece of a string; the range keeps the
   slice, and with it the object the bytes lie in.
3. The segments of a string; the range holds a slice of it, and with it the string's object.
4. The segments of an array of `char`, a literal among them, read up to its first NUL or its end, never past it, and
   copied into a string the range holds.
5. The segments of a C text, `const char*` or `char*`, read up to its NUL and copied likewise.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Complexity

- (1–3) Constant.
- (4–5) Linear in the length of the text, which is copied.

## Exceptions

- (1–3) None.
- (4–5) `length_error` when the text is longer than the [max_size()](../../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::word_breaks none;
    println("{}", none.empty());

    string line = "key=3.14 e-mail";
    for (auto w : txt::word_breaks(line.as_slice(4))) {
        print("[{}]", w);
    }
    println();
}
```

Output:

```text
true
[3.14][ ][e][-][mail]
```

## See also

- [text](text.md): the slice the range walks
- [sgcl::txt::word_breaks](../word_breaks.md)
