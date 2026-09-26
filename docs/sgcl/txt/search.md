# txt::search

```cpp
#include "sgcl/txt/search.h"
```

Finding a text inside a text, three ways for three questions. The bytes as they stand, which is what a parser wants. Blind to case, which is what a person searching wants. And blind to the way the text was written, which is what a search over names and file paths wants, since `"é"` typed in two code points must find `"é"` stored in one.

## The names

```cpp
class searcher {                                   // a pattern of bytes, prepared once
    explicit searcher(const string& pattern);
    size_t find(const string& text, size_t from = 0) const noexcept;   // npos when not there
    bool contains(const string& text) const noexcept;
    size_t count(const string& text) const noexcept;                   // occurrences that do not overlap
    const string& pattern() const noexcept;
};

struct occurrence { size_t pos; size_t size; };   // where a blind search found its pattern: the bytes of the text it covers

// Ask once
optional<occurrence> find_fold(const string& text, const string& pattern, size_t from = 0);
bool contains_fold(const string& text, const string& pattern);
optional<occurrence> find_normalized(const string& text, const string& pattern, size_t from = 0);
bool contains_normalized(const string& text, const string& pattern);

// Ask many times
class fold_searcher;          // a pattern folded once: find(text, from), contains(text), count(text), as searcher
class normalized_searcher;    // a pattern decomposed once, likewise
class folded_text;            // a text folded once, and asked as often as you like
class normalized_text;        // a text decomposed once, likewise
class fold_matches;           // every occurrence, the text folded once for all of them
class normalized_matches;     // the same without regard to how either side was written
```

## The prepared pattern

`searcher` is Boyer–Moore–Horspool over the bytes: the pattern is looked at once, a table of skips is built from it, and after that a pattern of *m* bytes is found in *n* bytes in about *n/m* steps on ordinary text. It is for the loop that looks for the same thing in many texts — a single search is what [`string::find`](../core/string.md) is for.

The bytes are safe to search. UTF-8 synchronises itself: the first byte of a character cannot appear inside another one, so a match of valid UTF-8 inside valid UTF-8 always begins on a character, and no test for that is needed.

A `searcher` holds a [`string`](../core/string.md), so it lives on the stack or inside a managed object, like any other value of the library that holds one.

## Blind to case, blind to how it was written

`find_fold` folds both sides ([`fold_case`](case.md)) and searches the folded text; `find_normalized` decomposes both sides and puts the marks in canonical order ([`normalize`](normalize.md)) instead. So `"STRASSE"` finds `"straße"`, and an `"é"` of one code point finds an `"é"` of two and a Korean syllable finds its jamo. Either way what comes back is an `occurrence`, the position and the size **in the text as it was given** — not in the folded or decomposed copy of it, although folding and decomposing made them different lengths: `"STRASSE"` covers the seven bytes of `"straße"`, not the seven of the pattern, and a position alone would not say where the match ends.

Neither can work on the bytes, because folding and decomposing change them. Both map the text to code points first, keeping the position in the original beside every one of them, and that mapping — not the searching — is where the time goes.

## Ask once, or ask many times

`find_fold` and `find_normalized` map **both sides on every call**. That is the right shape for one question and the wrong one for a loop, because the text is then mapped again for every occurrence and the loop is quadratic. The prepared forms map each side once:

| a text of 64 KB, every occurrence found | a call at a time | a range |
|---|---|---|
| `find_fold` / `fold_matches`, 878 of them | 337 ms | **0.41 ms** |
| `find_normalized` / `normalized_matches`, 1747 | 648 ms | **0.41 ms** |

At 256 KB, where the shape shows itself properly, it is 4.65 s against 1.51 ms and 9.46 s against 1.54 ms. Four times the text costs sixteen times as much one way and four times the other.

So: ask `find_fold` when you want one answer, and one of these when you want more than one.

```cpp
// every occurrence, as a range — the text folded once for all of them
for (auto m : txt::fold_matches(text, pattern)) {
    // m is a slice of the original text: the bytes this match covers
}

// one text, many questions
txt::folded_text ft(text);
ft.find(txt::fold_searcher(pattern));
ft.count(other);
ft.contains(third);

// one pattern, many texts
txt::fold_searcher f(pattern);
for (auto& text : texts) {
    txt::folded_text(text).find(f);
}
```

`fold_matches` and `normalized_matches` are ranges of the library, like [`words` and `graphemes`](segment.md): constructed from the text and the pattern, allocating nothing per element, deciding as they walk. The element is a [`slice`](../core/slice.md) of the original text — the bytes the match covers — and the iterator answers `pos()` and `size()` where the position rather than the bytes is wanted. The occurrences **do not overlap**: the next is looked for past the end of the last, as `searcher::count` counts them.

The mapped text is too large to copy into an iterator, so the range holds it in a tracked object and the iterator points at that. A loop over a temporary is safe, and so is an iterator that outlives the range it came from.

## A match takes whole characters

A match may not cut an expansion or a decomposition in two. `"ss"` finds a `"ß"`, which is the whole of what folding makes of it, and so does `"ß"` itself; `"s"` does not find half of one. `"e"` does not find the `"e"` that an `"é"` comes apart into, and one jamo does not find a Korean syllable.

And it takes whole combining sequences — a letter and the marks that belong to it. `"cafe"` is not found in `"café"` however that text is written, with the `"é"` as one code point or as an `"e"` and an acute of its own, because the acute belongs to the letter the match would stop on; `"e"` is not found in `"cafe\u0301"` for the same reason. This is the rule the [collated search](collate.md) keeps as well, and both ask the same function, so the two cannot answer differently about one text.

The two rules are asked of the two ends of a match, and together they are a rule about the whole of it: a match holds every code point of every character it touches, and nothing of any other. That holds where canonical ordering has pulled a decomposition apart — `"ḋ"` with a dot below it becomes `d`, dot-below, dot-above, the dot-below of the second character standing between the two parts of the first — and neither `"d"` nor `"ḍ"` nor `"ḋ"` is found in it, while the whole letter is, however either side spells it. The position and size a match reports are the bytes of those characters, from the first to the last. In a text that begins with marks written out of canonical order, a match of them begins at byte 0 and covers them all, which is what the collated search answers too.

```cpp
find_fold(string("straße"), string("ss"));    // {4, 2}: the ß
find_fold(string("aßb"), string("s"));        // nothing
find_normalized(string("café"), string("e"));        // nothing
find_normalized(string("cafe\u0301"), string("e"));   // nothing — the acute belongs to the e
```

Without the rule a match could begin and end in the middle of one character, and what came back was a position with no text at it — nothing a caller can cut out, highlight or draw a box round. Every match a range hands out is a slice with something in it.

The rule holds down every road: the one-shot functions, the prepared text, the prepared pattern and the ranges all answer the same.

## The edges

An empty pattern is found where it is looked for, and nowhere past the end of the text — `find_fold(s, string(), from)` is `{from, 0}` while `from <= s.size()` and nothing after that, which is what `std::string::find` and `searcher::find` answer. The bound is the text's own size, not the size of the folded copy of it. A range over an empty pattern is empty: a range of every position is not what anyone asking this question wants, and it would not end.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

int main() {
    string text = "Ala ma kota, a kot ma kota";
    txt::searcher kota(string("kota"));
    std::cout << kota.find(text) << ", " << kota.find(text, 8) << ", " << kota.count(text) << " razy\n";

    auto found = txt::find_fold(string("Die straße"), string("STRASSE"));
    std::cout << "STRASSE w 'Die straße': " << found->pos << ", bajtów " << found->size << '\n';

    string composed = "rue café";            // e with an acute, one code point
    string typed = "cafe\u0301";            // and two
    std::cout << "znalezione na bajcie " << txt::find_normalized(composed, typed)->pos
              << ", a jako bajty: " << (composed.find(typed) == npos ? "nie ma" : "jest") << '\n';

    // every occurrence, the text folded a single time for all of them
    string line = "Kot, KOT, kot i Kotek";
    for (auto it = txt::fold_matches(line, string("kot")); auto m : it) {
        std::cout << '[' << m << ']';
    }
    std::cout << '\n';

    // a ß is found whole, and not by half of what it folds to
    std::cout << txt::find_fold(string("straße"), string("ss"))->pos << ' '
              << (txt::find_fold(string("aßb"), string("s")) ? "znalezione" : "nic") << '\n';
    return 0;
}
```

The output:

```
7, 22, 2 razy
STRASSE w 'Die straße': 4, bajtów 7
znalezione na bajcie 4, a jako bajty: nie ma
[Kot][KOT][kot][Kot]
4 nic
```

## What it is held to

The exact search is checked against `std::string_view::find` over **4000 random texts and patterns** over a small alphabet, where matches and near misses are frequent, each from two starting positions. The prepared forms are checked against the one-shot ones over **2000 random texts** built of Polish, German and Greek pieces, at three starting positions each, and the ranges against the count the prepared text gives. The rest is checked against what the three are defined to do, with the letters that make them interesting: `ß` against `SS`, a Greek sigma in both its shapes, `"café"` written either way, a Korean syllable against its jamo, and the marks of one letter in either order.

Two edges have tests of their own, because both were faults. A refused match must not end the search — after turning one down the scan goes on from the next position, and there is a case for a `ß` standing in front of a real `s`, for two in a row, and for a refusal in the middle of a run that must not stop the count. And the canonical ordering moves marks, and a search from a byte offset inside a sequence whose marks it put in order is a case of its own, as is a text that begins with marks out of order and every byte of a text with marks out of order in four places, each answered as a walk from the front answers it.
