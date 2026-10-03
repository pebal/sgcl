[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::find_normalized

```cpp
#include "sgcl/txt/search.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    optional<occurrence> find_normalized(const string& text, const string& pattern,
                                         size_t from = 0) noexcept;
}
```

Finds the first occurrence of the pattern in the text without regard to the way either side was written, at or
after the byte `from`: both sides are decomposed and their marks put in canonical order ([nfd](nfc_t.md)) and the
decomposed text is searched. An `"é"` of one code point finds an `"é"` of two, and a Korean syllable finds its
jamo — what a search over names and file paths wants. What comes back is the position and the size **in the text
as it was given**, not in the decomposed copy of it.

A match takes whole characters and whole combining sequences. `"e"` does not find the `"e"` that an `"é"` comes
apart into, and one jamo does not find a Korean syllable; `"cafe"` is not found in `"café"` however that text is
written, because the acute belongs to the letter the match would stop on. That holds where canonical ordering has
pulled a decomposition apart: `"ḋ"` with a dot below it becomes `d`, dot-below, dot-above, the dot-below of the
second character standing between the two parts of the first, and neither `"d"` nor `"ḍ"` nor `"ḋ"` is found in it,
while the whole letter is, however either side spells it. The reported bytes are those of the characters the match
took, from the first to the last; in a text that begins with marks written out of canonical order, a match of them
begins at byte 0 and covers them all, which is what the [collated search](collator/find.md) answers too. The rule
is the one [find_fold](find_fold.md) keeps.

An empty pattern is found at `from` while `from` is not past the end of the text.

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

Linear in the lengths of the text and of the pattern for the decomposition; the scan is linear in the text on
ordinary text and the text times the pattern at worst.

## Exceptions

None.

## Notes

Both sides are decomposed **on every call**, so a loop over the occurrences is quadratic:
[normalized_matches](fold_matches/README.md), or a [normalized_text](folded_text/README.md) kept, decomposes the text once
([Benchmarks: search](benchmarks.md#search)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string composed = "rue café";  // e with an acute, one code point
    string typed = "cafe\u0301";  // and two
    println("na bajcie {}, a jako bajty: {}", txt::find_normalized(composed, typed)->pos,
            (composed.find(typed) == npos ? "nie ma" : "jest"));
    println("{} {}", txt::find_normalized("café", "e").has_value(),
            txt::find_normalized("cafe\u0301", "e").has_value());
}
```

Output:

```text
na bajcie 4, a jako bajty: nie ma
false false
```

## See also

- [contains_normalized](contains_normalized.md): whether there is an occurrence
- [normalized_matches](fold_matches/README.md): every occurrence, the text decomposed once
- [find_fold](find_fold.md): blind to case
- [nfd](nfc_t.md): the decomposition
