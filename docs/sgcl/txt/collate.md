# txt::collate

```cpp
#include "sgcl/txt/collate.h"
```

Putting text in the order a reader expects, which is not the order the bytes fall in. Sorted by bytes, a Polish list ends `ćma łoś źrebak żaba` — every letter with a mark after every letter without one — and `Zebra` comes before `ada`, because a capital Z is byte 0x5A and a small a is 0x61.

The algorithm of [UTS #10](https://www.unicode.org/reports/tr10/) gives every character weights at three levels — the letter, the accent, the case — and compares them a level at a time, so that a difference of letters settles the question before an accent is looked at, and an accent before a capital.

```
resume  <  résumé  <  RESUME        # the same letters: the accent, then the case
ada     <  Ala     <  zebra         # different letters: the case never enters into it
```

## The names

```cpp
enum class strength : uint8_t { primary, secondary, tertiary };

class collator {
    collator() noexcept;                                          // the root order
    explicit collator(locale where, strength = strength::tertiary);
    explicit collator(strength);

    int compare(const string& a, const string& b) const;          // <0, 0, >0
    bool equal(const string& a, const string& b) const;
    vector<std::byte> key(const string& text) const;
    size_t key(const string& text, slice<std::byte> buffer) const;   // the bytes it takes
    bool operator()(const string& a, const string& b) const;      // a comparator

    locale where() const noexcept;
    strength level() const noexcept;
    bool tailored() const noexcept;
};
```

A collator is a value, cheap to copy, and it is a comparator: it stands wherever one is asked for, in a `std::sort` or as the comparison of a [`sorted_map`](../containers/sorted_map.md).

`key` is the same answer as a sequence of bytes. It compares byte by byte exactly the way the collator compares the texts, so it is worth making once for a text that is sorted or looked up many times, and worth storing in an index beside the text.

The second form writes into a buffer the caller owns and returns how many bytes the key takes — what a sort and an index builder want, where a key lives no longer than the pass that uses it and one allocation a word is the whole cost. When the key needs more than the buffer holds, **nothing is written** and the size still comes back: a truncated key would compare as a different text, which is worse than none. So a caller may ask with an empty buffer first and then size one, or simply try again with a larger one. Sorting a thousand words through the comparator costs about 300 ns a word; through keys written into a buffer on the stack, about 165.

## How much of a difference counts

`strength` says which levels are looked at. At **primary** strength `resume`, `résumé` and `RESUME` are one word — that is what a search box wants, and what a duplicate check wants. At **secondary** the accent is a difference and the case is not. At **tertiary**, the default, all three are, which is what a sorted list wants.

Whatever the strength, a text is the same text however it was written: the elements are worked out on the decomposed form, so `café` written with one code point and with two compare equal at every level.

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

CLDR writes three things beside the rules, as settings rather than as relations, and they are not implemented: `[alternate shifted]` (punctuation and spaces move to a fourth level, so that `re-sume` and `resume` compare alike), `[caseFirst upper]` (capitals before small letters, which Danish asks for), and `[backwards 2]` (accents compared from the end of the word, which Canadian French asks for). Four languages ask for one of them — Church Slavonic, Danish, Maltese and Thai — and for those the letters are in the right order but those three refinements are missing.

Chinese, Japanese and Korean are not in the tables either. Chinese reorders every ideograph by its pronunciation and would cost three megabytes; Japanese and Korean about ninety kilobytes each.

## The tables and the cost

The root order is 255.5 KB: 42.2 KB for the weight of a letter where it runs with the code point, 27.2 KB saying which row of the index a letter that is not a plain one has, 159.1 KB for the rest, 26.2 KB of contractions. The tailorings are 94.3 KB for all 88 languages together, of which 0.7 KB is a mask a language carries over the code points its entries begin with: a language moves a handful of letters, and without it the tables would be searched for every character of every text to find that out. Ideographs and everything unassigned weigh by arithmetic rather than from a table, which is why a table that covers the whole of Unicode has forty thousand entries and not a million.

A language needs somewhere to put its own letters, so the root's weights are held apart: between two neighbouring letters of the root there are 65536 places at the first level, 128 at the second and 1024 at the third. That costs nothing in the tables — the root's weights are stored as the standard writes them and shifted where they are compared.

A comparison costs about 27 ns for two short words on an ordinary machine in the root order and 51 in a language's, and a sort key about 64 ns into a buffer of the caller's; sorting a thousand words through the comparator is 380 ns a word. The elements are produced one at a time and the first level of both texts is walked side by side, so a comparison stops at the first letter that differs, which in a sorted list is nearly always the first one — neither text is taken apart any further than that, and a short word needs no allocation at all.

Nor is it taken apart at all where it need not be. The algorithm is defined on the decomposed form, but a letter with no mark behind it cannot be reordered and cannot join anything, so its weights are read where it stands — which is what the table's entries for the composed letters are for, and the only thing they are for, since a text that is decomposed first never reaches them. That is the condition UAX #15 calls FCD. A language's own letters are read the same way, the rules being closed canonically so that a composed ż meets the rule written on the decomposed one; a rule over several letters is left to the slower road, where the marks are in canonical order and one may stand further off than it is written.

Since the locale arrives while the program runs, a program that uses a collator at all links every language's tailoring. One that never names `collator` links none of this, and no compilation flag is involved either way.

## What it is held to

The root order is held to `CollationTest` of the UCA, every one of its 206286 lines — a list of texts in the order the standard puts them, where each must compare at or before the one after it, and its key must compare the same way byte by byte. The test header carries every third line; the whole file is run by hand before a change lands.

The tailorings are held to **ICU**, which is the same standard from other hands and other data: 77 languages and 4596 texts, each language's own letters in the order ICU sorts them. The four languages with an unimplemented setting and the seven for which ICU carries no tailoring are named in the generated header and left out of that comparison.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>

using namespace sgcl;

// A list of Polish words, put in order three ways: by the bytes, by the
// root order, and by what a Pole expects
int main() {
    vector<string> words = {"żaba", "zamek", "ćma", "cebula", "łoś", "lis", "źrebak"};

    auto show = [&](const char* what, auto&& less) {
        auto list = words;
        std::sort(list.begin(), list.end(), less);
        std::cout << what;
        for (auto& w : list) {
            std::cout << w << ' ';
        }
        std::cout << '\n';
    };
    show("bajty  : ", [](const string& a, const string& b) { return a < b; });
    show("root   : ", txt::collator());
    show("polski : ", txt::collator(txt::locale("pl")));

    // At primary strength an accent and a case are not differences, which
    // is what a search box wants
    txt::collator search{txt::strength::primary};
    std::cout << "\nresume == résumé == RESUME: "
              << (search.equal("resume", "résumé") && search.equal("resume", "RESUME")) << '\n';

    // A key compares byte by byte the way the collator compares texts, so
    // an index can hold it instead of calling the collator again
    txt::collator polish{txt::locale("pl")};
    std::cout << "klucz 'żaba' ma " << polish.key("żaba").size() << " bajtów\n";
    std::cout << "czy biblioteka zna porządek języka: " << polish.tailored() << " (pl), "
              << txt::collator(txt::locale("tlh")).tailored() << " (tlh)\n";
    return 0;
}
```

```
bajty  : cebula lis zamek ćma łoś źrebak żaba 
root   : cebula ćma lis łoś żaba zamek źrebak 
polski : cebula ćma lis łoś zamek źrebak żaba 

resume == résumé == RESUME: 1
klucz 'żaba' ma 36 bajtów
czy biblioteka zna porządek języka: 1 (pl), 0 (tlh)
```

The root puts `żaba` before `zamek`, because to the root a `ż` is a `z` with a mark on it and `zaba` comes before `zamek`. Polish makes it a letter of its own, after `z`, and the word moves to the end.
