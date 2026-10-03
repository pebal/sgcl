[sgcl](../README.md) › [txt](README.md) › [collator](collator/README.md)

# sgcl::txt::collator::options

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collator {
    public:
        struct options {
            txt::strength strength = txt::strength::tertiary;
            optional<txt::punctuation> punctuation;    // CLDR ka / alternate
            optional<txt::case_order> case_order;      // CLDR kf / caseFirst
            optional<bool> case_level;                 // CLDR kc / caseLevel
            optional<bool> backwards;                  // CLDR kb, the accents from the end
            bool numeric = false;                      // CLDR kn, file9 before file10
        };
    };
}
```

`sgcl::txt::collator::options` is what a [collator](collator/README.md) is asked for beside putting the letters in order: how much of
a difference counts, and the five settings CLDR names, under the names CLDR gives them. They are a value handed to
the constructor rather than methods that change a collator afterwards: a collator is a comparator, copied into a
sort and shared between threads, and a setter would make it a thing that can change under one of them. An
aggregate also reads at the call site the way the setting reads in CLDR: `collator{locale("da"), {.numeric =
true}}`.

Four of the five are things a language asks for, and those four are left unset by default: a collator made for a
locale starts with what that language's rules ask for, and what the caller writes is what it wants instead. Danish
and Maltese ask for their capitals first, Thai for its punctuation shifted aside, Church Slavonic for the first
three together with its accents read from the end. `numeric` is not among them because no language asks for it —
it is a thing a program wants for its file names, never a thing a language wants for its words. The `optional`
says "not given, so take the language's", which is a question about what was asked for; what the collator settled
on is asked of it directly, and the answer is a `bool`: [shifts_punctuation](collator/shifts_punctuation.md),
[capitals_first](collator/capitals_first.md), [case_level](collator/case_level.md),
[backwards](collator/backwards.md), [numeric](collator/numeric.md).

## Member objects

| Field | Description |
|---|---|
| `strength` | how much of a difference counts, a [strength](strength.md); `tertiary` by default |
| `punctuation` | [punctuation::shifted](punctuation.md) moves punctuation, spaces and symbols to a fourth level, so that `re-sume` and `resume` are one word — look as though the hyphen were not there; the fourth level itself is compared only at `strength::quaternary`, which keeps the two apart while keeping them together. Unset by default: the language's answer |
| `case_order` | [case_order::upper_first](case_order.md) puts the capitals before the small letters, `lower_first` after. The case is read from the letter and not from its weights, so it holds for a letter a language moved as well. Unset by default: the language's answer |
| `case_level` | the case becomes a level of its own, between the accents and the third level, so that a collator can be asked for the letters and the case and nothing else: at primary strength with it, `resume` and `résumé` are one word and `RESUME` is not. Kana take the case CLDR gives them from the third weight of the root, a small kana a small letter. Unset by default: the language's answer |
| `backwards` | the accents are compared from the end of the word rather than from its start, what Canadian French asks for: `cote côte coté côté` rather than `cote coté côte côté`. Unset by default: the language's answer |
| `numeric` | a run of digits is compared as the number it spells, so `plik9` comes before `plik10`. The digits of any script count, the leading zeros do not (`007` and `7` are one number), and the number may be longer than any integer holds: what is compared is first how many digits are left after the zeros and then the digits themselves. A number sorts at the start of the digits, as CLDR has it: after everything that sorts before them and before every character that is no decimal digit but sorts as one (`⓪`, `₀`, `↉`), so `a12` comes before `a⓪`. `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator files{{.numeric = true}};
    txt::collator search{{.strength = txt::strength::primary,
                          .punctuation = txt::punctuation::shifted}};
    txt::collator names{txt::locale("da"), {.case_order = txt::case_order::lower_first}};
    println("plik9 before plik10: {}", files("plik9", "plik10"));
    println("resume == résumé: {}, resume == re-sume: {}", search.equal("resume", "résumé"),
            search.equal("resume", "re-sume"));
    println("a before A in Danish, small letters first: {}", names("a", "A"));
}
```

Output:

```text
plik9 before plik10: true
resume == résumé: true, resume == re-sume: true
a before A in Danish, small letters first: true
```

## See also

- [collator](collator/README.md): what takes them
- [strength](strength.md), [punctuation](punctuation.md), [case_order](case_order.md)
