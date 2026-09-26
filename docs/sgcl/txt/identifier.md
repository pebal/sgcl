# txt::identifier

```cpp
#include "sgcl/txt/identifier.h"
```

What may be a name, and when two names that are not the same look the same. Three specifications meet here, and they answer three different questions: [UAX #31](https://www.unicode.org/reports/tr31/) says which code points may spell an identifier, NFKC_Casefold ([UAX #15](https://www.unicode.org/reports/tr15/)) is the form two names are compared in, and [UTS #39](https://www.unicode.org/reports/tr39/) is the security half — whether a code point belongs in a name at all, which names look alike, and how many scripts a name is written in.

Nothing here decides anything. It reports; whoever registers the name decides. **The limits of every report are written down**, below and in the header, because a security check whose limits are not stated is worse than none: it is trusted further than it reaches.

## Identifiers (UAX #31)

```cpp
bool is_identifier_start(char32_t c) noexcept;                     // XID_Start
bool is_identifier_continue(char32_t c) noexcept;                  // XID_Continue
bool is_identifier_start(char32_t c, program_syntax_t) noexcept;   // and '_' and '$'
bool is_identifier_continue(char32_t c, program_syntax_t) noexcept;

bool is_identifier(const string& text);                            // rule R1, with R1a
bool is_identifier(const string& text, program_syntax_t);
```

`XID_Start` and `XID_Continue`, not `ID_Start` and `ID_Continue`. **The X is the whole point**: the plain pair is not closed under normalization, so a text that is an identifier can stop being one when it is put into NFKC. `U+037A GREEK YPOGEGRAMMENI` is `ID_Start` and normalizes to a space and an iota; `U+309B KATAKANA-HIRAGANA VOICED SOUND MARK` does the same; `U+0E33 THAI CHARACTER SARA AM` normalizes to a mark and a vowel, and a mark may not begin a name. Twenty-three code points are `ID_Start` and not `XID_Start`, nineteen are `ID_Continue` and not `XID_Continue`, and they are exactly the ones that would break that way. Taking them out is what lets a compiler compare two names in a normal form and a linker carry one.

`program_syntax` is the profile of §2.3 that every parser of a programming language wants: the underscore begins a name and the dollar sign stands anywhere in one, which is what C, C++, Java, JavaScript and the shells all do. It is a **tag, not a flag** — the walk has no runtime branch, and the call reads as the enum would.

**Rule R1a, the two joiners.** `U+200D ZERO WIDTH JOINER` and `U+200C ZERO WIDTH NON-JOINER` are formatting code points and are in neither `XID` set, and the Indic languages cannot be written without them: `क्ष` is a conjunct and `क्‍ष` with a joiner in it is not, and the two are different words. A joiner is allowed after a virama — a mark of combining class 9 — and a non-joiner is allowed there too and also where it breaks a cursive join that would otherwise happen, between a letter that joins to the left and one that joins to the right. That is the same line [RFC 5892](https://www.rfc-editor.org/rfc/rfc5892) draws for domain names. Anywhere else they are refused: `"a" ZWJ "b"` is not an identifier.

## NFKC_Casefold

```cpp
string nfkc_casefold(const string& text);
bool is_nfkc_casefolded(const string& text);
```

The form a name is compared in when the comparison must not care how the name was written, what case it was written in, or whether a compatibility form was used: `"ＦＵＬＬ"` and `"full"` fold to one text, `"ﬁle"` and `"file"` fold to one, and so do `"é"` written as one code point and as two. It is what [UTS #46](https://www.unicode.org/reports/tr46/) folds a domain label to and what UAX #31 recommends a compiler compare identifiers by. It also **drops the default ignorable code points**, which is the half that is not a folding: a soft hyphen hidden inside `"pay­pal"` is gone.

It is not for showing to anybody. The mapping cannot be undone, it loses the case, and it loses the difference between a ligature and the letters in it. Fold to compare, keep the original to print.

**The mapping is not a table here.** `DerivedNormalizationProps.txt` would be 91.4 KB of prototypes, and every one of them is `NFC(fold(NFKD(c)))` with the default ignorable code points dropped — for all 1 114 112 code points, which the generator asserts before it writes anything. So the module works it out from the compatibility decompositions and the full case folding it carries anyway, and keeps 2.9 KB of properties instead of 91.4 KB of mapping.

The order matters and is not the order one would guess. The text is **not** decomposed as a whole first: `U+0345 COMBINING GREEK YPOGEGRAMMENI` has combining class 240 and folds to `U+03B9 GREEK SMALL LETTER IOTA`, which has none, so decomposing the whole text would sort it past the marks that follow it and the fold would then leave it there. Each code point is taken apart on its own, folded, and only then is the whole put in canonical order and composed — which is where the standard draws the same line, its mapping being per code point with one NFC at the end.

A text already in the form comes back as **the same object**, as with `normalize`: the two properties that answer it cost 14.4 ns over a name where the fold costs 100.

## Security (UTS #39)

```cpp
enum class identifier_status : uint8_t { restricted, allowed };
enum class identifier_type : uint16_t { not_character, deprecated, default_ignorable, not_nfkc,
                                        not_xid, exclusion, obsolete, technical, uncommon_use,
                                        limited_use, inclusion, recommended };

identifier_status identifier_status_of(char32_t c) noexcept;
identifier_type identifier_type_of(char32_t c) noexcept;      // a set of the above, read with &
bool is_allowed_identifier(const string& text);

string skeleton(const string& text);                          // §4
bool is_confusable(const string& a, const string& b);

bool is_single_script(const string& text);                    // §5.1
restriction_level restriction_level_of(const string& text);   // §5.2
bool is_highly_restrictive(const string& text);
bool is_moderately_restrictive(const string& text);
```

`identifier_type_of` answers a **set** and not one value, so it is read with `&`: `(identifier_type_of(c) & identifier_type::technical) != identifier_type::not_character`. It is what says *why* a code point is not allowed in a name — deprecated, technical, obsolete, an exclusion, a compatibility form.

`skeleton` is the mapping of §4: the text decomposed, every code point replaced by the one it is confusable with, decomposed again. Two texts a reader could mistake for one another have the same skeleton. **A skeleton is not text**: it is a key to compare by and to look up in a table of the names already taken, and no reader should ever see one.

`restriction_level_of` is the ladder of §5.2, from `ascii_only` through `single_script`, `highly_restrictive` (one script, or one of the three writing systems that are more than one script — Japanese, Chinese, Korean — with Latin beside them), `moderately_restrictive` (Latin and one other recommended script, and not Cyrillic, Greek or Cherokee, whose letters are the ones Latin is mistaken for), `minimally_restrictive` (every code point allowed, the scripts mixed freely) to `unrestricted`. The specification suggests `moderately_restrictive` for a registry open to the world and `highly_restrictive` where a mistaken name costs something.

**An empty text is `unrestricted`**, which is the least safe rung, and that is a decision rather than an accident: an empty name is not a name, and a function whose answer is acted on by refusing everything above a rung has to fail towards refusing. `is_allowed_identifier("")` is `false` and `is_identifier("")` is `false` for the same reason. The rule to read the whole header by is that where an answer is not clear-cut it comes out on the cautious side — which is worth stating precisely because a caller who assumed the opposite would be let through.

## What this does not catch

A list, because the absence of one is what makes a security function dangerous.

- **Script_Extensions is not used.** `is_single_script` and the levels ask the `Script` property. A code point that is `Common` although only two scripts use it — the Japanese prolonged sound mark `ー` is the one everybody meets — counts here as belonging to all of them. That makes the answer **more generous** than the specification's and never less, so a text this calls single script may be two by `Script_Extensions`.
- **The confusables table is one judgement, in one font.** `"rn"` for `"m"` is in it, `"1"` for `"l"` is, `"paypa1"` is caught. Whether two glyphs look alike on the reader's screen, in the reader's font, at the reader's size is not a question any table answers.
- **A similar name is not a confusable one.** `"paypal-inc"` against `"paypal"` is a different name, not the same one written differently, and nothing here will say a word about it. Whole-name similarity, edit distance and the domains people typo are a different problem.
- **A mixed-script name is not necessarily an attack and a single-script one is not necessarily safe.** A wholly Cyrillic `"расчёт"` is single script and perfectly honest; a wholly Cyrillic `"расс"` written to be read as Latin `"pacc"` is single script too. The level is evidence, not a verdict.
- **Nothing here looks at the bidirectional algorithm.** A name with a right-to-left override in it can be drawn in an order its bytes do not have; [`bidi`](bidi.md) is where that is asked about, and `is_allowed_identifier` refuses the overrides because UTS #39 does, not because this header reasons about them.
- **This is not IDNA.** A domain label has rules of its own — the length, the hyphens in the third and fourth places, the Punycode — and they are not here.
- **The tables are Unicode 16.0.0.** A code point assigned tomorrow is `not_character` and `Restricted` today, which is the safe way round and still a difference.

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/txt/identifier.h"
#include <iostream>

using namespace sgcl;

// A registry that will not let two names be told apart by nobody
int main() {
    for (auto s : {string("wartość"), string("_name"), string("2name"), string("na me")}) {
        std::cout << s << ": identifier " << txt::is_identifier(s)
                  << ", with the profile " << txt::is_identifier(s, txt::program_syntax) << '\n';
    }

    for (auto s : {string("ＦＵＬＬ"), string("ﬁle"), string("Straße"), string("①②③")}) {
        std::cout << s << " -> " << txt::nfkc_casefold(s) << "   ";
    }
    std::cout << '\n';

    string wanted = "раypal";          // the а and the р are Cyrillic
    string taken = "paypal";
    std::cout << wanted << " vs " << taken
              << ": same bytes " << (wanted == taken)
              << ", same folded " << (txt::nfkc_casefold(wanted) == txt::nfkc_casefold(taken))
              << ", confusable " << txt::is_confusable(wanted, taken)
              << ", single script " << txt::is_single_script(wanted) << '\n';

    for (auto s : {string("paypal"), string("wartość"), string("変数"),
                   string("раypal"), string("na me")}) {
        const char* names[] = {"ascii_only", "single_script", "highly_restrictive",
                               "moderately_restrictive", "minimally_restrictive", "unrestricted"};
        std::cout << s << ": " << names[size_t(txt::restriction_level_of(s))] << '\n';
    }
    return 0;
}
```

The output:

```
wartość: identifier 1, with the profile 1
_name: identifier 0, with the profile 1
2name: identifier 0, with the profile 0
na me: identifier 0, with the profile 0
ＦＵＬＬ -> full   ﬁle -> file   Straße -> strasse   ①②③ -> 123   
раypal vs paypal: same bytes 0, same folded 0, confusable 1, single script 0
paypal: ascii_only
wartość: single_script
変数: single_script
раypal: minimally_restrictive
na me: unrestricted
```

Folding does **not** catch the Cyrillic name: `nfkc_casefold` is about how a name was written, and the Cyrillic а is a different letter and not a different writing of the same one. That is what `is_confusable` and the restriction level are for, and it is why a registry needs all three.

## What it is held to

Four oracles, three of them whole files.

`XID_Start` and `XID_Continue` over **every one of the 1 114 112 code points**, against Python's own tables rather than the UCD's — `str.isidentifier()` is the first with the underscore added and `"a" + c` is the second — with the generator asserting that the two sources agree before it writes either. The NFKC_CF mapping of `DerivedNormalizationProps.txt` **entry by entry**, all 10 554 of them, and the identity of the other 1 103 558. The confusables, `IdentifierStatus.txt` and `IdentifierType.txt` of UTS #39 **whole**: 6 355 prototypes, 391 ranges of `Allowed` over 112 778 code points, 1 682 ranges of `Identifier_Type`, with Table 1 of the specification checked as a property — `Allowed` is exactly `Recommended` or `Inclusion` — over the whole space. And the attacks people actually write, by hand.

The fourth is a fuzzer: 20 000 rounds of arbitrary bytes and 20 000 of arbitrary code points through `skeleton`, `nfkc_casefold`, `is_identifier` and `restriction_level_of`, checking that the two forms meant to be fixed points are ones, under the address sanitizer.

## What it costs

The benchmark is in the tree, so the next change has something to measure against rather than starting over:

```sh
cmake --build <build> --target bench_identifier
<build>/benchmarks/bench_identifier casefold latin     # and: upper, compat, folded
<build>/benchmarks/bench_identifier skeleton ascii
```

Over a **stream of a thousand different names** and never the same one twice, because a repeated name hides the searching. `latin` is Polish, French, German and Czech, two of the six with a capital in them; `upper` is the same letters all in capitals, so that every name changes and no quick check saves it; `compat` is fullwidth letters, a ligature, circled digits, Greek, Cyrillic and Japanese; `folded` is `latin` already put through `nfkc_casefold`.

| ns per call | ascii | latin | upper | compat | folded |
|---|---|---|---|---|---|
| `is_identifier` | 13.4 | 17.3 | | 31.1 | 17.2 |
| `is_identifier`, the profile | 14.4 | 18.1 | | 31.2 | 17.9 |
| `nfkc_casefold` | 14.7 | 101 | 296 | 196 | 19.2 |
| `fold_case`, for scale | 15.3 | 105 | 104 | 166 | 105 |
| `is_nfkc_casefolded` | 11.2 | 11.1 | | 14.8 | 14.6 |
| `skeleton` | 212 | 220 | 219 | 242 | 231 |
| `is_confusable` | 389 | 458 | 452 | 492 | 460 |
| `is_allowed_identifier` | 8.2 | 10.3 | | 14.3 | 10.2 |
| `restriction_level_of` | 9.8 | 29.6 | | 35.7 | 29.8 |
| `is_single_script` | 19.8 | 19.3 | | 30.1 | 19.4 |
| `without_marks` | 6.1 | 197 | 198 | 229 | 200 |

The scripts of a text are counted into **eight words of stack** rather than into a container. A name is written in one script or two, and the widest set UTS #39 names is four — Latin, Han, Hiragana and Katakana, which is Japanese — so eight is twice what anything a caller would call a name reaches. The container it replaced was the whole cost of the question: `is_single_script` used to take longer over an ASCII name (27.0 ns) than over a Polish one (26.0), which is not the shape of a walk that decodes a code point and reads a table. It is 19.8 and 19.3 now, and `restriction_level_of` over a Polish name fell from 38.0 to 29.6. A ninth distinct script is not dropped: it stops the walk and is reported as what it is, and every rung is already settled by the fact of it — nine scripts is not one script, is not any of the three sets of four, and is not Latin with one other.

The `fold_case` row is the one to read against. Over `latin` the two are level — 101 against 105 — because the quick check settles two names in three before any folding happens, and over `folded`, where `fold_case` still pays its full 105, this costs 19. Over `upper`, where every name changes and the quick check saves nothing, the compatibility decomposition and the composition cost about 190 ns on top of the plain fold. That is the shape of the trade: the form is dearer than a folding only when it has work to do, and the names a registry sees mostly do not.

The tables are **90.0 KB**: the confusable prototypes 59.4, `Identifier_Type` 13.0, `XID_Continue` 4.7, the joining types 4.2, `XID_Start` 4.1, the code points NFKC_Casefold changes 2.3, `Identifier_Status` 1.6 and the default ignorable ones 0.6. The NFKC_CF mapping would have been 91.4 KB more and is not here. A program that only asks `is_identifier` links the two `XID` sets, 8.8 KB, and the joining types rule R1a needs, 4.2 more; the confusables come in only with `skeleton`, and the linker drops what nothing asks about without a flag.
