[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::strength

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class strength : uint8_t {
        primary,
        secondary,
        tertiary,
        quaternary,
    };
}
```

How much of a difference counts to a [collator](collator.md): which of the levels of UTS #10 are looked at. The
algorithm gives every character weights at three levels — the letter, the accent, the case — and compares them a
level at a time, so a difference of letters settles the question before an accent is looked at, and an accent
before a capital. Whatever the strength, a text is the same text however it was written: `café` with one code
point and with two compare equal at every level.

| Value | Description |
|---|---|
| `primary` | the letters alone: `resume`, `résumé` and `RESUME` are one word — what a search box wants, and what a duplicate check wants |
| `secondary` | the letters and the accents: `résumé` differs from `resume`, `RESUME` does not |
| `tertiary` | the letters, the accents and the case, all three different words — what a sorted list wants; the default |
| `quaternary` | the fourth level as well, which holds the punctuation [punctuation::shifted](punctuation.md) moved aside and nothing else: worth asking for only with it, and then only to keep `re-sume` and `resume` apart rather than merely together |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto level : {txt::strength::primary, txt::strength::secondary, txt::strength::tertiary}) {
        txt::collator c(level);
        println("{} {}", c.equal("resume", "résumé"), c.equal("resume", "RESUME"));
    }
}
```

Output:

```text
true true
false true
false false
```

## See also

- [collator::options](collator-options.md): the strength beside the settings
- [collator::level](collator/level.md): the strength of a collator
- [collator](collator.md)
