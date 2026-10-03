[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::contains_normalized

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool contains_normalized(const string& text, const string& pattern) noexcept;
}
```

Checks whether the pattern occurs in the text without regard to the way either side was written:
[find_normalized](find_normalized.md)`(text, pattern)` has a value. Both sides are decomposed and put in canonical
order, and a match takes whole characters and whole combining sequences, as `find_normalized` says.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the text to look for |

## Return value

`true` when the pattern occurs in the text, `false` otherwise; `true` for an empty pattern.

## Complexity

Linear in the lengths of the text and of the pattern for the decomposition; the scan is linear in the text on
ordinary text and the text times the pattern at worst.

## Exceptions

None.

## Notes

Both sides are decomposed on every call: a pattern asked of many texts is a
[normalized_searcher](fold_searcher/README.md), a text asked many questions a [normalized_text](folded_text/README.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string hangul = "한국";  // two syllables
    string jamo = "\u1112\u1161\u11AB";  // the first one, in its three jamo
    println("{} {}", txt::contains_normalized(hangul, jamo),
            txt::contains_normalized(hangul, "\u1112"));
}
```

Output:

```text
true false
```

## See also

- [find_normalized](find_normalized.md): where it occurs
- [contains_fold](contains_fold.md): blind to case
