# txt::normalize

```cpp
#include "sgcl/txt/normalize.h"
```

The same text can be written in more than one way. `"é"` is one code point or two, `"각"` is one or three, and `"Å"` is three different code points that a reader would not tell apart. Normalization ([UAX #15](https://www.unicode.org/reports/tr15/)) puts a text into one of four forms, so that two texts a reader calls the same compare the same — which is what a search, a key of a map and a file name need.

## The four forms

```cpp
struct nfc_t;  struct nfd_t;  struct nfkc_t;  struct nfkd_t;
inline constexpr nfc_t nfc;  inline constexpr nfd_t nfd;
inline constexpr nfkc_t nfkc;  inline constexpr nfkd_t nfkd;
```

`nfd` and `nfkd` are **decomposed**: every character taken apart into its base and its marks, the marks in the order the standard fixes. `nfc` and `nfkc` are **composed**: decomposed first and then put back together, which is the form the web, most protocols and most file systems want.

The `k` forms decompose by **compatibility** as well: `"ﬁ"` becomes `"fi"`, `"①"` becomes `"1"`, `"Ａ"` becomes `"A"`, `"½"` becomes `"1⁄2"`. That keeps the meaning and loses the appearance, and it cannot be undone — use them to compare and to index, not to store.

The form is a **tag, not an enum**. `normalize(s, nfc)` reads at the call site as the enum would, the walk has no runtime branch, and a program that never asks for a `k` form does not carry the compatibility table, which is more than half the data of this header.

## The names

```cpp
template<class Form> string normalize(const string& text, Form);       // the text in the form
template<class Form> bool is_normalized(const string& text, Form);     // whether it is in it already

bool equal_normalized(const string& a, const string& b);               // the same text, however written
int compare_normalized(const string& a, const string& b);              // an order blind to the writing
size_t hash_normalized(const string& text);                            // a hash equal_normalized agrees with

uint8_t combining_class(char32_t c) noexcept;    // 0 for a starter, and the order of the marks
char32_t compose(char32_t a, char32_t b) noexcept;   // what they compose to, or 0
string decompose(char32_t c);                    // one code point taken apart, canonically
```

`combining_class` and `compose` are objects of the shape the rest of the module uses: they take a `char32_t`, refuse everything else, and are passable where a predicate is asked for.

## What it costs

A text that is already in the form comes back as **the same object**. The strings of the library are immutable and shared by copying, so the common case — text that arrives normalized, which on the web is nearly all of it — allocates nothing and copies nothing. Whether it is in the form is answered by the quick check properties of the UCD, which settle most texts without looking at a single decomposition; where they say "maybe", the text is normalized and compared.

```cpp
string s = "Ala ma kota";
normalize(s, nfc).data() == s.data();      // true: the same object
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// Two names that look alike and are not the same bytes, and a map that
// does not care which way they were typed
int main() {
    string typed = "Zaz\u0307o\u0301\u0142c\u0301 ge\u0328s\u0301la\u0328 jaz\u0301n\u0301";   // every letter that has a mark, written with it
    string pasted = "Zażółć gęślą jaźń";                                                        // the precomposed form
    std::cout << typed.size() << " vs " << pasted.size() << " bytes, "
              << typed.rune_count() << " vs " << pasted.rune_count() << " code points, "
              << txt::grapheme_count(typed) << " vs " << txt::grapheme_count(pasted) << " characters\n";
    std::cout << "equal as bytes: " << (typed == pasted)
              << ", as text: " << txt::equal_normalized(typed, pasted) << '\n';

    map<size_t, string> names;
    names[txt::hash_normalized(typed)] = pasted;
    std::cout << "found: " << names[txt::hash_normalized(pasted)] << '\n';

    // The compatibility forms fold what only looks different
    for (auto s : {string("ﬁ"), string("①"), string("Ａ"), string("½")}) {
        std::cout << s << " -> " << txt::normalize(s, txt::nfkc) << "   ";
    }
    std::cout << '\n';

    // A Korean syllable is one code point or three
    string syllable = "각";
    std::cout << syllable << ": " << syllable.rune_count() << " code point, "
              << txt::normalize(syllable, txt::nfd).rune_count() << " decomposed, "
              << "back: " << (txt::normalize(txt::normalize(syllable, txt::nfd), txt::nfc) == syllable) << '\n';
    return 0;
}
```

The output:

```
34 vs 26 bytes, 25 vs 17 code points, 17 vs 17 characters
equal as bytes: 0, as text: 1
found: Zażółć gęślą jaźń
ﬁ -> fi   ① -> 1   Ａ -> A   ½ -> 1⁄2   
각: 1 code point, 3 decomposed, back: 1
```

## What it is held to

`NormalizationTest.txt`, the UCD's conformance file: **19 965 lines**, each of them five ways of writing one text, with every invariant the standard states beside them — and the other half of the test, that every one of the **1 112 064 code points** part one does not name is its own normalization in all four forms. Both pass whole.

The tables are 68 KB: the compatibility decompositions 36.2, the canonical ones 19.8, the primary composites 5.8, the quick check properties 3.7 and the canonical combining class 2.9. The Hangul syllables are in none of them — eleven thousand of them decompose and compose by arithmetic, and the composition exclusions are not a list either: a canonical pair is a primary composite when NFC puts it back together, which the generator asks rather than reads.
