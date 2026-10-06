[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::damerau_levenshtein

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

size_t damerau_levenshtein(const string& a, const string& b, const distance_options& o = {});
```

Returns Damerau's distance (Lowrance and Wagner): Levenshtein's edits and transpositions of neighbours, with units
free to be edited again after a transposition — `CA` to `ABC` is two, `CA` → `AC` → `ABC`. A metric: it is symmetric
and keeps the triangle inequality. A unit is a code point (a byte that is not UTF-8 is one of its own), or a byte
with `o.bytes`; a common start and end of the two texts are set aside first.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [distance_options](distance_options.md): a cutoff, bytes as units |

## Return value

The distance, or `o.max + 1` when it is larger than `o.max` (found without measuring when the lengths alone differ
by more).

## Complexity

O(m·n) time and O(m) memory: the table of Lowrance and Wagner a row at a time, the transpositions read from what
Zhao and Sahni showed is enough of the rows above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::damerau_levenshtein("CA", "ABC"), txt::osa_distance("CA", "ABC"));
    println("{}", txt::damerau_levenshtein("algorithm", "logarithm"));
}
```

Output:

```text
2 3
3
```

## See also

- [osa_distance](osa_distance.md)
- [levenshtein](levenshtein.md)
- [sgcl::txt](README.md)
