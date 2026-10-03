[sgcl](../../README.md) › [txt](../README.md) › [word_breaks](../word_breaks/README.md)

# sgcl::txt::word_breaks::iterator

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class word_breaks {
    public:
        class iterator;
    };
}
```

`txt::word_breaks::iterator` is the forward iterator of [word_breaks](../word_breaks/README.md): its `*` is a segment between
two word boundaries, a slice of the text, found when the iterator reaches it, and it knows the byte position of that
element and its size in bytes, for the code that goes back to the bytes: a slice of the text around it, a search from
it, a position to report.

## Rules

- An iterator holds a slice of the text and a position in it, not the range: it is valid while the text's object
  lives, which the range, the string and every slice of it keep.
- `*` returns the element by value, `slice<const char>`: there is no element in memory to refer to.
- Two iterators are equal when they are at the same byte position.

## Member types

| Type | Definition |
|---|---|
| `iterator_category` | `std::forward_iterator_tag` |
| `value_type` | `slice<const char>` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `slice<const char>` |
| `pointer` | `void` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](../word_breaks/begin.md) and [end](../word_breaks/end.md) |
| `operator*` | the element at the position, `slice<const char>` |
| `operator++` | moves past the element, to the next one |
| `operator==` | checks whether two iterators are at the same byte position |

#### Observers

| Function | Description |
|---|---|
| [pos](pos.md) | the byte position of the element in the text |
| [size](size.md) | the bytes the element takes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "can't stop";
    txt::word_breaks parts(s);
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+5 5+1 6+4 
```

## See also

- [sgcl::txt::word_breaks](../word_breaks/README.md)
