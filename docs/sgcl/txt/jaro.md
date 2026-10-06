[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::jaro

```cpp
#include "sgcl/txt/distance.h"   // or "sgcl/txt.h"

double jaro(const string& a, const string& b, const jaro_options& o = {});
```

Returns Jaro's similarity of the texts, made for short strings such as names. Jaro's similarity counts the units of
each text that stand in the other within half the longer text's length of their place (m of them) and the half of
those out of order (t): `(m/|a| + m/|b| + (m - t)/m) / 3`, 1 for equal texts and 0 for texts with nothing in common
(two empty texts are equal). Units as for the distances: code points, or bytes with `o.bytes`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts |
| `o` | the [jaro_options](jaro_options.md); only `bytes` counts here |

## Return value

The similarity, in [0, 1].

## Complexity

O(n·w), w the window: half the longer text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{:.4f}", txt::jaro("MARTHA", "MARHTA"));
    println("{:.4f}", txt::jaro("DIXON", "DICKSONX"));
    println("{:.4f} {:.4f}", txt::jaro("", ""), txt::jaro("abc", "xyz"));
}
```

Output:

```text
0.9444
0.7667
1.0000 0.0000
```

## See also

- [jaro_winkler](jaro_winkler.md)
- [levenshtein](levenshtein.md)
- [sgcl::txt](README.md)
