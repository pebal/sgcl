[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::occurrence

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct occurrence {
        size_t pos = 0;
        size_t size = 0;

        friend bool operator==(const occurrence&, const occurrence&) noexcept = default;
    };
}
```

`sgcl::txt::occurrence` is where a search blind to case, or to the way a text was written, found its pattern: the
bytes of the original text the match covers, as [find_fold](find_fold.md), [find_normalized](find_normalized.md) and
the [find](fold_searcher/find.md) of a [fold_searcher](fold_searcher/README.md) return them. The size is the text's own
and need not be the pattern's: folding and decomposing make the two different lengths, so `"STRASSE"` covers the
seven bytes of `"straße"`, not the seven of the pattern, and a position alone would not say where the match ends.

## Member objects

| Field | Description |
|---|---|
| `pos` | the byte position in the text where the match begins; `0` by default |
| `size` | the bytes of the text the match covers; `0` by default, and for an empty pattern |

## Non-member functions

| Function | Description |
|---|---|
| `operator==`, `operator!=` | compare both fields |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Die straße";
    txt::occurrence o = *txt::find_fold(text, "STRASSE");
    println("{} {} [{}]", o.pos, o.size, text.as_slice(o.pos, o.size));
    println("{}", o == txt::occurrence{4, 7});
}
```

Output:

```text
4 7 [straße]
true
```

## See also

- [find_fold](find_fold.md), [find_normalized](find_normalized.md)
- [fold_searcher, normalized_searcher](fold_searcher/README.md)
