[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::direction

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class direction : uint8_t {
        automatic,
        left_to_right,
        right_to_left,
    };
}
```

Which way a paragraph runs as a whole: what [paragraph_direction](paragraph_direction.md) and
[bidi_runs::paragraph](bidi_runs/paragraph.md) answer, and what a caller passes to [levels](levels.md),
[visual_order](visual_order.md), [mirrored](mirrored.md) and [bidi_runs](bidi_runs.md) instead of letting the text
say.

`automatic`, the default, lets the text decide by rules P2 and P3 of [UAX #9](https://www.unicode.org/reports/tr9/):
the first strong character, a letter and not a digit, sets the paragraph. `"123 שלום"` runs right to left, because a
number is not strong and the Hebrew behind it is. A paragraph with no strong character at all runs left to right. A
caller who knows better (a user interface with a language setting, a protocol that says so) passes `left_to_right`
or `right_to_left`.

| Value | Description |
|---|---|
| `automatic` | the first strong character decides; never an answer, only asked for |
| `left_to_right` | the paragraph runs left to right, level 0 |
| `right_to_left` | the paragraph runs right to left, level 1 |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"Ala ma kota", "123 שלום", "123", ""}) {
        auto d = txt::paragraph_direction(s);
        bool rtl = d == txt::direction::right_to_left;
        println("[{}] {}", s, rtl ? "right to left" : "left to right");
    }
    println("{}", txt::levels("שלום abc", txt::direction::left_to_right));
}
```

Output:

```text
[Ala ma kota] left to right
[123 שלום] right to left
[123] left to right
[] left to right
[1, 1, 1, 1, 0, 0, 0, 0]
```

## See also

- [paragraph_direction](paragraph_direction.md): which way a text runs
- [levels](levels.md): the level of every code point
- [txt](README.md)
