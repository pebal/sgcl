# sgcl::txt::collate

```cpp
#include "sgcl/txt/collate.h"   // or "sgcl/txt/txt.h"

enum class strength : uint8_t { primary, secondary, tertiary, quaternary };
enum class punctuation : uint8_t { counted, shifted };
enum class case_order : uint8_t { natural, upper_first, lower_first };

struct options;              // the five settings CLDR names, and the strength
class collator;              // a comparator and a sort key, in the order of a language
class collated_searcher;     // a pattern weighed once
class collated_text;         // a text weighed once
class collated_matches;      // every occurrence, as a range
```

Putting text in the order a reader expects, which is not the order the bytes fall in. Sorted by bytes, a Polish list ends `ćma łoś źrebak żaba` — every letter with a mark after every letter without one — and `Zebra` comes before `ada`, because a capital Z is byte 0x5A and a small a is 0x61.

The algorithm of [UTS #10](https://www.unicode.org/reports/tr10/) gives every character weights at three levels — the letter, the accent, the case — and compares them a level at a time, so that a difference of letters settles the question before an accent is looked at, and an accent before a capital.

```text
resume  <  résumé  <  RESUME        # the same letters: the accent, then the case
ada     <  Ala     <  zebra         # different letters: the case never enters into it
```

## How much of a difference counts

`strength` says which levels are looked at. At **primary** strength `resume`, `résumé` and `RESUME` are one word — that is what a search box wants, and what a duplicate check wants. At **secondary** the accent is a difference and the case is not. At **tertiary**, the default, all three are, which is what a sorted list wants.

Whatever the strength, a text is the same text however it was written: the elements are worked out on the decomposed form, so `café` written with one code point and with two compare equal at every level.

## The settings

Beside how much of a difference counts, CLDR names five things a collator can be asked for, and `options` carries them under the names CLDR gives them. They are a value handed to the constructor rather than methods that change a collator afterwards: a collator is a comparator, copied into a sort and shared between threads, and a setter would make it a thing that can change under one of them. An aggregate also reads at the call site the way the setting reads in CLDR.

| | |
|---|---|
| `numeric` | a run of digits is compared as the number it spells, so `plik9` comes before `plik10`. The digits of any script count, the leading zeros do not (`007` and `7` are one number), and the number may be longer than any integer holds: what is compared is first how many digits are left after the zeros and then the digits themselves |
| `punctuation` | `shifted` moves punctuation, spaces and symbols to a fourth level, so that `re-sume` and `resume` are one word — look as though the hyphen were not there. The fourth level itself is only compared at `strength::quaternary`, which keeps the two apart while keeping them together |
| `case_level` | the case becomes a level of its own, between the accents and the third level, so that a collator can be asked for the letters and the case and nothing else: at primary strength with it, `resume` and `résumé` are one word and `RESUME` is not |
| `case_order` | `upper_first` puts the capitals before the small letters. The case of a letter is read from the letter and not from its weights, so it holds for a letter a language moved as well: Danish `Œ` and Maltese `Għ` sort where ICU sorts them, and a letter written in both cases — Danish sorts `Aa` as one letter — falls between the two, as it does there. Kana have no case, and here, for `case_level` and `case_order` only, they take the one CLDR gives them from the third weight of the root: a small kana (ぁ, ッ, ｧ) is a small letter and a normal one (あ, ツ, ｱ) a capital, as in ICU |
| `backwards` | the accents are compared from the end of the word rather than from its start, which is what Canadian French asks for: `cote côte coté côté` rather than `cote coté côte côté` |

Four of the five are things a language asks for, and those four are the ones left unset by default: a collator made for a locale starts with what that language's rules ask for, and what the caller writes is what it wants instead. Danish and Maltese ask for their capitals first, Thai for its punctuation shifted aside, Church Slavonic for all of the first three together with its accents read from the end. `numeric` is not among them because no language asks for it — it is a thing a program wants for its file names, never a thing a language wants for its words. The `optional` says "not given, so take the language's", which is a question about what was *asked for*; what the collator *settled on* is asked of it directly, and an answer is a `bool`: `c.case_level()`, `c.capitals_first()`, `c.shifts_punctuation()`, `c.backwards()`, `c.numeric()`. Each of the five has two values that matter once it is settled, so nothing is lost by saying so in a bool, and a name that reads as a question keeps the type names free — a member called `punctuation` would hide the type of that name inside the class.

Every setting that changes the order changes the sort key with it. That is the fault these invite — a level written into the key that the comparison does not look at, or compared and not written, or written the wrong way round — so the test asks every setting about every pair of a list of texts and holds `key` to `compare`, some four hundred thousand pairs.

## Finding one text inside another

A collator finds a pattern in a text counting as equal whatever it counts as equal: at primary strength `resume` finds `résumé`, and with the punctuation shifted it finds `re-sume`. That is what a search box wants, and it is [section 8](https://www.unicode.org/reports/tr10/#Searching) of the algorithm.

The position and the size are in the bytes of the text as it was given, not of any decomposed or folded form of it, and the size is the text's own: six letters of a pattern are found in eight bytes of text where an accent takes bytes the pattern never had.

A match begins and ends on a boundary, and the boundary taken here is the **combining sequence**: a letter with the marks that belong to it. That is the definition because it is the one the elements are already made on — the algorithm gathers a letter and its marks before it weighs them — so a match can neither begin in the middle of an `é` written as two code points nor end before the accent that belongs to the letter it ends on. Two smaller things follow from the same rule: a match may not begin or end inside what one letter weighs, so a pattern of `a` does not match the first half of an `æ` that a language weighs as two letters, and it may not cut a contraction, a Czech `ch` being one letter and not an occurrence of `c`.

An empty pattern is found where it is looked for, as it is in a string's own `find`. A pattern that is not empty but that the collator does not look at — an accent on its own where the accents are not compared — is **not** found: it would otherwise be found everywhere, which is no answer.

### Asked once, and asked in a loop

Weighing the text is the whole cost of such a search — more so than folding or decomposing one, since every letter goes through the tables, the contractions are matched and the marks are put in canonical order — and that cost is the text's, not the pattern's. So both sides can be prepared, in the shapes [`search`](search.md) has for its two searches: `collated_searcher`, a pattern weighed once, and `collated_text`, a text weighed once.

`collator::find` weighs both sides on every call, so a loop over the occurrences weighs the text once for each of them and is quadratic. Over 64 KB of text with 1902 occurrences in it, one pass costs **1.19 s** that way and **1.10 ms** through `collated_matches`, of which 0.63 ms is the weighing — a thousandfold, and the same lesson the folded search learned.

The positions of the elements ascend, so finding where a search starts is a bisection and a loop over the occurrences is linear. That is not true of the folded search, where the canonical ordering carries a mark's position with the mark; here an element carries the bytes of the combining sequence it came from, and the ordering moves a mark inside a sequence and never out of one.

## The root order and a language's own

The root order is the **DUCET**, the default table of the standard, and it already knows a great deal: that a capital is the same letter, that an accent is a smaller difference than a letter, that `ł` belongs beside `l`. A `collator()` with no locale uses it, and for a list in no particular language it is the right answer.

A language then moves a handful of letters, and those differences come from **CLDR**:

| | |
|---|---|
| Polish | `ą ć ę ł ń ó ś ź ż` each become a letter of their own, right after the one they are made from |
| Danish, Norwegian | `æ ø å` come after `z`, and `aa` sorts as `å` (`da` and `no`; `nb` and `nn` are not tags of their own here) |
| Swedish | `å ä ö` come after `z`, in that order — which is not the Danish one |
| German | nothing: `ä ö ü` stay where `a o u` are, which is what the root order already does. Only a phone book puts them elsewhere, and that is a collation of its own, not the language's |
| Czech | `ch` is one letter, between `h` and `i` |
| Hungarian | `cs dz dzs gy ly ny sz ty zs` are each one letter |
| Spanish | `ñ` comes after `n` |
| Turkish | `ç ğ ı i ö ş ü` take their own places, and a dotless `ı` is not an `i` |

88 languages are in the tables — the list is `aa af ar as az bal be bn bo br bs ceb cs cu cy da dsb dz ee eo es et fa fi fil fo fy gl gu ha haw hi hr hsb hu hy ig is kk kl km kn kok ku ky lkt ln lt lv mk ml mr mt my no nso om or pa pl ps ro sa se si sk sl smn sq ssy sv ta te th tk tn to tr ug uk ur uz vi vo wae wo yi yo`. A language not on it asks for nothing the root order does not already do: English, French, Italian and Dutch are there by their absence. `tailored()` says whether the library has an order of its own for the locale it was given; where it has none, the collator falls back to the root order, which is an answer rather than a failure.

The locale is the same [`locale`](case.md) the case mappings take, and it reads the language subtag alone: `locale("pl")`, `locale("pl-PL")` and `locale("pl_PL.UTF-8")` are one and the same. A tag that names a script — `sr-Latn`, `ff-Adlm` — cannot be told from its language here, so those tailorings are not in the tables.

## What is not implemented

One thing CLDR writes beside the rules is not: **`[reorder]`**, which moves a whole script rather than a letter — Belarusian and Ukrainian ask for the Cyrillic letters before the Latin ones, Arabic and Persian for the Arabic ones, and each of the ten Indic scripts for an order of its own. **35 of the 88 languages in the tables ask for it**, and the generator names every one of them on each run so that it cannot become quiet again.

Within the letters of one script it changes nothing, so a list in one language is unaffected; what is lost is a list that mixes scripts, where a Ukrainian reader expects the Cyrillic names before the Latin ones and gets them the other way round.

It is not implemented because it is not a setting but a change of the root order. Honouring it means partitioning the space of first weights by script, and DUCET does not partition that way: of the 18538 first weights that letters own, 72 are owned by letters of more than one script, and the 165 scripts fall into 182 runs. The partition does exist — in the **CLDR root** table, which writes it as a lead byte per script in `FractionalUCA.txt` — but that is a different weighting of the whole root from the DUCET this module is built on, with a conformance file of its own. Moving to it is a piece of work of that size and not of this one.

`[suppressContractions]` and `[normalization on]` are read and not honoured either; neither changes an answer here, the first because the contractions it names are the root's and the second because every text is treated as though it were normalized in any case.

Chinese, Japanese and Korean are not in the tables either. Chinese reorders every ideograph by its pronunciation and would cost three megabytes; Japanese and Korean about ninety kilobytes each.

## The tables and the cost

The root order is 255.5 KB: 42.2 KB for the weight of a letter where it runs with the code point, 27.2 KB saying which row of the index a letter that is not a plain one has, 159.1 KB for the rest, 26.2 KB of contractions. The tailorings are 102.0 KB for all 88 languages together, of which 0.7 KB is a mask a language carries over the code points its entries begin with — a language moves a handful of letters, and without it the tables would be searched for every character of every text to find that out — and another 0.7 KB the settings each language asks for. Ideographs and everything unassigned weigh by arithmetic rather than from a table, which is why a table that covers the whole of Unicode has forty thousand entries and not a million.

A language needs somewhere to put its own letters, so the root's weights are held apart: between two neighbouring letters of the root there are 65536 places at the first level, 128 at the second and 1024 at the third. That costs nothing in the tables — the root's weights are stored as the standard writes them and shifted where they are compared.

A comparison costs about 27 ns for two short words on an ordinary machine in the root order and 51 in a language's, and a sort key about 64 ns into a buffer of the caller's; sorting a thousand words through the comparator is 380 ns a word. A setting that was not asked for costs nothing at all: the collator settles once, where it is built, whether anything it was given changes what an element weighs or how a weight is read, and where nothing does, the comparison, the key and the stream that feeds them are the code they were before any setting existed. Measured on two words that differ in their fourth letter, a comparison is 44.7 ns with the settings in the header and 44.5 without them, a Polish one 182 against 183, and a key 43.2 against 43.4. The elements are produced one at a time and the first level of both texts is walked side by side, so a comparison stops at the first letter that differs, which in a sorted list is nearly always the first one — neither text is taken apart any further than that, and a short word needs no allocation at all.

Nor is it taken apart at all where it need not be. The algorithm is defined on the decomposed form, but a letter with no mark behind it cannot be reordered and cannot join anything, so its weights are read where it stands — which is what the table's entries for the composed letters are for, and the only thing they are for, since a text that is decomposed first never reaches them. That is the condition UAX #15 calls FCD. A language's own letters are read the same way, the rules being closed canonically so that a composed ż meets the rule written on the decomposed one; a rule over several letters is left to the slower road, where the marks are in canonical order and one may stand further off than it is written.

Since the locale arrives while the program runs, a program that uses a collator at all links every language's tailoring. One that never names `collator` links none of this, and no compilation flag is involved either way.

## What it is held to

The root order is held to `CollationTest` of the UCA, every one of its 206286 lines — a list of texts in the order the standard puts them, where each must compare at or before the one after it, and its key must compare the same way byte by byte. The test header carries every third line; the whole file is run by hand before a change lands.

The tailorings are held to **ICU**, which is the same standard from other hands and other data: 80 languages and 4663 texts, each language's own letters in the order ICU sorts them. The eight languages for which ICU carries no tailoring of its own are named in the generated header and left out of that comparison — Church Slavonic among them, which CLDR 46 has and the data ICU 78 is built from does not, so the three settings its rules ask for are honoured here and cannot be checked against ICU.

The search is held to the same rules the folded one is: that a match takes whole letters — `ss` finds a `ß`, `s` does not find half of one, and an `a` is not half of a Danish `aa` — and that the positions of the elements ascend, asked of a dozen texts made to be awkward (marks out of canonical order, a contraction reached across a mark, Hangul, Tibetan) under every strength and both kinds of punctuation.

The settings are held to ICU as well, and there in the way that catches what they invite: 29 settings of the collator over 46 texts, the order ICU puts them in and, for every neighbouring pair, whether it calls them different or equal. Both the comparison and the sort key are held to that, and a second test asks every combination of the settings about every pair of a list of texts made for the purpose — some four hundred thousand pairs — and requires the key to give the sign the comparison gives.

## Where the collator differs from ICU

A fuzzer asks the collator and ICU 78.3 the same questions about any text (`tests/txt/fuzz/txt_icu_fuzz.cpp`); where they part by design or by version, this is where, and the fuzzer leaves those cases out by name.

| | |
|---|---|
| Unicode 16 and 17 | The tables are Unicode 16's and ICU 78's are 17's: a code point 17 assigned, a property 17 changed (`ʕ` is a cased letter in 16), and the ideographs, which UCA 17 weighs by radical and stroke where UCA 16 weighs them by block and code point, sort differently. |
| DUCET and CLDR's root | The root is the DUCET, ICU's is CLDR's, which weighs a few things its own way: U+FFFE and U+FFFF are its lowest and highest letters where the DUCET weighs them as any noncharacter, and some marks outside U+0300–U+036F (U+05B3 against U+0334) come at the second level in another order. |
| Shifted punctuation | `punctuation::shifted` shifts what the DUCET calls variable — spaces, punctuation and symbols; CLDR's groups give `ー` and `ｰ` to the symbols (the DUCET to the letters), and ICU's answers around a control between a variable element and its mark are no order at all (`_` < `_\x03\u0300` while each equals `_\u0300`). |
| Numbers | With `numeric`, a number weighs from the first weight of its digit zero on here and below it in ICU, which parts only against the digits that are no decimal digits (`₀`, `¹`, `①`), whose first weight is a digit's. |
| Half-width voiced marks | `ﾞ` and `ﾟ` weigh only at the second level; ICU gives them the case of a normal kana there, and `case_level` and `case_order` give an element with no first weight no case here. |
| A letter of several elements with cases of their own | `case_level` and `case_order` read the case of a letter from its code points, ICU of each element from the root's third weight: `ǅ` is a titlecase letter here and a capital and a small letter to ICU. |
| A language's contraction across a mark | A language's contraction is matched where its code points stand together, not across a mark of a lower class between them as UCA S2.1 lets it: Swedish `Ô` followed by U+0334 is `ô` to ICU and `O` here. |
| `[reorder]` | Not honoured (above): a list that mixes scripts in a language that asks for it sorts in the root's order of scripts. |

## Members

### options

```cpp
struct options {
    txt::strength strength = txt::strength::tertiary;
    optional<txt::punctuation> punctuation;    // CLDR ka / alternate
    optional<txt::case_order> case_order;      // CLDR kf / caseFirst
    optional<bool> case_level;                 // CLDR kc / caseLevel
    optional<bool> backwards;                  // CLDR kb, the accents from the end
    bool numeric = false;                      // CLDR kn, file9 before file10
};
```

Handed to the constructor. The four settings a language asks for are left unset and take the language's answer, as [the settings](#the-settings) say.

### collator

```cpp
class collator {
    collator() noexcept;                                          // the root order
    explicit collator(locale where, strength = strength::tertiary);
    explicit collator(strength);
    explicit collator(const options&);
    collator(locale where, const options&);

    int compare(const string& a, const string& b) const;          // <0, 0, >0
    bool equal(const string& a, const string& b) const;
    vector<byte> key(const string& text) const;
    size_t key_to(const slice<byte>& buffer, const string& text) const;   // the bytes it takes (the buffer first, as format_to)
    bool operator()(const string& a, const string& b) const;      // a comparator

    struct match { size_t at; size_t size; };                     // bytes of the text
    optional<match> find(const string& text, const string& pattern, size_t from = 0) const;
    bool contains(const string& text, const string& pattern) const;
    bool starts_with(const string& text, const string& pattern) const;
    bool ends_with(const string& text, const string& pattern) const;

    locale where() const noexcept;
    strength level() const noexcept;
    bool shifts_punctuation() const noexcept;                     // what it settled on:
    bool capitals_first() const noexcept;                         // the language's answer
    bool case_level() const noexcept;                             // where the caller gave
    bool backwards() const noexcept;                              // none, the caller's
    bool numeric() const noexcept;                                // where it gave one
    bool tailored() const noexcept;
};
```

A collator is a value, cheap to copy, and it is a comparator: it stands wherever one is asked for, in a `std::sort` or as the comparison of a [`sorted_map`](../core/sorted_map.md).

`key` is the same answer as a sequence of bytes. It compares byte by byte exactly the way the collator compares the texts, so it is worth making once for a text that is sorted or looked up many times, and worth storing in an index beside the text.

The second form writes into a buffer the caller owns and returns how many bytes the key takes — what a sort and an index builder want, where a key lives no longer than the pass that uses it and one allocation a word is the whole cost. When the key needs more than the buffer holds, **nothing is written** and the size still comes back: a truncated key would compare as a different text, which is worse than none. So a caller may ask with an empty buffer first and then size one, or simply try again with a larger one. Sorting a thousand words through the comparator costs about 300 ns a word; through keys written into a buffer on the stack, about 165.

### collated_searcher

```cpp
class collated_searcher {                                 // a pattern weighed once
    collated_searcher(const collator&, const string& pattern);
    const string& pattern() const noexcept;
    size_t size() const noexcept;                         // elements the collator looks at
    bool empty() const noexcept;
};
```

A pattern weighed once by a collator and asked of many texts; `collated_text::searcher` gives one that belongs to the text's collator.

### collated_text

```cpp
class collated_text {                                     // a text weighed once
    collated_text(const collator&, const string& text);
    collated_text(const collator&, const slice<const char>& text);
    template<size_t N>
    collated_text(const collator&, const char (&text)[N]);  // to its first NUL or its end
    collated_text(const collator&, const char* text);       // char* too: to its NUL
    collated_searcher searcher(const string& pattern) const;

    optional<collator::match> find(const collated_searcher&, size_t from = 0) const;
    optional<collator::match> find(const string& pattern, size_t from = 0) const;
    bool contains(...) const;
    bool starts_with(...) const;
    bool ends_with(...) const;
    size_t count(...) const;                              // the occurrences that do not overlap

    const slice<const char>& text() const noexcept;
    size_t size() const noexcept;                         // elements it weighed to
    size_t at(size_t i) const noexcept;                   // and where each came from
};
```

A text weighed once and asked as often as you like.

### collated_matches

```cpp
class collated_matches {                                  // every occurrence, as a range
    ...
    template<size_t N>
    collated_matches(const collator&, const char (&text)[N], collated_searcher pattern);  // to its first NUL or its end
    collated_matches(const collator&, const char* text, collated_searcher pattern);       // char* too: to its NUL
};
```

Each element of the range is the slice of the original text the match covers, and the iterator answers `pos()` and `size()` where the position rather than the bytes is wanted. The occurrences do not overlap: the next is looked for past the end of the last. A searcher belongs to the collator it was weighed with, and `collated_text::searcher` is the way to get one that belongs to the same.

## Examples

A list of Polish words, put in order three ways: by the bytes, by the root order, and by what a Pole expects:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"
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
    return 0;
}
```

Output:

```text
bajty  : cebula lis zamek ćma łoś źrebak żaba 
root   : cebula ćma lis łoś żaba zamek źrebak 
polski : cebula ćma lis łoś zamek źrebak żaba 
```

The root puts `żaba` before `zamek`, because to the root a `ż` is a `z` with a mark on it and `zaba` comes before `zamek`. Polish makes it a letter of its own, after `z`, and the word moves to the end.

At primary strength an accent and a case are not differences, which is what a search box wants:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{txt::strength::primary};
    println("resume == résumé == RESUME: {}",
            (search.equal("resume", "résumé") && search.equal("resume", "RESUME")));
    return 0;
}
```

Output:

```text
resume == résumé == RESUME: true
```

A key compares byte by byte the way the collator compares texts, so an index can hold it instead of calling the collator again:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::collator polish{txt::locale("pl")};
    println("klucz 'żaba' ma {} bajtów", polish.key("żaba").size());
    println("czy biblioteka zna porządek języka: {} (pl), {} (tlh)", polish.tailored(),
            txt::collator(txt::locale("tlh")).tailored());
    return 0;
}
```

Output:

```text
klucz 'żaba' ma 36 bajtów
czy biblioteka zna porządek języka: true (pl), false (tlh)
```

The settings, a value handed to the constructor:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

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
    return 0;
}
```

Output:

```text
plik9 before plik10: true
resume == résumé: true, resume == re-sume: true
a before A in Danish, small letters first: true
```

A pattern found in a text, counting as equal what the collator counts as equal:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    txt::collator search{{.strength = txt::strength::primary}};
    auto hit = search.find("Le résumé du candidat", "resume");
    println("at {}, size {}", hit->at, hit->size);
    return 0;
}
```

Output:

```text
at 3, size 8
```

Every occurrence, the text weighed once for all of them:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

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
    return 0;
}
```

Output:

```text
[Résumé][resume][RESUME]
3
```

## See also

[The module](README.md); [`case`](case.md), the `locale` a collator takes; [`search`](search.md), the folded and normalized searches; [`sorted_map`](../core/sorted_map.md), which takes a collator as its comparison; [`normalize`](normalize.md), `compare_normalized`.
