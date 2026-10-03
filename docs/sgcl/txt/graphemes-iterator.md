[sgcl](../README.md) › [txt](README.md) › [graphemes](graphemes.md)

# sgcl::txt::graphemes::iterator

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class graphemes {
    public:
        class iterator;
    };
}
```

`txt::graphemes::iterator` is the forward iterator of [graphemes](graphemes.md): its `*` is a grapheme cluster, a
slice of the text, found when the iterator reaches it, and it knows the byte position of that element and its size in
bytes, for the code that goes back to the bytes: a slice of the text around it, a search from it, a position to
report.

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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](graphemes/begin.md) and [end](graphemes/end.md) |
| `operator*` | the element at the position, `slice<const char>` |
| `operator++` | moves past the element, to the next one |
| `operator==` | checks whether two iterators are at the same byte position |

#### Observers

| Function | Description |
|---|---|
| [pos](graphemes-iterator/pos.md) | the byte position of the element in the text |
| [size](graphemes-iterator/size.md) | the bytes the element takes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "e\u0301\U0001F1F5\U0001F1F1!";
    txt::graphemes all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+3 3+8 11+1 
```

## See also

- [sgcl::txt::graphemes](graphemes.md)
