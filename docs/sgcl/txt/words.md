[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::words

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using words = /* unspecified */;
}
```

`txt::words` is the words of a text, by [UAX #29](https://www.unicode.org/reports/tr29/): the segments of
[word_breaks](word_breaks.md) with a letter or a digit in them, the runs of spaces and the punctuation between them
left out. `"can't stop, won't stop"` is four words.

The range is constructed from the text, `txt::words(s)`, which looks like a call and is a construction, as
[runes](../core/runes.md) is. Its element is a word, a [slice](../core/slice.md) of the text, and its
[iterator](words-iterator.md) knows the byte position of the element and its size, for the code that goes back to the
bytes. It is a range of the library ([mixin::enumerable](../core/mixin/enumerable.md)): `count_of`, `exists`,
`find_if` and `for_each` are asked of the words.

## Rules

- A `words` holds a slice of the text, so it lives where a `tracked_ptr` may: on a stack or inside a managed object
  ([the rules of core](../core/README.md#the-rules), 1). A range over a temporary string is safe: the slice keeps the
  string's object.
- Nothing is copied and nothing is allocated per element: an element is a [slice](../core/slice.md) of the text, found
  as the walk reaches it. An invalid byte of UTF-8 is one code point, `U+FFFD`.
- An iterator refers to the text, not to the range: it is valid while the text's object lives.
- The rules keep an apostrophe and a decimal point inside a word (`don't`, `3.14`, `192.168.0.1`) and cut at a hyphen
  (`e-mail` is two words).
- The oracle and the table are those of [word_breaks](word_breaks.md).
- `words` is an alias of a class template of the library that [graphemes](graphemes.md),
  [word_breaks](word_breaks.md), `words` and [sentences](sentences.md) share, each its own type; the template is not
  part of the interface.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](words-iterator.md) | a forward iterator whose `*` is a word, with its byte position and its size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](words/words.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](words/begin.md) | an iterator to the first word |
| [end](words/end.md) | the iterator past the last one |

#### Capacity

| Function | Description |
|---|---|
| [empty](words/empty.md) | checks whether there is no word |
| [count](words/count.md) | the number of words, walked |

#### Observers

| Function | Description |
|---|---|
| [text](words/text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the words, carried by every range of the library
([mixin::enumerable](../core/mixin/enumerable.md)).

| Function | Description |
|---|---|
| [exists](../core/mixin/enumerable/exists.md) | checks whether the predicate accepts some element |
| [all](../core/mixin/enumerable/all.md) | checks whether the predicate accepts every element |
| [count_of](../core/mixin/enumerable/count_of.md) | the number of elements the predicate accepts |
| [find_if](../core/mixin/enumerable/find_if.md) | the first element the predicate accepts |
| [for_each](../core/mixin/enumerable/for_each.md) | calls a function with every element |

## Complexity

A step of the iterator is linear in the bytes of the element it finds; `count` and every question of
[mixin::enumerable](../core/mixin/enumerable.md) walk the text, linear in its bytes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string s = "can't stop, won't stop: 3.14 e-mail";
    for (auto w : txt::words(s)) {
        print("[{}]", w);
    }
    println();
    println("{} words", txt::words(s).count());
}
```

Output:

```text
[can't][stop][won't][stop][3.14][e][mail]
7 words
```

## See also

- [graphemes](graphemes.md), [word_breaks](word_breaks.md), [sentences](sentences.md), [line_breaks](line_breaks.md):
  the other boundaries of a text
- [grapheme_count](grapheme_count.md): the characters of a text counted
- [runes](../core/runes.md): the code points of a text
