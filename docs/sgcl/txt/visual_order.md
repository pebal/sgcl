[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::visual_order

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<size_t> visual_order(const string& text,
                                direction paragraph = direction::automatic) noexcept;
}
```

Returns the byte position of every code point of the text in the order it is drawn, left to right, the ones rule X9
of [UAX #9](https://www.unicode.org/reports/tr9/) removes (the embedding, override and pop characters and the
boundary neutrals) left out. It is the answer of [bidi_runs](bidi_runs/README.md) one character at a time: what a caret
steps over in mixed text, where the right arrow key may move backwards through the bytes, and what a renderer that
lays out character by character walks.

The levels are [levels](levels.md)'s, and the order is rule L2's: from the highest level down to the lowest odd
one, every run at that level or above is turned round. A text of several paragraphs is cut after each paragraph
separator (rule P1) and every paragraph resolved on its own; each is a line of its own, so its code points follow
those of the paragraph before, its separator at the end of a left to right one and at the start of a right to left
one.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `paragraph` | the direction of every paragraph, or `direction::automatic` for each to decide |

## Return value

The byte positions of the code points that are drawn, in the order they are drawn.

## Complexity

Linear in the length of the text times the number of levels it reaches, two or three in a real text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string mixed = "aאבb";
    println("{}", txt::visual_order(mixed));
    println("{}", txt::visual_order(mixed, txt::direction::right_to_left));
    println("{}", txt::visual_order("ab\u202Ecd"));  // the override itself is not drawn
}
```

Output:

```text
[0, 3, 1, 5]
[5, 3, 1, 0]
[0, 1, 6, 5]
```

## See also

- [bidi_runs](bidi_runs/README.md): the same order, cut into pieces
- [levels](levels.md): the level of every code point
- [txt](README.md)
