[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::osa_distance

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

size_t osa_distance(const string& a, const string& b, const distance_options& o = {});
```

Returns the optimal string alignment distance: Levenshtein's edits and the transposition of two neighbouring units,
no unit edited twice — so `ab` to `ba` is one edit, but `CA` to `ABC` three (the transposed pair could not then take
the `B` between). What a spelling checker counts; not a metric (the triangle inequality can fail), which
[damerau_levenshtein](damerau_levenshtein.md) is. A unit is a code point (a byte that is not UTF-8 is one of its
own), or a byte with `o.bytes`; a common start and end of the two texts are set aside first.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [distance_options](distance_options.md): a cutoff, bytes as units |

## Return value

The distance, or `o.max + 1` when it is larger than `o.max` (found without measuring when the lengths alone differ
by more).

## Complexity

O(⌈m/64⌉·n) as [levenshtein](levenshtein.md): Hyyrö's bit-parallel algorithm.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::osa_distance("ab", "ba"), txt::levenshtein("ab", "ba"));
    println("{}", txt::osa_distance("CA", "ABC"));
    println("{}", txt::osa_distance("receive", "recieve"));
}
```

Output:

```text
1 2
3
1
```

## See also

- [levenshtein](levenshtein.md)
- [damerau_levenshtein](damerau_levenshtein.md)
- [sgcl::txt](README.md)
