[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::find

```cpp
optional<match> find(const string& text, const string& pattern, size_t from = 0) const noexcept;
```

Finds where the pattern is in the text at or after the byte `from`, counting as equal whatever this collator counts
as equal: at primary strength `resume` finds `résumé`, and with the punctuation shifted it finds `re-sume`. That is
what a search box wants, and it is [section 8](https://www.unicode.org/reports/tr10/#Searching) of the algorithm.
The position and the size are in the bytes of the text as it was given, not of any decomposed or folded form of it.

A match begins and ends on a boundary, and the boundary taken here is the **combining sequence**: a letter with the
marks that belong to it. That is the definition because it is the one the elements are already made on — the
algorithm gathers a letter and its marks before it weighs them — so a match can neither begin in the middle of an
`é` written as two code points nor end before the accent that belongs to the letter it ends on. Two smaller things
follow from the same rule: a match may not begin or end inside what one letter weighs, so a pattern of `a` does not
match the first half of an `æ` that a language weighs as two letters, and it may not cut a contraction, a Czech
`ch` being one letter and not an occurrence of `c`. It is the rule [find_fold](../find_fold.md) and
[find_normalized](../find_normalized.md) keep, asked through the same function.

What is left of the letter a match ends on may only be what this collator does not look at, and that makes the
answer the same for both spellings of a text: at primary strength `resume` finds a `résumé` written either way,
accent and all, and at any strength above it finds neither, because there the accent is a difference and the match
would be stopping in front of it.

An empty pattern is found where it is looked for, as it is in a string's own `find`. A pattern that is not empty
but that the collator does not look at — an accent on its own where the accents are not compared — is **not**
found: it would otherwise be found everywhere, which is no answer.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to search, UTF-8 |
| `pattern` | the text to look for |
| `from` | the byte of the text the search starts at |

## Return value

The [match](../collator-match.md) — the byte position and the bytes it covers in `text` — or an empty `optional`
when there is none at or after `from`.

## Complexity

Linear in the length of the text for the weighing, which is the whole cost; the search is linear in the text on
ordinary text and the text times the pattern at worst.

## Exceptions

None.

## Notes

The whole text is weighed on every call, into scratch the thread lends, so a loop over every occurrence weighs it
once for each and is quadratic: `from` is there for a caller who wants the next one, not to make that loop cheap.
A [collated_text](../collated_text/README.md) kept, or [collated_matches](../collated_matches/README.md), weighs the text once
([Benchmarks: collation](../benchmarks.md#collation)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{{.strength = txt::strength::primary}};
    auto hit = search.find("Le résumé du candidat", "resume");
    println("at {}, size {}", hit->at, hit->size);

    txt::collator czech(txt::locale("cs"), txt::strength::primary);
    println("{} {}", czech.find("chata", "c").has_value(), czech.find("chata", "ch").has_value());
    println("{}", search.find("résumé", "\u0301").has_value());
}
```

Output:

```text
at 3, size 8
false true
false
```

## See also

- [contains](contains.md), [starts_with](starts_with.md), [ends_with](ends_with.md)
- [collated_matches](../collated_matches/README.md): every occurrence, the text weighed once
- [sgcl::txt::collator](README.md)
