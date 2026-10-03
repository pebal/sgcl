[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::word_breaks

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using word_breaks = /* unspecified */;
}
```

`txt::word_breaks` is the text cut at every word boundary of [UAX #29](https://www.unicode.org/reports/tr29/), which
gives the words **and** the runs between them: that is what the annex defines, and what a double click, or a text put
back together word by word, needs. A run of spaces is one segment (rule WB3d) while each punctuation mark is its own.
[words](../words/README.md) gives the words alone.

The range is constructed from the text, `txt::word_breaks(s)`, which looks like a call and is a construction, as
[runes](../../core/runes/README.md) is. Its element is a segment between two word boundaries, a [slice](../../core/slice/README.md) of the
text, and its [iterator](../word_breaks-iterator/README.md) knows the byte position of the element and its size, for the code
that goes back to the bytes. It is a range of the library ([mixin::enumerable](../../core/mixin/enumerable/README.md)):
`count_of`, `exists`, `find_if` and `for_each` are asked of the segments.

## Rules

- A `word_breaks` holds a slice of the text, so it lives where a `tracked_ptr` may: on a stack or inside a managed
  object ([the rules of core](../../core/README.md#the-rules), 1). A range over a temporary string is safe: the slice
  keeps the string's object.
- Nothing is copied and nothing is allocated per element: an element is a [slice](../../core/slice/README.md) of the text, found
  as the walk reaches it. An invalid byte of UTF-8 is one code point, `U+FFFD`.
- An iterator refers to the text, not to the range: it is valid while the text's object lives.
- Every byte of the text belongs to exactly one segment: the segments put back together are the text.
- The rules keep an apostrophe and a decimal point inside a word (`don't`, `3.14`, `192.168.0.1`) and cut at a hyphen
  (`e-mail` is two words).
- A run of ASCII letters is one word (rule WB5), walked in one step rather than one a character.
- The oracle is `WordBreakTest.txt` of the UCD, every one of its 1826 cases, with no rule tailored and none skipped.
  The table is Word_Break, 9.1 KB.
- `word_breaks` is an alias of a class template of the library that [graphemes](../graphemes/README.md), `word_breaks`,
  [words](../words/README.md) and [sentences](../sentences/README.md) share, each its own type; the template is not part of the interface.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](../word_breaks-iterator/README.md) | a forward iterator whose `*` is a segment between two word boundaries, with its byte position and its size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](word_breaks.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first segment between two word boundaries |
| [end](end.md) | the iterator past the last one |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the text is empty |
| [count](count.md) | the number of segments, walked |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the segments, carried by every range of the library
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
    string s = "can't stop, won't stop";
    for (auto w : txt::word_breaks(s)) {
        print("[{}]", w);
    }
    println();
    println("{} segments, {} words", txt::word_breaks(s).count(), txt::words(s).count());
}
```

Output:

```text
[can't][ ][stop][,][ ][won't][ ][stop]
8 segments, 4 words
```

## See also

- [graphemes](../graphemes/README.md), [words](../words/README.md), [sentences](../sentences/README.md), [line_breaks](../line_breaks/README.md): the other
  boundaries of a text
- [grapheme_count](../grapheme_count.md): the characters of a text counted
- [runes](../../core/runes/README.md): the code points of a text
