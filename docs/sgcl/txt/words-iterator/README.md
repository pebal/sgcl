[sgcl](../../README.md) › [txt](../README.md) › [words](../words/README.md)

# sgcl::txt::words::iterator

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class words {
    public:
        class iterator;
    };
}
```

`txt::words::iterator` is the forward iterator of [words](../words/README.md): its `*` is a word, a slice of the text, found
when the iterator reaches it, and it knows the byte position of that element and its size in bytes, for the code that
goes back to the bytes: a slice of the text around it, a search from it, a position to report.

## Rules

- An iterator holds a slice of the text and a position in it, not the range: it is valid while the text's object
  lives, which the range, the string and every slice of it keep.
- `*` returns the element by value, `slice<const char>`: there is no element in memory to refer to.
- Two iterators are equal when they are at the same byte position.
- A step passes over the segments without a letter or a digit: the iterator stands only on words.

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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](../words/begin.md) and [end](../words/end.md) |
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
    string s = "one, two";
    txt::words all(s);
    for (auto it = all.begin(); it != all.end(); ++it) {
        print("{}+{} ", it.pos(), it.size());
    }
    println();
}
```

Output:

```text
0+3 5+3 
```

## See also

- [sgcl::txt::words](../words/README.md)
