[sgcl](../../README.md) › [txt](../README.md) › [words](../words.md)

# sgcl::txt::words::words

```cpp
/*(1)*/ words() noexcept = default;
/*(2)*/ explicit words(const slice<const char>& text) noexcept;
/*(3)*/ explicit words(const string& text) noexcept;
/*(4)*/ template<size_t N>
        explicit words(const char (&text)[N]);
/*(5)*/ template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        explicit words(P text);
```

Constructs the range of the words of a text. `txt::words(s)` looks like a call and is a construction, as
[runes](../../core/runes.md) is. Nothing is found until the walk.

1. An empty range, over no text.
2. The words of a slice of UTF-8 bytes, a piece of a buffer as much as a piece of a string; the range keeps the slice,
   and with it the object the bytes lie in.
3. The words of a string; the range holds a slice of it, and with it the string's object.
4. The words of an array of `char`, a literal among them, read up to its first NUL or its end, never past it, and
   copied into a string the range holds.
5. The words of a C text, `const char*` or `char*`, read up to its NUL and copied likewise.

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
    txt::words none;
    println("{}", none.empty());

    string line = "title=Pan Tadeusz";
    txt::words title(line.as_slice(6));
    println("{} words", title.count());
}
```

Output:

```text
true
2 words
```

## See also

- [text](text.md): the slice the range walks
- [sgcl::txt::words](../words.md)
