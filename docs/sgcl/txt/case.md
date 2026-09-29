# sgcl::txt::case

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt/txt.h"

class locale;                                                  // the language a mapping may depend on

string to_lower_full(const string& text, locale = {});
string to_upper_full(const string& text, locale = {});
string to_title(const string& text, locale = {});              // the first letter of every word
string fold_case(const string& text);                          // for comparing, not for showing
bool equal_fold_full(const string& a, const string& b);
```

The case of a **text**, where [core](../core/utf8.md) has the case of a code point. Three things happen here that cannot happen one code point at a time:

- a letter may become two — `"straße"` in upper case is `"STRASSE"`, `"ﬁ"` is `"FI"`, and `"ΐ"` is three code points;
- a letter may depend on what stands around it — a Greek sigma at the end of a word is `"ς"` and anywhere else `"σ"`;
- a letter may depend on the language — in Turkish an `i` keeps its dot when it grows, and an `I` loses one it never had.

`string::to_lower()` and `string::to_upper()` of core stay where they are: they map one code point to one, which is right for most text and wrong for `ß`. The names here say `_full` because they do what the standard calls the full mapping.

## What the conditions are

The mappings that depend on their surroundings are in the code rather than in a table, because a table cannot hold a condition:

| condition | what it does |
|---|---|
| Final_Sigma | `Σ` lowercases to `ς` when a cased letter stands before it and none after, marks and apostrophes aside |
| After_I | in Turkish, a combining dot above that follows an `I` is the letter's own and disappears |
| After_Soft_Dotted | a dot above that follows an `i` or a `j` |
| More_Above | in Lithuanian, an `i` under an accent keeps its dot: `I` + grave is `i` + dot + grave |
| Before_Dot | in Turkish, an `I` before a dot above is a dotted `i` |

## What it is held to

What Python makes of every one of the **2981 code points** that have a case at all, and of 212 strings built around the conditions of `SpecialCasing.txt` — Python implements the rule of the final sigma, so it answers for that one too. The mappings that depend on a language Python does not do, and those cases are written out in the test from the file.

The tables are 8.2 KB and hold only the differences from the simple mapping core already has: one code point for the lower case, 102 for the upper, 135 for the title case, 298 for the folding, and the three properties the conditions ask about (Cased, Case_Ignorable, Soft_Dotted). Everything else falls through to [`unicode::to_lower`](../core/utf8.md).

## Members

### locale

```cpp
constexpr locale();                          // the root locale
explicit locale(const string& tag);          // a BCP-47 tag: "tr", "tr-TR", "az-Latn-AZ"
static constexpr locale root(), turkish(), azerbaijani(), lithuanian();
constexpr bool operator==(const locale&) const;
constexpr bool dotted_i() const;             // Turkish or Azerbaijani: an i written the Turkish way
constexpr bool keeps_dot() const;            // Lithuanian: the dot above kept over i and j
constexpr uint32_t subtag() const;           // the language subtag in four bytes: what a table keyed by language is looked up with
```

Four bytes, trivially copyable, `constexpr`. Three languages change the case of a letter and the tables of Unicode name no others, but the type takes a tag all the same, so that a language out of an HTTP header or out of the system needs no table of its own at the caller — and so that a collator can take the same type when it comes. An unknown tag is the root locale and nothing throws; only the language subtag is read, so `"tr-TR"` and `"TR"` are Turkish.

## Examples

A letter that becomes two, and one that depends on what stands around it:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string german = "die straße";
    println("{}   (core, one to one: {})", txt::to_upper_full(german), german.to_upper());

    string greek = "ΟΔΟΣ ΚΑΙ ΣΠΙΤΙ";
    println("{} -> {}", greek, txt::to_lower_full(greek));  // the sigma that ends a word
    return 0;
}
```

Output:

```text
DIE STRASSE   (core, one to one: DIE STRAßE)
ΟΔΟΣ ΚΑΙ ΣΠΙΤΙ -> οδος και σπιτι
```

A letter that depends on the language:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    auto tr = txt::locale("tr-TR");
    println("Istanbul -> {} (tr), {} (root)", txt::to_lower_full("ISTANBUL", tr),
            txt::to_lower_full("ISTANBUL"));
    return 0;
}
```

Output:

```text
Istanbul -> ıstanbul (tr), istanbul (root)
```

The title case, and the folding a comparison uses:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string german = "die straße";
    println("{}", txt::to_title("don't stop me now"));
    println("fold: {} == {} -> {}", txt::fold_case("STRASSE"), txt::fold_case(german),
            txt::equal_fold_full(german, "DIE STRASSE"));
    return 0;
}
```

Output:

```text
Don't Stop Me Now
fold: strasse == die strasse -> true
```

## See also

[The module](README.md); [`utf8`](../core/utf8.md), the case of a code point in core; [`collate`](collate.md), which takes the same `locale`; [`search`](search.md), `find_fold`; [`identifier`](identifier.md), `nfkc_casefold`.
