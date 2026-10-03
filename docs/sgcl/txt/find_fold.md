[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::find_fold

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    optional<occurrence> find_fold(const string& text, const string& pattern,
                                   size_t from = 0) noexcept;
}
```

Finds the first occurrence of the pattern in the text without regard to case, at or after the byte `from`. Both
sides are folded by the full folding of [fold_case](fold_case.md) and the folded text is searched, so `"STRASSE"`
finds `"straße"` and `"σίσυφος"` finds `"ΣΊΣΥΦΟΣ"`. What comes back is the position and the size **in the text as
it was given**, not in the folded copy of it, although folding made them different lengths.

A match takes whole characters: it may not cut what one character folds to in two. `"ss"` finds a `"ß"`, which is
the whole of what folding makes of it, and so does `"ß"` itself; `"s"` does not find half of one. And it takes
whole combining sequences, a letter with the marks that belong to it: `"cafe"` is not found in `"café"`, however
that text is written. The rule is the one [find_normalized](find_normalized.md) and the
[collated search](collator/find.md) keep, asked through the same function, so the three cannot answer differently
about one text. A match holds every code point of every character it touches, and nothing of any other: every match
is a piece of the text that can be cut out, highlighted or drawn round.

An empty pattern is found at `from` while `from` is not past the end of the text, as `std::string::find` and
[searcher::find](searcher/find.md) answer; the bound is the text's own size, not the size of its folded copy.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the text to look for |
| `from` | the byte of the text the search starts at |

## Return value

The [occurrence](occurrence.md) — the byte position and the bytes it covers in `text` — or an empty `optional`
when there is none at or after `from`.

## Complexity

Linear in the lengths of the text and of the pattern for the folding; the scan is linear in the text on ordinary
text and the text times the pattern at worst.

## Exceptions

None.

## Notes

Both sides are folded **on every call**. That is the right shape for one question and the wrong one for a loop:
a loop over the occurrences folds the text again for each of them and is quadratic. [fold_matches](fold_matches.md),
or a [folded_text](folded_text.md) kept, folds the text once ([Benchmarks: search](benchmarks.md#search)).

The folded and the normalized searches are held to one another and to what they are defined to do. The prepared
forms are checked against the one-shot ones over **2000 random texts** built of Polish, German and Greek pieces, at
three starting positions each, and the ranges against the count the prepared text gives. The rest is checked with
the letters that make them interesting: `ß` against `SS`, a Greek sigma in both its shapes, `"café"` written either
way, a Korean syllable against its jamo, and the marks of one letter in either order. Two edges have tests of their
own, because both were faults. A refused match must not end the search — after turning one down the scan goes on
from the next position, and there is a case for a `ß` standing in front of a real `s`, for two in a row, and for a
refusal in the middle of a run that must not stop the count. And the canonical ordering moves marks, so a search
from a byte offset inside a sequence whose marks it put in order is a case of its own, as is a text that begins with
marks out of order and every byte of a text with marks out of order in four places, each answered as a walk from
the front answers it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto found = txt::find_fold("Die straße", "STRASSE");
    println("STRASSE w 'Die straße': {}, bajtów {}", found->pos, found->size);
    println("{} {}", txt::find_fold("straße", "ss")->pos,
            (txt::find_fold("aßb", "s") ? "znalezione" : "nic"));
}
```

Output:

```text
STRASSE w 'Die straße': 4, bajtów 7
4 nic
```

## See also

- [contains_fold](contains_fold.md): whether there is an occurrence
- [fold_matches](fold_matches.md): every occurrence, the text folded once
- [folded_text](folded_text.md), [fold_searcher](fold_searcher.md): a text, a pattern folded once
- [find_normalized](find_normalized.md): blind to the way a text was written
- [fold_case](fold_case.md): the folding
