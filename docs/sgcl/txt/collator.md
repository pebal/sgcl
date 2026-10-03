[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::collator

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class collator;
}
```

`sgcl::txt::collator` puts text in the order a reader expects, which is not the order the bytes fall in. Sorted by
bytes, a Polish list ends `ćma łoś źrebak żaba` — every letter with a mark after every letter without one — and
`Zebra` comes before `ada`, because a capital Z is byte 0x5A and a small a is 0x61. The algorithm of
[UTS #10](https://www.unicode.org/reports/tr10/) gives every character weights at three levels — the letter, the
accent, the case — and compares them a level at a time, so that a difference of letters settles the question before
an accent is looked at, and an accent before a capital: `ada < Ala < zebra`, the case never entering into it, and
`resume < RESUME < résumé`, the accent outranking the case.

A collator is a comparator and a sort key in one: [compare](collator/compare.md) and
[operator()](collator/operator_call.md) put two texts in order, and [key](collator/key.md) gives the bytes that
compare the same way, for an index. It is ICU's `Collator` and Go's `collate.Collator`, with the order of the root
— the DUCET — and of 88 languages from CLDR; how much of a difference counts is a [strength](strength.md), and the
five settings CLDR names are [options](collator-options.md). It also finds one text inside another counting as equal what it
counts as equal ([find](collator/find.md)), section 8 of the algorithm.

The locale is the same [locale](locale.md) the case mappings take, and it reads the language subtag alone:
`locale("pl")`, `locale("pl-PL")` and `locale("pl_PL.UTF-8")` are one and the same.

## Rules

- A collator is a value of three words, cheap to copy, with nothing to free: it may live anywhere, and it is a
  comparator wherever one is asked for, a `std::sort` or the comparison of a [sorted_map](../core/sorted_map.md).
- Nothing in a collator changes after it is built: one collator may be used by any number of threads at once.
  Everything it was given is settled where it is built, so a setting that was not asked for costs nothing.
- Whatever the strength, a text is the same text however it was written: the elements are worked out on the
  decomposed form, so `café` written with one code point and with two compare equal at every level.
- A program that uses a collator links every language's tailoring, since the locale arrives while the program runs;
  one that never names `collator` links none of this, and no compilation flag is involved either way.

### The root order and a language's own

The root order is the **DUCET**, the default table of the standard, and it already knows a great deal: that a
capital is the same letter, that an accent is a smaller difference than a letter, that `ł` belongs beside `l`. A
collator with no locale uses it, and for a list in no particular language it is the right answer. A language then
moves a handful of letters, and those differences come from **CLDR**:

| Language | What it moves |
|---|---|
| Polish | `ą ć ę ł ń ó ś ź ż` each become a letter of their own, right after the one they are made from |
| Danish, Norwegian | `æ ø å` come after `z`, and `aa` sorts as `å` (`da` and `no`; `nb` and `nn` are not tags of their own here) |
| Swedish | `å ä ö` come after `z`, in that order — which is not the Danish one |
| German | nothing: `ä ö ü` stay where `a o u` are, which is what the root order already does. Only a phone book puts them elsewhere, and that is a collation of its own, not the language's |
| Czech | `ch` is one letter, between `h` and `i` |
| Hungarian | `cs dz dzs gy ly ny sz ty zs` are each one letter |
| Spanish | `ñ` comes after `n` |
| Turkish | `ç ğ ı i ö ş ü` take their own places, and a dotless `ı` is not an `i` |

88 languages are in the tables: `aa af ar as az bal be bn bo br bs ceb cs cu cy da dsb dz ee eo es et fa fi fil fo fy
gl gu ha haw hi hr hsb hu hy ig is kk kl km kn kok ku ky lkt ln lt lv mk ml mr mt my no nso om or pa pl ps ro sa se
si sk sl smn sq ssy sv ta te th tk tn to tr ug uk ur uz vi vo wae wo yi yo`. A language not on it asks for nothing
the root order does not already do: English, French, Italian and Dutch are there by their absence.
[tailored](collator/tailored.md) says whether the library has an order of its own for the locale it was given;
where it has none, the collator falls back to the root order, which is an answer rather than a failure. A tag that
names a script — `sr-Latn`, `ff-Adlm` — cannot be told from its language here, so those tailorings are not in the
tables.

### What is not implemented

One thing CLDR writes beside the rules is not: **`[reorder]`**, which moves a whole script rather than a letter —
Belarusian and Ukrainian ask for the Cyrillic letters before the Latin ones, Arabic and Persian for the Arabic ones,
and each of the ten Indic scripts for an order of its own. **35 of the 88 languages in the tables ask for it**, and
the generator names every one of them on each run so that it cannot become quiet again. Within the letters of one
script it changes nothing, so a list in one language is unaffected; what is lost is a list that mixes scripts, where
a Ukrainian reader expects the Cyrillic names before the Latin ones and gets them the other way round.

It is not implemented because it is not a setting but a change of the root order. Honouring it means partitioning
the space of first weights by script, and DUCET does not partition that way: of the 18538 first weights that
letters own, 72 are owned by letters of more than one script, and the 165 scripts fall into 182 runs. The partition
does exist — in the **CLDR root** table, which writes it as a lead byte per script in `FractionalUCA.txt` — but that
is a different weighting of the whole root from the DUCET this module is built on, with a conformance file of its
own. Moving to it is a piece of work of that size.

`[suppressContractions]` and `[normalization on]` are read and not honoured either; neither changes an answer here,
the first because the contractions it names are the root's and the second because every text is treated as though
it were normalized in any case.

Chinese, Japanese and Korean are not in the tables either. Chinese reorders every ideograph by its pronunciation and
would cost three megabytes; Japanese and Korean about ninety kilobytes each.

### The tables

The root order is 255.5 KB: 42.2 KB for the weight of a letter where it runs with the code point, 27.2 KB saying
which row of the index a letter that is not a plain one has, 159.1 KB for the rest, 26.2 KB of contractions. The
tailorings are 102.0 KB for all 88 languages together, of which 0.7 KB is a mask a language carries over the code
points its entries begin with — a language moves a handful of letters, and without it the tables would be searched
for every character of every text to find that out — and another 0.7 KB the settings each language asks for.
Ideographs and everything unassigned weigh by arithmetic rather than from a table, which is why a table that covers
the whole of Unicode has forty thousand entries and not a million.

A language needs somewhere to put its own letters, so the root's weights are held apart: between two neighbouring
letters of the root there are 65536 places at the first level, 128 at the second and 1024 at the third. That costs
nothing in the tables — the root's weights are stored as the standard writes them and shifted where they are
compared.

### What it is held to

The root order is held to `CollationTest` of the UCA, every one of its 206286 lines — a list of texts in the order
the standard puts them, where each must compare at or before the one after it, and its key must compare the same way
byte by byte. The test header carries every third line; the whole file is run by hand before a change lands.

The tailorings are held to **ICU**, which is the same standard from other hands and other data: 80 languages and
4663 texts, each language's own letters in the order ICU sorts them. The eight languages for which ICU carries no
tailoring of its own are named in the generated header and left out of that comparison — Church Slavonic among
them, which CLDR 46 has and the data ICU 78 is built from does not, so the three settings its rules ask for are
honoured here and cannot be checked against ICU.

The settings are held to ICU as well, and there in the way that catches what they invite: 29 settings of the
collator over 46 texts, the order ICU puts them in and, for every neighbouring pair, whether it calls them different
or equal. Every setting that changes the order changes the sort key with it, and that is the fault these invite — a
level written into the key that the comparison does not look at, or compared and not written, or written the wrong
way round — so both the comparison and the key are held to that, and a second test asks every combination of the
settings about every pair of a list of texts made for the purpose, some four hundred thousand pairs, and requires
the key to give the sign the comparison gives.

The search is held to the same rules the folded one is: that a match takes whole letters — `ss` finds a `ß`, `s`
does not find half of one, and an `a` is not half of a Danish `aa` — and that the positions of the elements ascend,
asked of a dozen texts made to be awkward (marks out of canonical order, a contraction reached across a mark,
Hangul, Tibetan) under every strength and both kinds of punctuation.

### Where it differs from ICU

A fuzzer asks the collator and ICU 78.3 the same questions about any text (`tests/txt/fuzz/txt_icu_fuzz.cpp`); where
they part by design or by version, this is where, and the fuzzer leaves those cases out by name.

| Difference | What it is |
|---|---|
| Unicode 16 and 17 | The tables are Unicode 16's and ICU 78's are 17's: a code point 17 assigned, a property 17 changed (`ʕ` is a cased letter in 16), and the ideographs, which UCA 17 weighs by radical and stroke where UCA 16 weighs them by block and code point, sort differently. |
| DUCET and CLDR's root | The root is the DUCET, ICU's is CLDR's, which weighs a few things its own way: U+FFFE and U+FFFF are its lowest and highest letters where the DUCET weighs them as any noncharacter, and some marks outside U+0300–U+036F (U+05B3 against U+0334) come at the second level in another order. |
| Shifted punctuation | `punctuation::shifted` shifts what the DUCET calls variable — spaces, punctuation and symbols; CLDR's groups give `ー` and `ｰ` to the symbols (the DUCET to the letters), and ICU's answers around a control between a variable element and its mark are no order at all (`_` < `_\x03\u0300` while each equals `_\u0300`). |
| Half-width voiced marks | `ﾞ` and `ﾟ` weigh only at the second level; ICU gives them the case of a normal kana there, and `case_level` and `case_order` give an element with no first weight no case here. |
| A letter of several elements with cases of their own | `case_level` and `case_order` read the case of a letter from its code points, ICU of each element from the root's third weight: `ǅ` is a titlecase letter here and a capital and a small letter to ICU. |
| A language's contraction across a mark | A language's contraction is matched where its code points stand together, not across a mark of a lower class between them as UCA S2.1 lets it: Swedish `Ô` followed by U+0334 is `ô` to ICU and `O` here. |
| `[reorder]` | Not honoured (above): a list that mixes scripts in a language that asks for it sorts in the root's order of scripts. |

## Member types

| Type | Definition |
|---|---|
| [match](collator-match.md) | where a search found its pattern: the byte position and the bytes of the text it covers |
| [options](collator-options.md) | the strength and the five settings CLDR names, what a collator is made with |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](collator/collator.md) | constructs a collator of the root order or of a language, with a strength or options |
| `(destructor)` | does nothing |

#### Comparison

| Function | Description |
|---|---|
| [compare](collator/compare.md) | compares two texts: negative, zero or positive |
| [equal](collator/equal.md) | checks whether two texts are one and the same to this collator |
| [operator()](collator/operator_call.md) | checks whether a text comes before another: the collator as a comparator |
| [key](collator/key.md) | the sort key of a text, bytes that compare the same way |
| [key_to](collator/key_to.md) | the sort key written into a buffer of the caller's |

#### Search

| Function | Description |
|---|---|
| [find](collator/find.md) | where a pattern is found in a text, counting as equal what the collator does |
| [contains](collator/contains.md) | checks whether a pattern is found in a text |
| [starts_with](collator/starts_with.md) | checks whether a text begins with a pattern |
| [ends_with](collator/ends_with.md) | checks whether a text ends with a pattern |

#### Observers

| Function | Description |
|---|---|
| [where](collator/where.md) | the locale it was made with |
| [level](collator/level.md) | the strength |
| [shifts_punctuation](collator/shifts_punctuation.md) | whether punctuation is shifted to the fourth level |
| [capitals_first](collator/capitals_first.md) | whether capitals come before small letters |
| [case_level](collator/case_level.md) | whether the case is a level of its own |
| [backwards](collator/backwards.md) | whether the accents are compared from the end |
| [numeric](collator/numeric.md) | whether a run of digits is compared as a number |
| [tailored](collator/tailored.md) | whether the library has an order of its own for the language |
| [operator==](collator/operator_cmp.md) | checks whether two collators put text in the same order |

## Complexity

A comparison walks the first level of both texts side by side and stops at the first letter that differs, which in
a sorted list is nearly always the first one: neither text is taken apart any further than that, and a short word
needs no allocation at all. At worst it is linear in the lengths of the two texts. A key is linear in the length of
its text ([Benchmarks: collation](benchmarks.md#collation)).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include <algorithm>

using namespace sgcl;

int main() {
    vector<string> words = {"żaba", "zamek", "ćma", "cebula", "łoś", "lis", "źrebak"};

    auto show = [&](const char* what, auto&& less) {
        auto list = words;
        std::sort(list.begin(), list.end(), less);
        print("{}", what);
        for (auto& w : list) {
            print("{} ", w);
        }
        println();
    };
    show("bajty  : ", [](const string& a, const string& b) { return a < b; });
    show("root   : ", txt::collator());
    show("polski : ", txt::collator(txt::locale("pl")));
}
```

Output:

```text
bajty  : cebula lis zamek ćma łoś źrebak żaba 
root   : cebula ćma lis łoś żaba zamek źrebak 
polski : cebula ćma lis łoś zamek źrebak żaba 
```

The root puts `żaba` before `zamek`, because to the root a `ż` is a `z` with a mark on it and `zaba` comes before
`zamek`. Polish makes it a letter of its own, after `z`, and the word moves to the end.

## See also

- [options](collator-options.md), [strength](strength.md): what a collator is asked for
- [collated_text](collated_text.md), [collated_searcher](collated_searcher.md),
  [collated_matches](collated_matches.md): a search by collation prepared for a loop
- [find_fold](find_fold.md), [find_normalized](find_normalized.md): the folded and normalized searches
- [locale](locale.md): the locale a collator takes
- [sorted_map](../core/sorted_map.md): takes a collator as its comparison
