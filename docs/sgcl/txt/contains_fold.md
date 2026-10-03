[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::contains_fold

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool contains_fold(const string& text, const string& pattern) noexcept;
}
```

Checks whether the pattern occurs in the text without regard to case: [find_fold](find_fold.md)`(text, pattern)`
has a value. Both sides are folded by the full folding, so `"STRASSE"` is in `"straße"`, and a match takes whole
characters and whole combining sequences, as `find_fold` says.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the text to look for |

## Return value

`true` when the pattern occurs in the text, `false` otherwise; `true` for an empty pattern.

## Complexity

Linear in the lengths of the text and of the pattern for the folding; the scan is linear in the text on ordinary
text and the text times the pattern at worst.

## Exceptions

None.

## Notes

Both sides are folded on every call: a pattern asked of many texts is a [fold_searcher](fold_searcher/README.md), a text
asked many questions a [folded_text](folded_text/README.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::contains_fold("ul. Długa 5, GDAŃSK", "gdańsk"),
            txt::contains_fold("STRASSE", "straße"));
}
```

Output:

```text
true true
```

## See also

- [find_fold](find_fold.md): where it occurs
- [contains_normalized](contains_normalized.md): blind to the way a text was written
