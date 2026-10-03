[sgcl](../README.md) › [txt](README.md) › [sentences](sentences.md)

# sgcl::txt::sentences::iterator

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class sentences {
    public:
        class iterator;
    };
}
```

`txt::sentences::iterator` is the forward iterator of [sentences](sentences.md): its `*` is a sentence, a slice of the
text, found when the iterator reaches it, and it knows the byte position of that element and its size in bytes, for
the code that goes back to the bytes: a slice of the text around it, a search from it, a position to report.

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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](sentences/begin.md) and [end](sentences/end.md) |
| `operator*` | the element at the position, `slice<const char>` |
| `operator++` | moves past the element, to the next one |
| `operator==` | checks whether two iterators are at the same byte position |

#### Observers

| Function | Description |
|---|---|
| [pos](sentences-iterator/pos.md) | the byte position of the element in the text |
| [size](sentences-iterator/size.md) | the bytes the element takes |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "One. Two!  Three?";
    txt::sentences all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+5 5+6 11+6 
```

## See also

- [sgcl::txt::sentences](sentences.md)
