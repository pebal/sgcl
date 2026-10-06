[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::lcs_length

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

size_t lcs_length(const string& a, const string& b, const distance_options& o = {});
```

Returns the length of the longest common subsequence of the texts: the most units both have in the same order, not
necessarily next to each other. The units neither keeps are what [diff_chars](diff_chars.md) shows as removed and
inserted: `|a| + |b| - 2 · lcs_length` of them. A unit is a code point (a byte that is not UTF-8 is one of its own),
or a byte with `o.bytes`; a common start and end of the two texts are set aside first.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [distance_options](distance_options.md): a cutoff, bytes as units |

## Return value

The length, or `o.max + 1` when it is larger than `o.max`.

## Complexity

O(⌈m/64⌉·n): the bit-parallel algorithm of Allison and Dix, as Hyyrö wrote it, a machine word for each 64 units of
the shorter text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::lcs_length("ABCBDAB", "BDCABA"));
    println("{}", txt::lcs_length("日本語", "日本"));
}
```

Output:

```text
4
2
```

## See also

- [levenshtein](levenshtein.md)
- [diff_chars](diff_chars.md)
- [sgcl::txt](README.md)
