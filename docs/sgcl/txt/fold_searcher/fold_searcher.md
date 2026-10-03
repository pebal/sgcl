[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](README.md)

# sgcl::txt::fold_searcher::fold_searcher

```cpp
explicit fold_searcher(const string& pattern) noexcept;
explicit normalized_searcher(const string& pattern) noexcept;
```

Keeps the pattern and maps it once: a `fold_searcher` folds it by the full folding, so `"ß"` becomes `ss`; a
`normalized_searcher` decomposes it and puts its marks in canonical order, so an `"é"` of one code point becomes
two. The mapped code points are what every search looks for.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the text to look for, UTF-8 |

## Complexity

Linear in the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher folded("Straße");
    txt::normalized_searcher decomposed("é");
    println("{} -> {} code points, {} -> {}", folded.pattern(), folded.size(),
            decomposed.pattern(), decomposed.size());
}
```

Output:

```text
Straße -> 7 code points, é -> 2
```

## See also

- [points](points.md): the mapped code points
- [sgcl::txt::fold_searcher, normalized_searcher](README.md)
