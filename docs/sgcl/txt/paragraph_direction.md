[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::paragraph_direction

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    direction paragraph_direction(const string& text) noexcept;
}
```

Returns which way the text runs, by rules P2 and P3 of
[UAX #9](https://www.unicode.org/reports/tr9/): the first strong character, a letter and not a digit, decides, and
what stands between an isolate initiator and its matching pop does not count. `"123 שלום"` runs right to left. A
paragraph of numbers and punctuation alone, and an empty one, runs left to right.

Of a text of several paragraphs it is the first one's: the text is cut after its first paragraph separator (rule
P1), and what comes after it runs its own way. [bidi_runs](bidi_runs.md) and [levels](levels.md) resolve every
paragraph.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`direction::left_to_right` or `direction::right_to_left`; never `direction::automatic`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"Nazwa: שלום", "שלום, Nazwa", "123 שלום", "\u2067abc\u2069 שלום", "2 + 2"}) {
        bool rtl = txt::paragraph_direction(s) == txt::direction::right_to_left;
        println("{}", rtl ? "right to left" : "left to right");
    }
}
```

Output:

```text
left to right
right to left
right to left
right to left
left to right
```

## See also

- [direction](direction.md): the directions of a paragraph
- [bidi_runs](bidi_runs.md): the pieces in the order they are drawn, and the paragraph's direction
- [txt](README.md)
