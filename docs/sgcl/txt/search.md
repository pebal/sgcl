# txt::search

```cpp
#include "sgcl/txt/search.h"
```

Finding a text inside a text, three ways for three questions.

```cpp
class searcher {
    explicit searcher(const string& pattern);
    size_t find(const string& text, size_t from = 0) const noexcept;   // npos when not there
    bool contains(const string& text) const noexcept;
    size_t count(const string& text) const noexcept;                   // occurrences that do not overlap
    const string& pattern() const noexcept;
};

size_t find_fold(const string& text, const string& pattern, size_t from = 0);
bool contains_fold(const string& text, const string& pattern);

size_t find_normalized(const string& text, const string& pattern, size_t from = 0);
bool contains_normalized(const string& text, const string& pattern);
```

## The prepared pattern

`searcher` is Boyer–Moore–Horspool over the bytes: the pattern is looked at once, a table of skips is built from it, and after that a pattern of *m* bytes is found in *n* bytes in about *n/m* steps on ordinary text. It is for the loop that looks for the same thing in many texts — a single search is what [`string::find`](../core/string.md) is for.

The bytes are safe to search. UTF-8 synchronises itself: the first byte of a character cannot appear inside another one, so a match of valid UTF-8 inside valid UTF-8 always begins on a character, and no test for that is needed.

A `searcher` holds a [`string`](../core/string.md), so it lives on the stack or inside a managed object, like any other value of the library that holds one.

## Blind to case

`find_fold` folds both sides ([`fold_case`](case.md)) and searches the folded text, and reports the position **in the text as it was given** — not in any folded copy of it. So `"STRASSE"` finds `"straße"` at the byte where the German word begins, although folding made them different lengths.

## Blind to how it was written

`find_normalized` decomposes both sides and puts the marks in canonical order ([`normalize`](normalize.md)) before searching, so an `"é"` of one code point finds an `"é"` of two and a Korean syllable finds its jamo. The position is in the original text again.

Both of these build a mapped copy of the text on every call — the position in the original is kept beside every code point — so a loop that searches the same text many times should fold or normalize it once itself and use a `searcher` on the result.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

int main() {
    string text = "Ala ma kota, a kot ma kota";
    txt::searcher kota(string("kota"));
    std::cout << kota.find(text) << ", " << kota.find(text, 8) << ", " << kota.count(text) << " razy\n";

    std::cout << "STRASSE w 'Die straße': " << txt::find_fold(string("Die straße"), string("STRASSE")) << '\n';

    string composed = "rue café";            // e with an acute, one code point
    string typed = "café";                  // and two
    std::cout << "znalezione na bajcie " << txt::find_normalized(composed, typed)
              << ", a jako bajty: " << (composed.find(typed) == npos ? "nie ma" : "jest") << '\n';
    return 0;
}
```

The output:

```
7, 22, 2 razy
STRASSE w 'Die straße': 4
znalezione na bajcie 4, a jako bajty: nie ma
```

## What it is held to

The exact search is checked against `std::string_view::find` over **4000 random texts and patterns** over a small alphabet, where matches and near misses are frequent, each from two starting positions. The other two are checked against what they are defined to do, with the letters that make them interesting: `ß` against `SS`, a Greek sigma in both its shapes, `"café"` written either way, a Korean syllable against its jamo, and the marks of one letter in either order.
