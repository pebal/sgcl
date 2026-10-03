[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](../fold_searcher.md)

# sgcl::txt::fold_searcher::points

```cpp
const vector<char32_t>& points() const noexcept;
```

Returns the code points the pattern mapped to, the sequence every search looks for: folded by the full folding for
a `fold_searcher`, decomposed and in canonical order for a `normalized_searcher`.

## Parameters

None.

## Return value

The mapped code points, in order.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher folded("Maß");
    for (char32_t c : folded.points()) {
        print("U+{:04X} ", uint32_t(c));
    }
    println();
    txt::normalized_searcher decomposed("\u1E0B\u0323");  // d with a dot above, then one below
    for (char32_t c : decomposed.points()) {
        print("U+{:04X} ", uint32_t(c));
    }
    println();
}
```

Output:

```text
U+006D U+0061 U+0073 U+0073 
U+0064 U+0323 U+0307 
```

## See also

- [size](size.md): how many
- [fold_case](../fold_case.md), [nfd](../nfc_t.md): the two mappings
- [sgcl::txt::fold_searcher, normalized_searcher](../fold_searcher.md)
