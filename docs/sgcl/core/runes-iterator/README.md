[sgcl](../../README.md) › [core](../README.md) › [runes](../runes/README.md)

# sgcl::runes::iterator

```cpp
#include "sgcl/core/slice.h"   // or "sgcl/core.h"

namespace sgcl {
    class runes {
    public:
        class iterator;
    };
}
```

`sgcl::runes::iterator` is the forward iterator of [runes](../runes/README.md): its `*` is the code point at its position,
decoded when the iterator reaches it, and it knows that position in bytes and the bytes the code point takes. The two
are what Go's `for i, r := range s` gives beside the rune: the way back from a code point to the bytes of the text,
for a slice of it, a search from it, or a position to report.

## Rules

- An iterator holds a view of the text and a position in it, not the range: it is valid while the text's object
  lives, which the range, the string and every slice of it keep.
- `*` returns the code point by value, `char32_t`: there is no element in memory to refer to.
- Two iterators are equal when they are at the same byte position.

## Member types

| Type | Definition |
|---|---|
| `iterator_category` | `std::forward_iterator_tag` |
| `value_type` | `char32_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `char32_t` |
| `pointer` | `void` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](../runes/begin.md) and [end](../runes/end.md) |
| `operator*` | the code point at the position, `char32_t`; `utf8::replacement` for an invalid byte |
| `operator++` | moves past the code point, by its width in bytes |
| `operator==` | checks whether two iterators are at the same byte position |

#### Observers

| Function | Description |
|---|---|
| [pos](pos.md) | the byte position of the code point in the text |
| [width](width.md) | the bytes the code point takes |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "zażółć gęślą jaźń";
    vector<string_slice> marked;
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        if (*it > 0x7F) {
            marked.push_back(s.as_slice(it.pos(), it.width()));  // the bytes of the letter
        }
    }
    println("{}", marked);
}
```

Output:

```text
["ż", "ó", "ł", "ć", "ę", "ś", "ą", "ź", "ń"]
```

## See also

- [utf8::decode](../utf8/decode.md): what a step does
- [sgcl::runes](../runes/README.md)
