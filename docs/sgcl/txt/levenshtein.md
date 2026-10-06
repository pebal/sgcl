[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::levenshtein

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

size_t levenshtein(const string& a, const string& b, const distance_options& o = {});
```

Returns the Levenshtein distance of the texts: the fewest insertions, deletions and substitutions of single units
that turn one into the other. A unit is a code point (a byte that is not UTF-8 is one of its own), or a byte with
`o.bytes`; a common start and end of the two texts are set aside first.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [distance_options](distance_options.md): a cutoff, bytes as units |

## Return value

The distance, or `o.max + 1` when it is larger than `o.max` (found without measuring when the lengths alone differ
by more).

## Complexity

O(⌈m/64⌉·n), m the units of the shorter text and n of the longer: Myers' bit-parallel algorithm, a machine word for
each 64 units of the shorter text, a step for each unit of the longer.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::levenshtein("kitten", "sitting"));
    println("{}", txt::levenshtein("zażółć", "zazolc"));
    println("{}", txt::levenshtein("zażółć", "zazolc", {.bytes = true}));
    for (const char* word : {"color", "colour", "collar", "cooler"}) {
        println("{} {}", word, txt::levenshtein("colour", word, {.max = 1}) <= 1);
    }
}
```

Output:

```text
3
4
8
color true
colour true
collar false
cooler false
```

## See also

- [osa_distance](osa_distance.md)
- [damerau_levenshtein](damerau_levenshtein.md)
- [lcs_length](lcs_length.md)
- [diff_chars](diff_chars.md)
- [sgcl::txt](README.md)
