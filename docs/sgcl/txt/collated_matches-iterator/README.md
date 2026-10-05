[sgcl](../../README.md) › [txt](../README.md) › [collated_matches](../collated_matches/README.md)

# sgcl::txt::collated_matches::iterator

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collated_matches {
    public:
        class iterator;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::collated_matches::iterator` is the forward iterator of [collated_matches](../collated_matches/README.md): its `*`
is the slice of the original text an occurrence covers, made when it is asked for, and it knows the byte position
of that occurrence and its size — the text's own, which need not be the pattern's.

## Rules

- An iterator holds the tracked object with the weighed text and the pattern, not the range: it stays valid after
  the range is gone, and a copy of it is independent.
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
| `(constructor)` | the default iterator, at no text; an iterator of a text is made by [begin](../collated_matches/begin.md) and [end](../collated_matches/end.md) |
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
    txt::collator primary{txt::strength::primary};
    string text = "Résumé, resume";
    auto matches = txt::collated_matches(primary, text, "resume");
    for (auto it = matches.begin(); it != matches.end(); ++it) {
        println("{}..{} [{}]", it.pos(), it.pos() + it.size(), *it);
    }
}
```

Output:

```text
0..8 [Résumé]
10..16 [resume]
```

## See also

- [collator::match](../collator-match.md): the same two numbers from a single search
- [sgcl::txt::collated_matches](../collated_matches/README.md)
