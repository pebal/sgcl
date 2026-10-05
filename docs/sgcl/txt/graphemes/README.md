[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::graphemes

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using graphemes = /* unspecified */;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`txt::graphemes` is the grapheme clusters of a text, by [UAX #29](https://www.unicode.org/reports/tr29/): what a
reader calls a character, which a code point is not. A grapheme is one combining sequence (`"é"` written as an `e` and
a combining acute is one), one flag (two regional indicators), one emoji with its skin tone or its joined family, one
Hangul syllable written as jamo, one Devanagari consonant with its vowel sign or its conjunct, and one `"\r\n"`. It is
what a caret steps over, what a backspace deletes and what a limit of "twenty characters" should count;
[grapheme_count](../grapheme_count.md) counts it, and [grapheme_next](../grapheme_next.md),
[grapheme_prev](../grapheme_prev.md) and [grapheme_start](../grapheme_start.md) move a cursor by it.

The range is constructed from the text, `txt::graphemes(s)`, which looks like a call and is a construction, as
[runes](../../core/runes/README.md) is. Its element is a grapheme cluster, a [slice](../../core/slice/README.md) of the text, and its
[iterator](../graphemes-iterator/README.md) knows the byte position of the element and its size, for the code that goes back to
the bytes. It is a range of the library ([mixin::enumerable](../../core/mixin/enumerable/README.md)): `count_of`, `exists`,
`find_if` and `for_each` are asked of the grapheme clusters.

## Rules

- A `graphemes` holds a slice of the text. A range over a temporary string is safe: the slice keeps the string's object.
- Nothing is copied and nothing is allocated per element: an element is a [slice](../../core/slice/README.md) of the text, found
  as the walk reaches it. An invalid byte of UTF-8 is one code point, `U+FFFD`.
- An iterator refers to the text, not to the range: it is valid while the text's object lives.
- Every byte of the text belongs to exactly one grapheme.
- Two ASCII characters are always separate graphemes, unless they are a carriage return and a line feed (rule GB3): a
  run of them is walked in one step rather than one a character.
- The oracle is `GraphemeBreakTest.txt` of the UCD, every one of its 1093 cases, with no rule tailored and none
  skipped. The tables are Grapheme_Cluster_Break, 9.5 KB, and Indic_Conjunct_Break, 3.3 KB, for rule GB9c.
- `graphemes` is an alias of a class template of the library that `graphemes`, [word_breaks](../word_breaks/README.md),
  [words](../words/README.md) and [sentences](../sentences/README.md) share, each its own type; the template is not part of the interface.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](../graphemes-iterator/README.md) | a forward iterator whose `*` is a grapheme cluster, with its byte position and its size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](graphemes.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first grapheme cluster |
| [end](end.md) | the iterator past the last one |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the text is empty |
| [count](count.md) | the number of grapheme clusters, walked |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the grapheme clusters, carried by every range of the library
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
    string s = "e\u0301\U0001F1F5\U0001F1F1 \u1100\u1161\u11A8";  // e and an acute, a flag, jamo
    println("{} bytes, {} code points, {} characters", s.size(), s.rune_count(),
            txt::graphemes(s).count());
    for (auto g : txt::graphemes(s)) {
        print("[{}]", g);
    }
    println();
}
```

Output:

```text
21 bytes, 8 code points, 4 characters
[é][🇵🇱][ ][각]
```

## See also

- [word_breaks](../word_breaks/README.md), [words](../words/README.md), [sentences](../sentences/README.md), [line_breaks](../line_breaks/README.md): the
  other boundaries of a text
- [grapheme_count](../grapheme_count.md): the characters of a text counted
- [runes](../../core/runes/README.md): the code points of a text
