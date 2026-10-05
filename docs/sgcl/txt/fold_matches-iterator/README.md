[sgcl](../../README.md) › [txt](../README.md) › [fold_matches](../fold_matches/README.md)

# sgcl::txt::fold_matches::iterator

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class fold_matches {
    public:
        class iterator;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::fold_matches::iterator` is the forward iterator of [fold_matches](../fold_matches/README.md), and of
`normalized_matches` the same way: its `*` is the slice of the original text an occurrence covers, made when it is
asked for, and it knows the byte position of that occurrence and its size — which need not be the pattern's, since
folding and decomposing change lengths.

## Rules

- An iterator holds the tracked object with the text, the pattern and the mapping, not the range: it stays valid
  after the range is gone, and a copy of it is independent.
- `*` returns the slice by value: there is no element in memory to refer to.
- Two iterators are equal when they stand at the same occurrence, or both at the end.

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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](../fold_matches/begin.md) and [end](../fold_matches/end.md) |
| `operator*` | the slice of the text the occurrence covers |
| `operator++` | moves to the next occurrence, looked for past the end of this one |
| `operator==` | checks whether two iterators stand at the same occurrence |

#### Observers

| Function | Description |
|---|---|
| [pos](pos.md) | the byte position of the occurrence in the text |
| [size](size.md) | the bytes of the text the occurrence covers |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Maße: MASSE, masse";
    auto matches = txt::fold_matches(text, "masse");
    for (auto it = matches.begin(); it != matches.end(); ++it) {
        println("{}..{} [{}]", it.pos(), it.pos() + it.size(), *it);
    }
}
```

Output:

```text
0..5 [Maße]
7..12 [MASSE]
14..19 [masse]
```

## See also

- [occurrence](../occurrence.md): the same two numbers from a single search
- [sgcl::txt::fold_matches, normalized_matches](../fold_matches/README.md)
