[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::line_breaks

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class line_breaks;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`txt::line_breaks` is the text cut at every place a line may be broken, by [UAX
#14](https://www.unicode.org/reports/tr14/): after a space or a hyphen and between two ideographs, never between a
number and its decimal mark, inside `"(a)"` or at a no-break space. Every element is a piece that must stay together,
its trailing spaces included, so a renderer lays them one after another and starts a new line where the next will not
fit; [wrap](../wrap.md) does exactly that over [columns](../columns.md).

The range is constructed from the text, `txt::line_breaks(s)`, which looks like a call and is a construction, as
[runes](../../core/runes/README.md) is. Its element is a piece that must stay together, a [slice](../../core/slice/README.md) of the text,
and its [iterator](../line_breaks-iterator/README.md) knows the byte position of the element and its size, for the code that
goes back to the bytes. It is a range of the library ([mixin::enumerable](../../core/mixin/enumerable/README.md)): `count_of`,
`exists`, `find_if` and `for_each` are asked of the pieces.

## Rules

- A `line_breaks` holds a slice of the text. A range over a temporary string is safe: the slice keeps the string's
  object.
- Nothing is copied and nothing is allocated per element: an element is a [slice](../../core/slice/README.md) of the text, found
  as the walk reaches it. An invalid byte of UTF-8 is one code point, `U+FFFD`.
- An iterator refers to the text, not to the range: it is valid while the text's object lives.
- Every byte of the text belongs to exactly one piece.
- Unlike the other ranges of boundaries, its iterator carries state: rule LB15a asks what stood before an opening
  quotation mark, and that may be on the other side of a break opportunity, so the scan runs from the beginning of the
  text. Walking the range is still linear.
- A run of ASCII letters holds together on a line (rule LB28), walked in one step rather than one a character.
- **The scripts that write without spaces are the limit of this.** In Thai, Lao, Khmer and Burmese a line may be
  broken between words, and the words are not marked: finding them takes a dictionary of the language, a few hundred
  kilobytes of one. UAX #14 gives those characters the class SA and says an implementation without a dictionary
  resolves it from the category, which is what rule LB1 does here, making them ordinary letters. So a text in those
  scripts breaks between characters rather than between words: correct by the rules, and not what somebody who reads
  them expects. The day it matters it is a dictionary and a segmentation of its own, not another rule.
- The oracle is `LineBreakTest.txt` of the UCD, every one of its 16672 cases, with no rule tailored and none skipped.
  The tables are Line_Break, 14.8 KB with rule LB1 already resolved in it, and the East Asian set of UAX #14, 0.7 KB.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](../line_breaks-iterator/README.md) | a forward iterator whose `*` is a piece that must stay together, with its byte position and its size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](line_breaks.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first piece that must stay together |
| [end](end.md) | the iterator past the last one |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the text is empty |
| [count](count.md) | the number of pieces, walked |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the pieces, carried by every range of the library
([mixin::enumerable](../../core/mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| [exists](../../core/mixin/enumerable/exists.md) | checks whether the predicate accepts some element |
| [all](../../core/mixin/enumerable/all.md) | checks whether the predicate accepts every element |
| [count_of](../../core/mixin/enumerable/count_of.md) | the number of elements the predicate accepts |
| [find_if](../../core/mixin/enumerable/find_if.md) | the first element the predicate accepts |
| [for_each](../../core/mixin/enumerable/for_each.md) | calls a function with every element |

## Complexity

A step of the iterator is linear in the bytes of the element it finds; `count` and every question of
[mixin::enumerable](../../core/mixin/enumerable/README.md) walk the text, linear in its bytes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "Ala ma kota (a) 3.14 e-mail 漢字";
    for (auto piece : txt::line_breaks(s)) {
        print("[{}]", piece);
    }
    println();
}
```

Output:

```text
[Ala ][ma ][kota ][(a) ][3.14 ][e-][mail ][漢][字]
```

## See also

- [graphemes](../graphemes/README.md), [word_breaks](../word_breaks/README.md), [words](../words/README.md), [sentences](../sentences/README.md): the other
  boundaries of a text
- [grapheme_count](../grapheme_count.md): the characters of a text counted
- [runes](../../core/runes/README.md): the code points of a text
