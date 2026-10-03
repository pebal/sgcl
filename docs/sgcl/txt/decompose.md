[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::decompose

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string decompose(char32_t c) noexcept;
}
```

Returns one code point taken apart as far as it goes, canonically: its full canonical decomposition with the marks in
canonical order, which is its NFD. A decomposition that chains (a letter with two accents decomposes into a letter
with one and a mark) is followed to the bottom; a Hangul syllable comes apart into its jamo by arithmetic. A code
point with no decomposition is itself, and a value that is no code point (a surrogate, one past `U+10FFFF`) is
`U+FFFD`, as everywhere in the module. The compatibility decompositions are not taken: `decompose(U'ﬁ')` is `"ﬁ"`,
and [normalize](normalize.md) with `nfkd` gives `"fi"`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The decomposition as a UTF-8 string of one to four code points.

## Complexity

Constant: a decomposition is a few code points long.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'ǻ', U'각', U'ﬁ', U'a'}) {
        print("{}:", c);
        for (char32_t part : txt::decompose(c).runes()) {
            print(" {:04X}", uint32_t(part));
        }
        println();
    }
}
```

Output:

```text
ǻ: 0061 030A 0301
각: 1100 1161 11A8
ﬁ: FB01
a: 0061
```

## See also

- [compose](compose.md): two code points put together
- [normalize](normalize.md): a whole text decomposed or composed
- [txt](README.md)
