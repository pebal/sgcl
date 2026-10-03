[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::fold_searcher, normalized_searcher

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using fold_searcher = /* a pattern folded once */;
    using normalized_searcher = /* a pattern decomposed once */;
}
```

`sgcl::txt::fold_searcher` is a pattern folded once ([fold_case](../fold_case.md)), for the loop that asks the same
question of many texts without regard to case; `sgcl::txt::normalized_searcher` is a pattern decomposed and put in
canonical order once, for the same loop without regard to the way a text was written. The two are one class over
two mappings, with one interface: what this page and the pages of the members say of one holds for the other, the
folding read as the decomposition. A searcher keeps the pattern as it was given, as [searcher](../searcher/README.md) does,
so that it can say what it looks for.

What a searcher saves is the pattern's mapping; its [find](find.md), [contains](contains.md)
and [count](count.md) still map the text on every call. The other way round — one text asked many
questions — is a [folded_text](../folded_text/README.md) or a [normalized_text](../folded_text/README.md), which takes a searcher as its
pattern; every occurrence of one pattern in one text is [fold_matches](../fold_matches/README.md). A match takes whole
characters and whole combining sequences, as [find_fold](../find_fold.md) and [find_normalized](../find_normalized.md)
say: the one-shot functions, the prepared text, the prepared pattern and the ranges all answer the same.

## Rules

- A searcher holds a [string](../../core/string/README.md) and a [vector](../../core/vector/README.md), so it lives where those may: on
  a stack or inside a managed object.
- Nothing in it changes after it is built: one searcher may be used by any number of threads at once.
- A copy is the same pattern. A searcher moved from is the searcher of the empty pattern, its `pattern()` empty
  too, and answers as one built from `""`; assigned to, it is the new one.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](fold_searcher.md) | maps the pattern |
| `(destructor)` | drops the pattern and its mapping |

#### Search

| Function | Description |
|---|---|
| [find](find.md) | the first occurrence in a text, at or after a position |
| [contains](contains.md) | checks whether the pattern occurs in a text |
| [count](count.md) | the number of occurrences that do not overlap |

#### Observers

| Function | Description |
|---|---|
| [pattern](pattern.md) | the pattern as it was given |
| [size](size.md) | the number of code points the pattern mapped to |
| [empty](empty.md) | checks whether the pattern mapped to nothing |
| [points](points.md) | the code points the pattern mapped to |

## Complexity

Building is linear in the length of the pattern. A search maps the text, linear in its length, and scans it:
linear in the text on ordinary text, the text times the pattern at worst.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher street("straße");
    for (string t : {"STRASSE", "Strasse 5", "ulica"}) {
        print("{} ", street.contains(t));
    }
    println();

    txt::normalized_searcher cafe("cafe\u0301");
    println("{}", cafe.find("Le café du coin").has_value());
}
```

Output:

```text
true true false 
true
```

## See also

- [folded_text, normalized_text](../folded_text/README.md): a text mapped once
- [fold_matches, normalized_matches](../fold_matches/README.md): every occurrence, as a range
- [find_fold](../find_fold.md), [find_normalized](../find_normalized.md): the one-shot forms
- [searcher](../searcher/README.md): a pattern of bytes
