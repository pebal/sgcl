[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::collated_matches

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collated_matches;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::collated_matches` is every occurrence of a pattern in a text counting as equal what a
[collator](../collator/README.md) counts as equal, the text weighed a single time for all of them: the counterpart, for a
search by collation, of [fold_matches](../fold_matches/README.md). It is a range of the library
([mixin::enumerable](../../core/mixin/enumerable/README.md)), like [words](../words/README.md) and [graphemes](../graphemes/README.md):
constructed from the text and the pattern, deciding as it walks.

The element is a [slice](../../core/slice/README.md) of the original text, the bytes the match covers — which need not be as
many as the pattern has, a pattern of `resume` matching eight of them in `résumé` — and the
[iterator](../collated_matches-iterator/README.md) answers [pos](../collated_matches-iterator/pos.md) and
[size](../collated_matches-iterator/size.md) where the position rather than the bytes is wanted. The occurrences do
not overlap: the next is looked for past the end of the last. The rules of a match are those of
[collator::find](../collator/find.md).

## Rules

- The weighed text is too large to copy into every iterator, so the range holds it with the pattern in one tracked
  object, and the iterator points at that. A loop over a temporary is safe, and so is an iterator that outlives the
  range it came from.
- A pattern weighed by another collator than the range's is weighed again once, where the range is built, rather
  than at every step of the walk.
- An empty pattern matches nowhere: a range of every position is not what anyone asking this question wants, and it
  would not end.
- Nothing in a range changes after it is built: it may be walked by any number of threads at once.
- A copy shares the walk's state. A range moved from is the empty one, as one made with nothing; assigned to, it
  is the new one.

## Member types

| Type | Definition |
|---|---|
| `searcher_type` | [collated_searcher](../collated_searcher/README.md) |
| `value_type` | `slice<const char>` |
| `size_type` | `size_t` |
| [iterator](../collated_matches-iterator/README.md) | a forward iterator whose `*` is the slice of a match, with its byte position and size |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](collated_matches.md) | weighs the text and the pattern |
| `(destructor)` | drops the reference to the weighed text |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first occurrence |
| [end](end.md) | the iterator past the last occurrence |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether there is no occurrence |
| [count](count.md) | the number of occurrences, walked |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the text the range walks |
| [pattern](pattern.md) | the pattern, weighed |

#### From mixin::enumerable

The questions asked of the matches, carried by every range of the library
([mixin::enumerable](../../core/mixin/enumerable/README.md)).

| Function | Description |
|---|---|
| [contains](../../core/mixin/enumerable/contains.md) | checks whether a match is equal to a text, byte for byte |
| `index_of` | the position, in matches, of the first one equal to a text |
| `find_index` | the position, in matches, of the first one the predicate accepts |
| `exists` | checks whether the predicate accepts some match |
| `all` | checks whether the predicate accepts every match |
| `count_of` | the number of matches the predicate accepts |
| `for_each` | calls a function with every match |

## Complexity

Building weighs the text, linear in its length. A whole walk is one scan of the weighed elements, linear on
ordinary text and the text times the pattern at worst, where a loop over [collator::find](../collator/find.md) weighs
the text again for every occurrence and is quadratic ([Benchmarks: collation](../benchmarks.md#collation)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{{.strength = txt::strength::primary}};
    string text = "Résumé, resume, RESUME and re-sume";
    for (auto m : txt::collated_matches(search, text, "resume")) {  // slices of the text
        print("[{}]", m);
    }
    println();
    txt::collated_text weighed{search, text};  // or keep the text
    auto pattern = weighed.searcher("resume");  // and the pattern
    println("{}", weighed.count(pattern));
}
```

Output:

```text
[Résumé][resume][RESUME]
3
```

## See also

- [collated_searcher](../collated_searcher/README.md): the pattern weighed once
- [collated_text](../collated_text/README.md): a text weighed once and asked many questions
- [fold_matches, normalized_matches](../fold_matches/README.md): every occurrence without regard to case or spelling
