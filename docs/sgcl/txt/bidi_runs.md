[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::bidi_runs

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class bidi_runs;
}
```

`txt::bidi_runs` is the pieces of a text in the order they are drawn, each with the level it runs at: the answer of
the bidirectional algorithm of [UAX #9](https://www.unicode.org/reports/tr9/) for whoever draws text that runs both
ways at once, a user interface or a terminal, and for moving a caret through it. Storing, searching and comparing
need none of it.

Arabic, Hebrew, Persian and Urdu are written right to left, but the numbers inside them run left to right, and so
does a Latin word quoted in them. One line then has pieces going both ways, and the order the characters are
**stored** in, the logical order, the order they are typed and read, is not the order they are **drawn** in:

```text
stored:  Nazwa: שלום 123 OK
drawn:   Nazwa: OK 123 םולש
```

The algorithm gives every character a level ([levels](levels.md)): 0 runs left to right, 1 right to left, 2 left to
right inside a right to left piece, and so on. What is drawn is the pieces of odd level turned round. The range walks
the pieces **in the order they go on the line**, left to right, and a renderer draws them one after another and
turns the characters of a piece of odd level round; [mirrored](mirrored.md) says which of their glyphs to change.
Nothing is copied: every piece is a slice of the text. The range is a range of the library
([mixin::enumerable](../core/mixin/enumerable.md)).

## Rules

- A `bidi_runs` holds a slice of the text, so it lives where a `tracked_ptr` may: on a stack or inside a managed
  object ([the rules of core](../core/README.md#the-rules), 1). A range over a temporary string is safe: the slice
  keeps the string's object.
- The pieces are worked out when the range is constructed, the whole text at once, and held in a vector of the
  range; the walk reads them.
- A text of several paragraphs is cut after each paragraph separator (rule P1), the separator kept with the paragraph
  it ends, and every paragraph is resolved on its own, its direction its own. Each is a line of its own: its pieces
  come after the pieces of the one before, and no piece holds characters of two.
- A piece is a run of code points next to each other in the text and at one level. The characters rule X9 removes
  (the embedding, override and pop characters and the boundary neutrals) are in no piece: they are not drawn.
- A copy is the same runs. Runs moved from are the empty ones, as ones made with nothing, left to right; assigned
  to, they are the new ones.

## Member types

| Type | Definition |
|---|---|
| [run](bidi_runs-run.md) | a piece: the slice of the text and its level |
| `value_type` | `run` |
| `size_type` | `size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bidi_runs/bidi_runs.md) | runs the algorithm over a text |
| `(destructor)` | drops the slice of the text and the pieces |

#### Iterators

| Function | Description |
|---|---|
| [begin](bidi_runs/begin.md) | an iterator to the leftmost piece |
| [end](bidi_runs/end.md) | the iterator past the rightmost piece |

#### Capacity

| Function | Description |
|---|---|
| [empty](bidi_runs/empty.md) | checks whether there is no piece |
| [count](bidi_runs/count.md) | the number of pieces |

#### Observers

| Function | Description |
|---|---|
| [paragraph](bidi_runs/paragraph.md) | the direction of the first paragraph |
| [text](bidi_runs/text.md) | the slice of the text the pieces are cut from |

#### From mixin::enumerable

The questions asked of the pieces, carried by every range of the library
([mixin::enumerable](../core/mixin/enumerable.md)).

| Function | Description |
|---|---|
| [exists](../core/mixin/enumerable/exists.md) | checks whether the predicate accepts some piece |
| [all](../core/mixin/enumerable/all.md) | checks whether the predicate accepts every piece |
| [count_of](../core/mixin/enumerable/count_of.md) | the number of pieces the predicate accepts |
| [find_if](../core/mixin/enumerable/find_if.md) | the first piece the predicate accepts |
| [for_each](../core/mixin/enumerable/for_each.md) | calls a function with every piece |

## Complexity

The construction is linear in the length of the text times the number of levels it reaches, two or three in a real
text; a step of the iterator is constant.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string line = "Nazwa: שלום 123 OK";
    txt::bidi_runs pieces(line);
    println("the paragraph runs {}", pieces.paragraph() == txt::direction::right_to_left
                                         ? "right to left"
                                         : "left to right");
    for (auto piece : pieces) {
        println("  level {} {} [{}]", piece.level,
                piece.right_to_left() ? "right to left" : "left to right", piece.text);
    }
}
```

Output:

```text
the paragraph runs left to right
  level 0 left to right [Nazwa: ]
  level 2 left to right [123]
  level 1 right to left [שלום ]
  level 0 left to right [ OK]
```

## See also

- [levels](levels.md): the level of every code point
- [visual_order](visual_order.md): the order one code point at a time
- [mirrored](mirrored.md): the glyphs a right to left piece changes
- [paragraph_direction](paragraph_direction.md): the direction of a paragraph alone
