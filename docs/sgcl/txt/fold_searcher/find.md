[sgcl](../../README.md) › [txt](../README.md) › [fold_searcher](../fold_searcher.md)

# sgcl::txt::fold_searcher::find

```cpp
optional<occurrence> find(const string& text, size_t from = 0) const noexcept;
```

Finds the first occurrence of the prepared pattern in the text at or after the byte `from`: of a `fold_searcher`
without regard to case, as [find_fold](../find_fold.md); of a `normalized_searcher` without regard to the way the
text was written, as [find_normalized](../find_normalized.md). The text is mapped on every call, the pattern never
again. A match takes whole characters and whole combining sequences; an empty pattern is found at `from` while
`from` is not past the end of the text.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `from` | the byte of the text the search starts at |

## Return value

The [occurrence](../occurrence.md) — the byte position and the bytes it covers in `text` — or an empty `optional`
when there is none at or after `from`.

## Complexity

Linear in the length of the text for the mapping; the scan is linear in the text on ordinary text and the text
times the pattern at worst.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::fold_searcher kot("KOT");
    for (string t : {"Ala ma kota", "Kot Ali", "pies"}) {
        auto o = kot.find(t);
        print("{} ", o ? txt::format("{}", o->pos) : "-");
    }
    println();
}
```

Output:

```text
7 0 - 
```

## See also

- [contains](contains.md): whether there is an occurrence
- [folded_text::find](../folded_text/find.md): the text mapped once instead
- [sgcl::txt::fold_searcher, normalized_searcher](../fold_searcher.md)
