[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::levels

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    vector<uint8_t> levels(const string& text, direction paragraph = direction::automatic) noexcept;
}
```

Returns the resolved embedding level of every code point of the text, by the bidirectional algorithm of
[UAX #9](https://www.unicode.org/reports/tr9/): even runs left to right, odd right to left, 0 a left to right
paragraph, 1 a right to left one or a right to left piece in it, 2 a left to right piece inside that, and so on, up
to the depth of 125 the standard allows. It is the raw answer, for a renderer that lays the text out itself;
[bidi_runs](bidi_runs.md) is the same thing already cut into pieces, [visual_order](visual_order.md) the same thing
as the order the code points are drawn in.

The algorithm works out everything but the paragraph's direction on its own: the embedding and override characters,
the isolates of Unicode 6.3, the weak types (a number after an Arabic letter is an Arabic number), the neutrals
between two directions, and the brackets: `(` and `)` take one direction together, so a parenthesis does not flip
away from what it encloses. The paragraph's direction is `paragraph` when it is not `direction::automatic`, otherwise
the first strong character's ([direction](direction.md)).

The levels are one to a code point, in the order the code points are stored, the ones rule X9 removes (the
embedding, override and pop characters and the boundary neutrals) included with a level of their own. An invalid byte
of UTF-8 is one code point, `U+FFFD`.

A text of several paragraphs is cut after each paragraph separator (rule P1: a line feed, a carriage return, CR LF
as one, U+2029, U+0085, the information separators 1C to 1E), the separator kept with the paragraph it ends, and
every paragraph is resolved on its own, its direction its own first strong character's. A separator takes the level of its paragraph (L1).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `paragraph` | the direction of every paragraph, or `direction::automatic` for each to decide |

## Return value

The level of every code point of the text, in the order they are stored.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Notes

The oracle is `BidiCharacterTest.txt` of the UCD, which gives for every case the level the paragraph resolves to,
the level of every character and the order they are drawn in. **All 91 707 of its cases pass**; the header the tests
carry holds every third of them, 30 569, because the whole file is 7.8 MB of test data, four times everything else
the tests carry, and the full set is run by hand before a change to the algorithm lands
(`python3 tools/unicode_tables.py --all-bidi`).

The file's cases are single paragraphs, so rule P1 is held to them joined: every case followed by a paragraph
separator and the next case of the same direction asked for must give the first case's levels, the separator at its
paragraph's level, then the second case's, and the two orders one after the other. All 91 674 such pairs pass.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string line = "Nazwa: שלום 123 OK";
    println("{}", txt::levels(line));
    println("{}", txt::levels(line, txt::direction::right_to_left));
}
```

Output:

```text
[0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 0, 0, 0]
[2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2]
```

Two paragraphs, each resolved on its own:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "abc\n\u05D0\u05D1 1";  // a left to right paragraph, then a right to left one
    println("{}", txt::levels(text));
}
```

Output:

```text
[0, 0, 0, 0, 1, 1, 1, 2]
```

## See also

- [bidi_runs](bidi_runs.md): the pieces, each at its level, in the order they are drawn
- [visual_order](visual_order.md): the order the code points are drawn in
- [mirrored](mirrored.md): rule L4 with the levels in hand
- [txt](README.md)
