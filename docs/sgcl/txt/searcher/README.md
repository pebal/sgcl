[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::searcher

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class searcher;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::searcher` is a pattern of bytes prepared once: Boyer–Moore–Horspool over the bytes, the counterpart of
`std::boyer_moore_horspool_searcher`. The pattern is looked at once and a table of skips built from it; after that a
pattern of *m* bytes is found in *n* bytes in about *n/m* steps on ordinary text. It is for the loop that looks for
the same thing in many texts: a single search is what `find` of a [string](../../core/string/README.md) is for. The search of
a [regex](../regex/README.md) jumps to the places its literal run stands through the same table.

The bytes are safe to search. UTF-8 synchronises itself: the first byte of a character cannot appear inside
another one, so a match of valid UTF-8 inside valid UTF-8 always begins on a character, and no test for that is
needed. A search blind to case is [find_fold](../find_fold.md) and [fold_searcher](../fold_searcher/README.md), one blind to the
way a text was written [find_normalized](../find_normalized.md).

## Rules

- Nothing in it changes after it is built: one searcher may be used by any number of threads at once.
- Every member that takes a text takes a string, a [slice\<const char\>](../../core/slice/README.md), an array of `char` up to
  its first NUL or its end, and a `const char*` or `char*` up to its NUL.
- The search is held to `std::string_view::find` over **4000 random texts and patterns** over a small alphabet,
  where matches and near misses are frequent, each from two starting positions.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](searcher.md) | prepares the pattern |
| `(destructor)` | drops the pattern |

#### Search

| Function | Description |
|---|---|
| [find](find.md) | the byte position of the first occurrence at or after a position |
| [contains](contains.md) | checks whether the pattern occurs in the text |
| [count](count.md) | the number of occurrences that do not overlap |

#### Observers

| Function | Description |
|---|---|
| [pattern](pattern.md) | the pattern searched for |

## Complexity

Building the table is linear in the length of the pattern. A search takes about *n/m* steps on ordinary text and
*n·m* at worst.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Ala ma kota, a kot ma kota";
    txt::searcher kota("kota");
    println("{}, {}, {} razy", kota.find(text), kota.find(text, 8), kota.count(text));
}
```

Output:

```text
7, 22, 2 razy
```

## See also

- [fold_searcher, normalized_searcher](../fold_searcher/README.md): a pattern prepared for a search blind to case or to
  how a text was written
- [regex](../regex/README.md): a pattern matched in one pass
- [string](../../core/string/README.md): `find` for a single search
