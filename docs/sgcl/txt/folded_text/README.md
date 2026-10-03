[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::folded_text, normalized_text

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using folded_text = /* a text folded once */;
    using normalized_text = /* a text decomposed once */;
}
```

`sgcl::txt::folded_text` is a text folded once ([fold_case](../fold_case.md)) and asked as often as you like without
regard to case; `sgcl::txt::normalized_text` is a text decomposed and put in canonical order once, asked without
regard to the way it was written. The two are one class over two mappings, with one interface: what this page and
the pages of the members say of one holds for the other.

Mapping the text is the whole cost of such a search — over sixty-four kilobytes it is some three hundred
microseconds, and a search through the mapped text some twenty — and that cost is the text's, not the pattern's.
[find_fold](../find_fold.md) and [find_normalized](../find_normalized.md) map both sides on every call, which is the
right shape for one question and makes a loop over the occurrences quadratic; a caller with more than one question
builds this and keeps it, and its questions cost only the search. A pattern asked of it can be a string or a
prepared [fold_searcher](../fold_searcher/README.md). A match takes whole characters and whole combining sequences, as
`find_fold` and `find_normalized` say, and every road answers the same.

## Rules

- A text holds a [slice](../../core/slice/README.md) of the text it was built from and [vectors](../../core/vector/README.md) of the
  mapping, so it lives where those may: on a stack or inside a managed object. The slice keeps the text alive.
- Nothing in it changes after it is built: one text may be asked by any number of threads at once.
- The positions it answers with are bytes of the text as it was given, not of the mapped copy.
- The mapping is kept as a code point and its byte position in the text for every code point the text mapped to,
  twelve bytes each.
- A copy is the same text. A text moved from is the empty text, as one made with nothing; assigned to, it is the
  new one.

## Member types

| Type | Definition |
|---|---|
| `searcher_type` | [fold_searcher](../fold_searcher/README.md) for a `folded_text`, [normalized_searcher](../fold_searcher/README.md) for a `normalized_text` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](folded_text.md) | maps the text |
| `(destructor)` | drops the slice and the mapping |

#### Search

| Function | Description |
|---|---|
| [find](find.md) | the first occurrence of a pattern at or after a position: where it begins and the bytes it covers |
| [contains](contains.md) | checks whether a pattern occurs in the text |
| [count](count.md) | the number of occurrences of a pattern that do not overlap |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the text it was built from |
| [size](size.md) | the number of code points the text mapped to |
| [empty](empty.md) | checks whether the text mapped to nothing |
| [points](points.md) | the mapped text: its code points and the byte each came from |

## Complexity

Building is linear in the length of the text. A search starts by a bisection over the positions, so a search from
any byte costs no walk to it; the scan is linear in the rest of the text on ordinary text and the text times the
pattern at worst.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string text = "Kot, KOT, kot i Kotek";
    txt::folded_text ft(text);  // one text, many questions
    println("{} {} {}", ft.find(txt::fold_searcher("kot"))->pos, ft.count("kot"),
            ft.contains("KOTEK"));

    txt::fold_searcher f("straße");  // one pattern, many texts
    for (string t : {"STRASSE", "Strasse 5", "ulica"}) {
        print("{} ", txt::folded_text(t).contains(f));
    }
    println();
}
```

Output:

```text
0 4 true
true true false 
```

## See also

- [fold_searcher, normalized_searcher](../fold_searcher/README.md): a pattern mapped once
- [fold_matches, normalized_matches](../fold_matches/README.md): every occurrence, as a range
- [find_fold](../find_fold.md), [find_normalized](../find_normalized.md): the one-shot forms
- [collated_text](../collated_text/README.md): a text weighed once by a collator
