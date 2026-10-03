[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::fold_matches, normalized_matches

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using fold_matches = /* every occurrence without regard to case */;
    using normalized_matches = /* every occurrence without regard to how the text was written */;
}
```

`sgcl::txt::fold_matches` is every occurrence of a pattern in a text without regard to case, the text folded a
single time for all of them; `sgcl::txt::normalized_matches` is the same without regard to the way either side was
written, the text decomposed once. The two are one class over two mappings, with one interface. They are ranges of
the library ([mixin::enumerable](../core/mixin/enumerable.md)), like [words](words.md) and
[graphemes](graphemes.md): constructed from the text and the pattern, allocating nothing per element, deciding as
they walk.

The element is a [slice](../core/slice.md) of the original text, the bytes the match covers — which need not be as
many as the pattern has, `"STRASSE"` covering the seven bytes of `"straße"` — and the
[iterator](fold_matches-iterator.md) answers [pos](fold_matches-iterator/pos.md) and
[size](fold_matches-iterator/size.md) where the position rather than the bytes is wanted. The occurrences do not
overlap: the next is looked for past the end of the last, as [searcher::count](searcher/count.md) counts them. A
match takes whole characters and whole combining sequences, as [find_fold](find_fold.md) says, so every match is a
slice with something in it.

## Rules

- The mapped text is too large to copy into an iterator, so the range holds it, the text and the pattern in one
  tracked object, and the iterator points at that. A range lives where a `tracked_ptr` may: on a stack or inside a
  managed object. A loop over a temporary is safe, and so is an iterator that outlives the range it came from.
- An empty pattern matches nowhere: a range of every position is not what anyone asking this question wants, and it
  would not end.
- Nothing in a range changes after it is built: it may be walked by any number of threads at once.
- A copy shares the walk's state. A range moved from is the empty one, its text and its pattern empty, as one made
  with nothing; assigned to, it is the new one.

## Member types

| Type | Definition |
|---|---|
| `searcher_type` | [fold_searcher](fold_searcher.md) for `fold_matches`, [normalized_searcher](fold_searcher.md) for `normalized_matches` |
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](fold_matches-iterator.md) | a forward iterator whose `*` is the slice of a match, with its byte position and size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](fold_matches/fold_matches.md) | maps the text and the pattern |
| `(destructor)` | drops the reference to the mapped text |

#### Iterators

| Function | Description |
|---|---|
| [begin](fold_matches/begin.md) | an iterator to the first occurrence |
| [end](fold_matches/end.md) | the iterator past the last occurrence |

#### Capacity

| Function | Description |
|---|---|
| [empty](fold_matches/empty.md) | checks whether there is no occurrence |
| [count](fold_matches/count.md) | the number of occurrences, walked |

#### Observers

| Function | Description |
|---|---|
| [text](fold_matches/text.md) | the text the range walks |
| [pattern](fold_matches/pattern.md) | the pattern, mapped |
| [points](fold_matches/points.md) | the mapped text: its code points and the byte each came from |

#### From mixin::enumerable

The questions asked of the matches, carried by every range of the library
([mixin::enumerable](../core/mixin/enumerable.md)).

| Function | Description |
|---|---|
| [contains](../core/mixin/enumerable/contains.md) | checks whether a match is equal to a text |
| `index_of` | the position, in matches, of the first one equal to a text |
| `find_index` | the position, in matches, of the first one the predicate accepts |
| `exists` | checks whether the predicate accepts some match |
| `all` | checks whether the predicate accepts every match |
| `count_of` | the number of matches the predicate accepts |
| `for_each` | calls a function with every match |

## Complexity

Building maps the text and the pattern, linear in their lengths. A whole walk is a scan of the mapped text, linear
in it on ordinary text and the text times the pattern at worst: one pass, where a loop over
[find_fold](find_fold.md) maps the text again for every occurrence and is quadratic
([Benchmarks: search](benchmarks.md#search)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string line = "Kot, KOT, kot i Kotek";
    for (auto m : txt::fold_matches(line, "kot")) {
        print("[{}]", m);
    }
    println();
    println("{}", txt::normalized_matches("café, cafe\u0301", "café").count());
}
```

Output:

```text
[Kot][KOT][kot][Kot]
2
```

## See also

- [fold_searcher, normalized_searcher](fold_searcher.md): the pattern mapped once
- [folded_text, normalized_text](folded_text.md): a text mapped once and asked many questions
- [collated_matches](collated_matches.md): every occurrence by a collator
