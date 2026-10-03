[sgcl](../README.md) › [txt](README.md) › [line_breaks](line_breaks.md)

# sgcl::txt::line_breaks::iterator

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class line_breaks {
    public:
        class iterator;
    };
}
```

`txt::line_breaks::iterator` is the forward iterator of [line_breaks](line_breaks.md): its `*` is a piece that must
stay together, a slice of the text, found when the iterator reaches it, and it knows the byte position of that element
and its size in bytes, for the code that goes back to the bytes: a slice of the text around it, a search from it, a
position to report.

## Rules

- An iterator holds a slice of the text and a position in it, not the range: it is valid while the text's object
  lives, which the range, the string and every slice of it keep.
- `*` returns the element by value, `slice<const char>`: there is no element in memory to refer to.
- Two iterators are equal when they are at the same byte position.
- It carries the state of the rules between two pieces (rule LB15a asks what stood before an opening quotation mark),
  so a copy of an iterator goes on from where it stands and an iterator is made only by [begin](line_breaks/begin.md)
  or [end](line_breaks/end.md).

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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](line_breaks/begin.md) and [end](line_breaks/end.md) |
| `operator*` | the element at the position, `slice<const char>` |
| `operator++` | moves past the element, to the next one |
| `operator==` | checks whether two iterators are at the same byte position |

#### Observers

| Function | Description |
|---|---|
| [pos](line_breaks-iterator/pos.md) | the byte position of the element in the text |
| [size](line_breaks-iterator/size.md) | the bytes the element takes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "one two-three";
    txt::line_breaks pieces(s);
    for (auto it = pieces.begin(); it != pieces.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+4 4+4 8+5 
```

## See also

- [sgcl::txt::line_breaks](line_breaks.md)
