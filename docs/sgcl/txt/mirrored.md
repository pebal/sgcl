[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::mirrored

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    /*(1)*/ string mirrored(const string& text,
                            direction paragraph = direction::automatic) noexcept;
    /*(2)*/ string mirrored(const string& text, const vector<uint8_t>& levels) noexcept;
}
```

Returns the text with rule L4 of [UAX #9](https://www.unicode.org/reports/tr9/) applied: every character whose
resolved level is odd and which is `Bidi_Mirrored` ([is_mirrored](is_mirrored.md)) swapped for the code point of its
mirrored shape ([mirrored_of](mirrored_of.md)), the rest left alone, and the whole kept in the order it is
**stored** in.

A bracket in a right to left run is drawn the other way round: the character that opens a parenthesis in Arabic has
to be **shown** as `)`, because the line runs the other way and the shape has to follow it. That is a substitution
of one character for another, nothing more. It is **not** shaping: joining an Arabic letter to its neighbours,
choosing an initial or a final form, forming a ligature and placing a mark are a font's work, and none of it is
here.

A renderer holds this and the pieces of [bidi_runs](bidi_runs.md) and has what it needs: the levels say which pieces
to turn round, this says which glyphs to change, and neither is any use without the other.

1. Works the levels out, as [levels](levels.md) does, with `paragraph` the direction of every paragraph or
   `direction::automatic` for each to decide; a text of several paragraphs is cut after each paragraph separator
   (rule P1) and every paragraph resolved on its own.
2. Takes the levels the caller has in hand: whoever draws the text has worked them out already, since
   [bidi_runs](bidi_runs.md) and [levels](levels.md) both run the whole algorithm. It is the rule and nothing else.
   The levels are one to a code point, as `levels` gives them; a code point the vector does not reach is left where
   it stands, and levels of the caller's own making are obeyed as given.

- (1–2) A text with nothing to mirror comes back as **the same object**, which is most texts: the first pass over it
  only asks. A mirrored code point takes as many bytes as its original, so the text keeps its length.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `paragraph` | the direction of every paragraph, or `direction::automatic` for each to decide |
| `levels` | the level of every code point, as [levels](levels.md) gives them |

## Return value

The text with the mirrored characters swapped; `text` itself, the same object, when there is nothing to swap.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

Rule L4 is checked against the levels `BidiCharacterTest.txt` itself gives, over the 30 569 cases the tests carry,
17 017 of which have something to mirror: an oracle that does not depend on the levels this library works out, which
the test of [levels](levels.md) weighs against the same file. `BidiTest.txt` is no use here: it gives classes rather
than characters, and a class has no glyph to mirror.

What (2) saves against (1) is on [Benchmarks: Bidirectional text](benchmarks.md#bidirectional-text).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string brackets = "א (ב) [ג]";
    println("stored: {}\ndrawn:  {}", brackets, txt::mirrored(brackets));

    string latin = "Ala (ma) kota";
    println("{}", txt::mirrored(latin).object() == latin.object());

    auto lv = txt::levels(brackets);
    println("{}", txt::mirrored(brackets, lv));  // the paragraph not worked out a second time
}
```

Output:

```text
stored: א (ב) [ג]
drawn:  א )ב( ]ג[
true
א )ב( ]ג[
```

## See also

- [is_mirrored](is_mirrored.md), [mirrored_of](mirrored_of.md): the property and the mapping of one code point
- [bidi_runs](bidi_runs.md): the pieces to turn round
- [txt](README.md)
