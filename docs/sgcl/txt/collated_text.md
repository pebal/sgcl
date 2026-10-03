[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::collated_text

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collated_text;
}
```

`sgcl::txt::collated_text` is a text weighed once by a [collator](collator.md) and asked as often as you like
whether a pattern is in it, counting as equal what the collator counts as equal: the counterpart, for a search by
collation, of [folded_text](folded_text.md). Weighing is the whole cost of such a search — every letter goes through
the tables, the contractions are matched, the marks are put in canonical order, more than folding or decomposing
costs — and that cost is the text's, not the pattern's. [collator::find](collator/find.md) weighs the text on every
call, which is the right shape for one question and makes a loop over the occurrences quadratic; a caller with more
than one question builds this and keeps it ([Benchmarks: collation](benchmarks.md#collation)).

A pattern asked of it is a string, weighed on the call, or a [collated_searcher](collated_searcher.md) weighed once;
a searcher of another collator is weighed again with the text's own. The rules of a match — whole combining
sequences, no half of what one letter weighs, an empty pattern found where it is looked for and a pattern of
nothing the collator looks at found nowhere — are those of [collator::find](collator/find.md).

## Rules

- A collated text holds its [collator](collator.md), the text as a [string](../core/string.md) and the weighed
  elements, so it lives where a string may: on a stack or inside a managed object. It keeps the text as the string
  it is, not as a slice: a slice is two raw pointers into a managed buffer, which may not live inside a managed
  object, and a [collated_matches](collated_matches.md) keeps one of these in one.
- Nothing in it changes after it is built: one text may be asked by any number of threads at once.
- The positions it answers with are bytes of the text as it was given. Each element carries the start of the
  combining sequence it came from, and those positions ascend — the canonical ordering moves a mark inside a
  sequence and never out of one — so finding where a search starts is a bisection, and a loop over the occurrences
  is linear. That is not true of the folded search, where the ordering carries a mark's position with the mark.
- A copy is the same text. A text moved from is the empty text of the root collator, as one made with nothing;
  assigned to, it is the new one.

## Member types

| Type | Definition |
|---|---|
| `searcher_type` | [collated_searcher](collated_searcher.md) |
| `match` | [collator::match](collator-match.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](collated_text/collated_text.md) | weighs the text by a collator |
| `(destructor)` | drops the text and its elements |
| [searcher](collated_text/searcher.md) | a pattern weighed the way this text was |

#### Search

| Function | Description |
|---|---|
| [find](collated_text/find.md) | where a pattern is found at or after a position |
| [contains](collated_text/contains.md) | checks whether a pattern is found |
| [starts_with](collated_text/starts_with.md) | checks whether the text begins with a pattern |
| [ends_with](collated_text/ends_with.md) | checks whether the text ends with a pattern |
| [count](collated_text/count.md) | the number of occurrences that do not overlap |

#### Observers

| Function | Description |
|---|---|
| [text](collated_text/text.md) | the text, as a slice |
| [bytes](collated_text/bytes.md) | the text, as the string it holds |
| [by](collated_text/by.md) | the collator it was weighed by |
| [size](collated_text/size.md) | the number of elements it weighed to |
| [at](collated_text/at.md) | where in the text an element came from |
| [empty](collated_text/empty.md) | checks whether it weighed to nothing |

## Complexity

Building is linear in the length of the text. A search from a byte starts by a bisection over the elements; the
scan is linear in the rest of the text on ordinary text and the text times the pattern at worst.
[starts_with](collated_text/starts_with.md) and [ends_with](collated_text/ends_with.md) try one place.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{{.strength = txt::strength::primary}};
    string text = "Résumé, resume, RESUME and re-sume";
    txt::collated_text weighed{search, text};
    auto pattern = weighed.searcher("resume");
    println("{} {} {}", weighed.count(pattern), weighed.starts_with(pattern),
            weighed.find(pattern, 1)->at);
}
```

Output:

```text
3 true 10
```

## See also

- [collated_searcher](collated_searcher.md): a pattern weighed once
- [collated_matches](collated_matches.md): every occurrence, as a range
- [collator::find](collator/find.md): the one-shot form, and the rules
- [folded_text, normalized_text](folded_text.md): a text folded or decomposed once
