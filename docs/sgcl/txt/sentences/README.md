[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::sentences

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using sentences = /* unspecified */;
}
```

`txt::sentences` is the text cut where one sentence ends and the next begins, by [UAX
#29](https://www.unicode.org/reports/tr29/). A full stop is not enough and not always needed: `"i.e."` and `"U.S.A."`
stay inside one sentence, a stop followed by a lower case letter ends nothing, and a paragraph separator ends a
sentence without any punctuation at all.

The range is constructed from the text, `txt::sentences(s)`, which looks like a call and is a construction, as
[runes](../../core/runes/README.md) is. Its element is a sentence, a [slice](../../core/slice/README.md) of the text, and its
[iterator](../sentences-iterator/README.md) knows the byte position of the element and its size, for the code that goes back to
the bytes. It is a range of the library ([mixin::enumerable](../../core/mixin/enumerable/README.md)): `count_of`, `exists`,
`find_if` and `for_each` are asked of the sentences.

## Rules

- A `sentences` holds a slice of the text, so it lives where a `tracked_ptr` may: on a stack or inside a managed
  object ([the rules of core](../../core/README.md#the-rules), 1). A range over a temporary string is safe: the slice
  keeps the string's object.
- Nothing is copied and nothing is allocated per element: an element is a [slice](../../core/slice/README.md) of the text, found
  as the walk reaches it. An invalid byte of UTF-8 is one code point, `U+FFFD`.
- An iterator refers to the text, not to the range: it is valid while the text's object lives.
- Every byte of the text belongs to exactly one sentence, the space after the stop with the sentence it closes.
- These are the rules of the annex and nothing more, so an initial before a capitalised name, `"Pan J. Kowalski"`, is
  two sentences. Doing better needs a list of a language's abbreviations, which the annex leaves to the caller.
- The oracle is `SentenceBreakTest.txt` of the UCD, every one of its 512 cases, with no rule tailored and none
  skipped. The table is Sentence_Break, 17.7 KB.
- `sentences` is an alias of a class template of the library that [graphemes](../graphemes/README.md),
  [word_breaks](../word_breaks/README.md), [words](../words/README.md) and `sentences` share, each its own type; the template is not part
  of the interface.
- A copy is the same range. A range moved from is the empty one, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](../sentences-iterator/README.md) | a forward iterator whose `*` is a sentence, with its byte position and its size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sentences.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first sentence |
| [end](end.md) | the iterator past the last one |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the text is empty |
| [count](count.md) | the number of sentences, walked |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the sentences, carried by every range of the library
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
    string s = "Ala ma kota, i.e. a cat. It is black! Is it? yes.";
    for (auto x : txt::sentences(s)) {
        println("[{}]", x);
    }
}
```

Output:

```text
[Ala ma kota, i.e. a cat. ]
[It is black! ]
[Is it? ]
[yes.]
```

## See also

- [graphemes](../graphemes/README.md), [word_breaks](../word_breaks/README.md), [words](../words/README.md), [line_breaks](../line_breaks/README.md): the
  other boundaries of a text
- [grapheme_count](../grapheme_count.md): the characters of a text counted
- [runes](../../core/runes/README.md): the code points of a text
