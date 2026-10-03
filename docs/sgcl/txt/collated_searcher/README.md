[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::collated_searcher

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collated_searcher;
}
```

`sgcl::txt::collated_searcher` is a pattern weighed once by a [collator](../collator/README.md), for the loop that asks the
same question of many texts: the counterpart, for a search by collation, of [fold_searcher](../fold_searcher/README.md). It
keeps the pattern as it was given, as [searcher](../searcher/README.md) does, so that it can say what it looks for, and the
collator it was weighed with, since the weights are that collator's. A [collated_text](../collated_text/README.md) and
[collated_matches](../collated_matches/README.md) take it as their pattern.

A searcher made with one collator and asked of a text made with another answers about neither: its elements were
filtered by what the one collator looks at and are then compared by what the other calls equal — a searcher weighed
with the punctuation shifted has no hyphen among its elements, and a text weighed with it counted once found
`re-sume` inside `resume` that way, an answer neither collator gives. So the text weighs the pattern again with its
own collator, and the answer is the one the text's collator would have given a pattern of its own making. Weighing
again rather than refusing, because a refusal reads as "not found", a wrong answer that looks like an ordinary one,
while the weighing costs the pattern and never the text. It happens only where a caller mixed two collators, which
is a mistake and not a road; [collated_text::searcher](../collated_text/searcher.md) gives a searcher that belongs to
the text's collator.

## Rules

- A searcher holds a [collator](../collator/README.md), a [string](../../core/string/README.md) and the weighed elements, so it lives
  where a string may: on a stack or inside a managed object.
- Nothing in it changes after it is built: one searcher may be used by any number of threads at once.
- A copy is the same pattern. A searcher moved from is the empty pattern of the root collator, its `pattern()`
  empty too; assigned to, it is the new one.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](collated_searcher.md) | weighs the pattern by a collator |
| `(destructor)` | drops the pattern and its elements |

#### Observers

| Function | Description |
|---|---|
| [pattern](pattern.md) | the pattern as it was given |
| [by](by.md) | the collator it was weighed by |
| [size](size.md) | the number of elements the collator looks at |
| [empty](empty.md) | checks whether the collator looks at nothing of the pattern |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{txt::strength::primary};
    txt::collated_searcher resume(search, "resume");
    for (string title : {"Résumé tips", "RESUME", "Résister"}) {
        print("{} ", txt::collated_text(search, title).contains(resume));
    }
    println();
}
```

Output:

```text
true true false 
```

## See also

- [collated_text](../collated_text/README.md): a text weighed once
- [collated_matches](../collated_matches/README.md): every occurrence, as a range
- [collator::find](../collator/find.md): the rules of a search by collation
