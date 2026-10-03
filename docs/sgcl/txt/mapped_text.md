[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::mapped_text

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct mapped_text {
        vector<char32_t> points;
        vector<size_t> at;
    };
}
```

`sgcl::txt::mapped_text` is a text as a search without regard to case, or to the way it was written, sees it: its
code points folded by the full folding ([fold_case](fold_case.md)) or decomposed and put in canonical order, and the
byte of the original text each of them came from. [folded_text](folded_text/points.md),
[normalized_text](folded_text/points.md), [fold_matches](fold_matches/points.md) and
[normalized_matches](fold_matches/points.md) build one and keep it; `points()` shows it.

The positions belong to the places in the mapped text: one character's code points all carry its byte, so where the
position changes is where the next character begins, and the positions ascend through the whole text. That is what
lets a match be reported in bytes of the text as it was given, and what keeps a match from cutting a character in
two.

## Rules

- A plain struct of two [vectors](../core/vector/README.md): it lives where those may, on a stack or inside a managed
  object. The ones the classes keep do not change after they are built.
- `at` holds one entry more than `points`: the size of the text, so that the end of a match is a position too.

## Member objects

| Field | Description |
|---|---|
| `points` | the code points of the mapping, in order |
| `at` | for each code point the byte of the original text it came from, then the size of the text |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::folded_text ft("Maße");
    const txt::mapped_text& m = ft.points();
    for (size_t i : range(m.points.size())) {
        print("{}@{} ", m.points[i], m.at[i]);
    }
    println("end@{}", m.at.back());
}
```

Output:

```text
m@0 a@1 s@2 s@2 e@4 end@5
```

## See also

- [folded_text, normalized_text](folded_text/README.md): a text mapped once
- [fold_matches, normalized_matches](fold_matches/README.md): every occurrence, the text mapped once
- [occurrence](occurrence.md): where a search found its pattern
