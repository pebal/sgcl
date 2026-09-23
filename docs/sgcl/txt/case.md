# txt::case

```cpp
#include "sgcl/txt/case.h"
```

The case of a **text**, where [core](../core/utf8.md) has the case of a code point. Three things happen here that cannot happen one code point at a time:

- a letter may become two — `"straße"` in upper case is `"STRASSE"`, `"ﬁ"` is `"FI"`, and `"ΐ"` is three code points;
- a letter may depend on what stands around it — a Greek sigma at the end of a word is `"ς"` and anywhere else `"σ"`;
- a letter may depend on the language — in Turkish an `i` keeps its dot when it grows, and an `I` loses one it never had.

## The names

```cpp
class locale;                                                  // the language a mapping may depend on

string to_lower_full(const string& text, locale = {});
string to_upper_full(const string& text, locale = {});
string to_title(const string& text, locale = {});              // the first letter of every word
string fold_case(const string& text);                          // for comparing, not for showing
bool equal_fold_full(const string& a, const string& b);
```

`string::to_lower()` and `string::to_upper()` of core stay where they are: they map one code point to one, which is right for most text and wrong for `ß`. The names here say `_full` because they do what the standard calls the full mapping.

## locale

```cpp
constexpr locale();                          // the root locale
explicit locale(const string& tag);          // a BCP-47 tag: "tr", "tr-TR", "az-Latn-AZ"
static constexpr locale root(), turkish(), azerbaijani(), lithuanian();
constexpr bool operator==(const locale&) const;
```

Four bytes, trivially copyable, `constexpr`. Three languages change the case of a letter and the tables of Unicode name no others, but the type takes a tag all the same, so that a language out of an HTTP header or out of the system needs no table of its own at the caller — and so that a collator can take the same type when it comes. An unknown tag is the root locale and nothing throws; only the language subtag is read, so `"tr-TR"` and `"TR"` are Turkish.

## What the conditions are

The mappings that depend on their surroundings are in the code rather than in a table, because a table cannot hold a condition:

| condition | what it does |
|---|---|
| Final_Sigma | `Σ` lowercases to `ς` when a cased letter stands before it and none after, marks and apostrophes aside |
| After_I | in Turkish, a combining dot above that follows an `I` is the letter's own and disappears |
| After_Soft_Dotted | a dot above that follows an `i` or a `j` |
| More_Above | in Lithuanian, an `i` under an accent keeps its dot: `I` + grave is `i` + dot + grave |
| Before_Dot | in Turkish, an `I` before a dot above is a dotted `i` |

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

int main() {
    string german = "die straße";
    std::cout << txt::to_upper_full(german) << "   (core, one to one: " << german.to_upper() << ")\n";

    string greek = "ΟΔΟΣ ΚΑΙ ΣΠΙΤΙ";
    std::cout << greek << " -> " << txt::to_lower_full(greek) << '\n';   // the sigma that ends a word

    auto tr = txt::locale(string("tr-TR"));
    std::cout << "Istanbul -> " << txt::to_lower_full(string("ISTANBUL"), tr)
              << " (tr), " << txt::to_lower_full(string("ISTANBUL")) << " (root)\n";

    std::cout << txt::to_title(string("don't stop me now")) << '\n';
    std::cout << "fold: " << txt::fold_case(string("STRASSE")) << " == " << txt::fold_case(german)
              << " -> " << txt::equal_fold_full(german, string("DIE STRASSE")) << '\n';
    return 0;
}
```

The output:

```
DIE STRASSE   (core, one to one: DIE STRAßE)
ΟΔΟΣ ΚΑΙ ΣΠΙΤΙ -> οδος και σπιτι
Istanbul -> ıstanbul (tr), istanbul (root)
Don't Stop Me Now
fold: strasse == die strasse -> 1
```

## What it is held to

What Python makes of every one of the **2981 code points** that have a case at all, and of 212 strings built around the conditions of `SpecialCasing.txt` — Python implements the rule of the final sigma, so it answers for that one too. The mappings that depend on a language Python does not do, and those cases are written out in the test from the file.

The tables are 8.2 KB and hold only the differences from the simple mapping core already has: one code point for the lower case, 102 for the upper, 135 for the title case, 298 for the folding, and the three properties the conditions ask about (Cased, Case_Ignorable, Soft_Dotted). Everything else falls through to [`unicode::to_lower`](../core/utf8.md).
