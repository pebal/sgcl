[sgcl](../README.md) › txt

# sgcl::txt

```cpp
#include "sgcl/txt.h"   // namespace sgcl::txt
```

What a human expects of text and a byte does not give: the properties of a code point, the boundaries between
graphemes, words, sentences and lines, normalization, the full case mappings, the order of a language, text that runs
both ways, names of hosts and identifiers, the encodings, escaping, patterns, the writing of values into text with
[format](format.md) and of a shape the program did not write with [stencil](stencil/README.md) (and of HTML, each
value escaped for its place, with [html_stencil](html_stencil/README.md)), HTML read as a browser reads it
([html_document](html_document/README.md)) and made safe ([sanitize_html](sanitize_html.md)), and what a locale
writes by CLDR's data: numbers and amounts ([number_format](number_format/README.md)), plural forms
([plural_of](plural_of.md)), lists, relative times and the locale itself ([locale](locale/README.md)), and with the
optional headers of the [display names](names.md) the names of languages, regions, scripts, currencies and zones in
any language; and the translations of a program, GNU gettext's catalogs ([catalog](catalog/README.md)) and ICU's
messages ([message_format](message_format/README.md)); and the differences of texts — edit scripts by lines, words or
code points, the unified format, patches applied as `patch` applies them and three-way merges ([diff_lines](diff_lines.md),
[unified_diff](unified_diff.md), [apply_patch](apply_patch.md), [merge3](merge3.md)); and columns of text aligned by their tabs
([align_tabs](align_tabs.md)), and how far apart two texts are ([levenshtein](levenshtein.md),
[jaro_winkler](jaro_winkler.md)), and Markdown, CommonMark's and GitHub's, as HTML or a tree
([markdown_to_html](markdown_to_html.md), [markdown_document](markdown_document/README.md)). The module depends on [core](../core/README.md) (with its containers) and on nothing else — never on
[async](../async/README.md) or [io](../io/README.md) — and `net`, `time`, `slog` and the user interface are built on it; the index of the whole
interface is [the modules](../README.md).

Core has text at the level of bytes and code points: [utf8](../core/utf8/README.md) (decoding, encoding, validation),
[unicode](../core/unicode/README.md) (the simple case mappings, one code point to one, and the White_Space property),
[runes](../core/runes/README.md) (the code points of a text, decoded as they are walked) and the whole text interface of
[string](../core/string/README.md) and [slice](../core/slice/README.md). That does not move, and this module does not repeat it:
`txt` adds only what needs the larger tables and the algorithms of the annexes.

Every algorithm here is the library's own, written from the specification — UAX #9, #11, #14, #15, #29, #31, UTS #10,
#39, #46, RFC 3492 and 3986 — and held to the specification's own test files; other implementations are used in the
tests as an oracle and nowhere else. The data is Unicode 16.0.0, the same version as core's (`txt::version`), and the
tailorings of collation and the locale data are CLDR 46.

## The rules

1. **A code point and nothing else.** Every name that asks about a character takes a `char32_t` and refuses
   everything else: a `char` is a byte of UTF-8, not a character (`is_space(s[0])` on a no-break space asks about
   `0xC2`), and `'ż'` in a UTF-8 source is a multi-character literal of type `int`. The refusal is a deleted
   overload, and the names are objects rather than functions, the shape core's [unicode](../core/unicode/README.md) uses, so
   that each is still passable where a predicate or a projection is asked for: `s.runes().count_of(txt::is_alpha)`.
2. **The library's types in the interface.** Text comes in as a [string](../core/string/README.md), a
   [slice\<const char\>](../core/slice/README.md) or a C text and goes out as a string or a slice: both hold the object the
   characters live in, which a `std::string_view` does not. A range this module hands back — the graphemes of a text, its words, its matches — is a class
   constructed from the text, as [runes](../core/runes/README.md) is (`txt::graphemes(s)` looks like a call and is a
   construction), a range of the library ([mixin::enumerable](../core/mixin/enumerable/README.md)) whose elements are slices
   of the text, decoded as it is walked and allocating nothing per element: a loop over a temporary is safe.
3. **Nothing is refused.** An invalid byte is `U+FFFD`, as it is in core (`decode(b, e, txt::strict)` gives the
   error instead, for a program that must not change a text); a code point no encoding can carry is `'?'`. An
   `optional` or an `expected` appears only where an operation can really fail: a name of an encoding nobody knows, a
   pattern or a template read where the program runs, a malformed escape, a domain name that must not be looked up.
4. **Nothing throws over the data.** A function that answers about a text or walks it is `noexcept`, and a text from
   outside the program is answered with an `optional` or an `expected`, never an exception. What throws is a mistake
   of the program's own or a limit, each said on its page under `## Exceptions`: a literal pattern or template that
   does not parse (`bad_expected_access`), `{:c}` of a number no `char` holds (`out_of_range`), a text built by the
   function past the 4 GiB a [string](../core/string/README.md) holds (`length_error`), and what a function of the program
   throws when the library calls it (a formatter, a function of a template's pipeline).
5. **A pattern in the program is read by the compiler.** A literal pattern of [format](format.md) and of
   [regex](regex/README.md) and the literal source of a [stencil](stencil/README.md) are checked where the program is compiled; a
   pattern that arrives while the program runs goes through a form that reports why it is not one
   ([runtime](runtime.md), [regex::compile](regex/compile.md), [stencil::parse](stencil/parse.md)).
6. **Its own implementation, from the specification.** Every algorithm is written from its annex; other
   implementations (ICU, Python, Go, `std`) are used in the tests as an oracle and nowhere else. A boundary is written
   down rather than discovered: [stencil](stencil/README.md) has no escaping that knows where a value lands and says
   so, the page of HTML being [html_stencil](html_stencil/README.md)'s; a check of [identifiers](is_identifier.md)
   reports, and whoever registers the name decides, with the limits of every report written on its page.

### Searching

Three ways for three questions: the bytes as they stand, which is what a parser wants ([searcher](searcher/README.md),
[regex](regex/README.md)); blind to case, which is what a person searching wants ([find_fold](find_fold.md)); and blind to
the way the text was written, which is what a search over names and file paths wants, since `"é"` typed in two code
points must find `"é"` stored in one ([find_normalized](find_normalized.md)). A [collator](collator/README.md) adds a fourth:
whatever the collator counts as equal.

- **A match takes whole characters.** In every search blind to case, to spelling or by collation, a match may not cut
  in two what one character folds or decomposes to, nor begin or end inside a combining sequence: `"ss"` finds a
  `"ß"`, `"s"` does not find half of one, and `"cafe"` is not found in `"café"` however it is written. One function
  decides it for all three, so they cannot answer differently about one text, and every match is a piece of the text
  that can be cut out. The positions reported are bytes of the text as it was given, never of a folded or decomposed
  copy.
- **Ask once, or ask in a loop.** The one-shot searches ([find_fold](find_fold.md),
  [find_normalized](find_normalized.md), [collator::find](collator/find.md)) map or weigh both sides on every call:
  the right shape for one question, and quadratic in a loop over the occurrences. Each has prepared forms — a pattern
  prepared once ([fold_searcher](fold_searcher/README.md), [collated_searcher](collated_searcher/README.md)), a text prepared once
  ([folded_text](folded_text/README.md), [collated_text](collated_text/README.md)) and a range of every occurrence
  ([fold_matches](fold_matches/README.md), [collated_matches](collated_matches/README.md)) — and all of them answer the same.

### The tables

`tools/unicode_tables.py` generates them all, into `sgcl/core/detail/` for core and `sgcl/txt/detail/` for this
module. What Python's `unicodedata` knows (the general category, the case mappings, East_Asian_Width) is taken from
it, and every table it gives is checked against it code point by code point; what it does not know (the scripts, the
emoji properties, the break properties) is read from the UCD files, which the script downloads once into
`.ucd/<version>/` and asserts to declare the same Unicode version as the Python that runs it — otherwise "one source"
would drift apart the day the Python changes. The generated headers are committed; the data is not.

Below U+10000 a property is a two-stage table — an index of blocks and the blocks, each distinct block stored once:
two reads and no branch; above it, sorted ranges and a binary search, split at the end of the Basic Multilingual
Plane. ASCII is answered without a table, and a run of ASCII letters is walked in one step where the rules give it one
answer (GB3, WB5, LB28, the case of a Latin letter). The tables are 707 KB, which is what `tools/unicode_tables.py`
prints when it has written them all: 42.6 KB for the properties, 63.1 KB for the boundaries, 78.2 KB for the
normalization, 9.6 KB for the case mappings, 33.8 KB for the 27 single byte encodings, 17.5 KB for the domain names,
5.6 KB for the names a pattern may ask about, 85.7 KB for the identifiers and the confusables, 13.3 KB for the
bidirectional algorithm, and 357.5 KB for the collation — 255.5 for the root order and 101.3 for the 88 languages,
the one table bigger than the rest put together. A program carries only the tables it asks about: one that only
calls `is_alpha` links 16.7 KB, and the linker drops the rest without a flag. The times of the shapes are on
[benchmarks](benchmarks.md).

The locale data is generated by `tools/cldr_tables.py` from CLDR 46 (the same zip as the tailorings, which it never
downloads) for the 699 locales of `common/main` whose language CLDR rates modern, moderate or basic, with their
regional and script variants; a language outside them formats in root's data, as in ICU. Each group is a header of
its own, included only by what reads it — the locales and their likely subtags, the plural rules (compiled into C++
functions, one a rule set), numbers, currencies, lists, relative time, and the calendar in `time` — every locale's
data resolved there once by CLDR's inheritance and aliases, each distinct text and record once, the texts as UTF-8
in `u8` literals and the tables as base-64 digits: about 1 MB of source for all of it. The formatting is held
against ICU 78 over some fifteen thousand cases, and agrees but where ICU's newer data (CLDR 48) differs.

### Conformance

Every algorithm is held to the UCD's own test files, every case of each — 20103 break cases
(`GraphemeBreakTest.txt`, `WordBreakTest.txt`, `SentenceBreakTest.txt`, `LineBreakTest.txt`), 19965 lines of
`NormalizationTest.txt`, the 2981 code points that have a case and 212 strings built around the conditions of
`SpecialCasing.txt` (against Python), 91707 bidirectional cases and 206286 lines of the collation conformance file
— with no rule tailored and none skipped — and 6387 of the 6389 cases of `IdnaTestV2.txt` ([idna](idna/README.md) says
which two and why); the tailorings of collation, which have no conformance file, are held to ICU, and regex to
Python's `re`. Each page names its own oracle.

## Functions

| Function | Header | Description |
|---|---|---|
| [align_tabs](align_tabs.md) | `tab_writer.h` | cells ended by tabs aligned into columns, Go's `text/tabwriter` with a terminal's widths |
| [apply_patch](apply_patch.md) | `diff.h` | a unified patch applied to a text as `patch` applies it: hunks moved to where their lines stand, fuzz, reversal |
| [best_match](best_match.md) | `locale.h` | the supported locale closest to what a user wants, by CLDR's language matching, of locales or of an Accept-Language header |
| [bidi_class_of](bidi_class_of.md) | `bidi.h` | the bidirectional class of a code point, an object called as a function |
| [category_of](category_of.md) | `properties.h` | the general category of a code point, `category::unassigned` when it has none |
| [columns](columns.md) | `properties.h` | the cells a code point or a text takes on a terminal: 0, 1 or 2 a code point, by East_Asian_Width |
| [combining_class_of](combining_class_of.md) | `normalize.h` | the canonical combining class of a code point: 0 for a starter, the order of the marks |
| [compare_normalized](compare_normalized.md) | `normalize.h` | an order of two texts blind to the way they were written |
| [compose](compose.md) | `normalize.h` | what two code points compose to, or 0 |
| [contains_fold](contains_fold.md) | `search.h` | checks whether a pattern occurs in a text without regard to case |
| [contains_normalized](contains_normalized.md) | `search.h` | checks whether a pattern occurs in a text without regard to how either was written |
| [damerau_levenshtein](damerau_levenshtein.md) | `distance.h` | Damerau's edit distance: Levenshtein's edits and transpositions of neighbours, a metric |
| [decode](decode.md) | `encoding.h` | bytes in an encoding, given or by its name, as text, `U+FFFD` for a byte that means nothing; with `strict`, the first such byte as an error |
| [decompose](decompose.md) | `normalize.h` | one code point taken apart, canonically |
| [detect_bom](detect_bom.md) | `encoding.h` | what a byte order mark at the front of some bytes says, and its size |
| [diff_chars](diff_chars.md) | `diff.h` | the edit script between two texts by code points |
| [diff_lines](diff_lines.md) | `diff.h` | the edit script between two texts by lines, Myers' shortest or patience diff, as byte ranges of both |
| [diff_words](diff_words.md) | `diff.h` | the edit script between two texts by words |
| [encode](encode.md) | `encoding.h` | text as bytes in an encoding, given or by its name, `?` for what it cannot write |
| [encoding_from_name](encoding_from_name.md) | `encoding.h` | the encoding a name from a header stands for, in any case and under its aliases; `nullopt` for a name nobody knows |
| [equal_fold_full](equal_fold_full.md) | `case.h` | checks whether two texts are the same but for their case, by the full folding: `ß` equals `SS` |
| [equal_normalized](equal_normalized.md) | `normalize.h` | checks whether two texts are the same text, however written |
| [find_fold](find_fold.md) | `search.h` | the first occurrence without regard to case, as the bytes of the original text it covers |
| [find_normalized](find_normalized.md) | `search.h` | the first occurrence without regard to how either side was written (canonical decomposition) |
| [fits\<A...\>](fits.md) | `format.h` | whether a pattern read where the program runs fits values of the types named, with nothing written: a catalogue of translations checked where it is loaded |
| [fold_case](fold_case.md) | `case.h` | a text folded for comparison by the full case folding |
| [format](format.md) | `format.h` | text built from the pattern of `std::format` and values: the pattern read where the program is compiled, a field of text measured in columns, `{:?}`, lists, tables, pairs and `optional` in the shapes of C++23, enumerations as their number, patterns of time; a pattern read where the program runs answers an `optional` |
| [format_currency](format_currency.md) | `number.h` | an amount of a currency as a locale writes it: `12,50 zł`, `PLN 12.50` |
| [format_list](format_list.md) | `list_format.h` | items joined as a locale joins a list: `Ala, Ola i Ela`, `a, b, or c` |
| [format_message](format_message.md) | `message_format.h` | a message of ICU's MessageFormat read and written once |
| [format_number](format_number.md) | `number.h` | a number as a locale writes it: digits, separators, percent, scientific, compact forms, currencies |
| [format_relative](format_relative.md) | `relative_time.h` | a time relative to now as a locale says it: `za 3 dni`, `yesterday`, of a value of a unit or of a span |
| [format_to](format_to.md) | `format.h` | the text of `format` into memory the caller lends, answering what the whole text takes |
| [from_utf16](from_utf16.md) | `encoding.h` | UTF-16 units as text |
| [from_utf32](from_utf32.md) | `encoding.h` | code points as text |
| [grapheme_count](grapheme_count.md) | `segment.h` | the characters of a text a reader counts |
| [grapheme_next](grapheme_next.md) | `segment.h` | the grapheme boundary after a position: a caret's right arrow |
| [grapheme_prev](grapheme_prev.md) | `segment.h` | the grapheme boundary before a position: a caret's left arrow, a backspace |
| [grapheme_start](grapheme_start.md) | `segment.h` | the start of the grapheme that holds a position |
| [hash_normalized](hash_normalized.md) | `normalize.h` | a hash equal_normalized agrees with |
| [identifier_status_of](identifier_status_of.md) | `identifier.h` | whether UTS #39 allows a code point in a name |
| [identifier_type_of](identifier_type_of.md) | `identifier.h` | what UTS #39 says is wrong with a code point in a name, a set |
| [is_allowed_identifier](is_allowed_identifier.md) | `identifier.h` | checks whether every code point of a text is allowed in a name |
| [is_alnum](is_alnum.md) | `properties.h` | checks whether a code point is a letter or a decimal digit |
| [is_alpha](is_alpha.md) | `properties.h` | checks whether a code point is a letter, Go's `unicode.IsLetter` |
| [is_confusable](is_confusable.md) | `identifier.h` | checks whether two texts look alike, by their skeletons |
| [is_control](is_control.md) | `properties.h` | checks whether a code point is a control, C0 or C1 |
| [is_digit](is_digit.md) | `properties.h` | checks whether a code point is a decimal digit, in any script |
| [is_emoji](is_emoji.md) | `properties.h` | checks whether a code point is an emoji, Extended_Pictographic |
| [is_format](is_format.md) | `properties.h` | checks whether a code point is a formatting one: the joiners, the marks of direction, the soft hyphen |
| [is_highly_restrictive](is_highly_restrictive.md) | `identifier.h` | checks whether a name stands on the rung `highly_restrictive` or below |
| [is_identifier](is_identifier.md) | `identifier.h` | checks whether a text is an identifier: UAX #31, rule R1 with R1a |
| [is_identifier_continue](is_identifier_continue.md) | `identifier.h` | checks whether a code point may continue an identifier, `XID_Continue` |
| [is_identifier_start](is_identifier_start.md) | `identifier.h` | checks whether a code point may begin an identifier, `XID_Start` |
| [is_lower](is_lower.md) | `properties.h` | checks whether a code point has an upper case form other than itself, core's `unicode::is_lower` |
| [is_mark](is_mark.md) | `properties.h` | checks whether a code point is a combining, spacing or enclosing mark |
| [is_mirrored](is_mirrored.md) | `bidi.h` | checks whether a code point is drawn mirrored in a right to left run, `Bidi_Mirrored` |
| [is_moderately_restrictive](is_moderately_restrictive.md) | `identifier.h` | checks whether a name stands on the rung `moderately_restrictive` or below |
| [is_nfkc_casefolded](is_nfkc_casefolded.md) | `identifier.h` | checks whether a text is in NFKC_Casefold already |
| [is_normalized](is_normalized.md) | `normalize.h` | checks whether a text is in a normalization form already, by the quick check properties |
| [is_printable](is_printable.md) | `properties.h` | checks whether a terminal can show a code point, Go's `unicode.IsPrint` |
| [is_punct](is_punct.md) | `properties.h` | checks whether a code point is a punctuation mark |
| [is_single_script](is_single_script.md) | `identifier.h` | checks whether a text is written in one script |
| [is_space](is_space.md) | `properties.h` | checks whether a code point is white space, core's `unicode::is_space` |
| [is_upper](is_upper.md) | `properties.h` | checks whether a code point has a lower case form other than itself, core's `unicode::is_upper` |
| [jaro](jaro.md) | `distance.h` | Jaro's similarity of two short texts, in [0, 1] |
| [jaro_winkler](jaro_winkler.md) | `distance.h` | Jaro's similarity raised for a common prefix, for names |
| [lcs_length](lcs_length.md) | `distance.h` | the length of the longest common subsequence, bit-parallel |
| [levels](levels.md) | `bidi.h` | the bidirectional level of every code point of a text, UAX #9 |
| [levenshtein](levenshtein.md) | `distance.h` | the edit distance of two texts, by code points or bytes, bit-parallel, with a cutoff |
| [markdown_to_html](markdown_to_html.md) | `markdown.h` | the HTML of Markdown: CommonMark 0.31 and GitHub's tables, strikethrough, autolinks and task lists, safe by default |
| [merge3](merge3.md) | `diff.h` | the three-way merge of `git merge-file`: changes of both sides taken, conflicts between markers |
| [mirrored](mirrored.md) | `bidi.h` | a text with rule L4 of UAX #9 applied, its brackets swapped where they run right to left |
| [mirrored_of](mirrored_of.md) | `bidi.h` | the code point of the mirrored shape |
| [name_of](name_of.md) | `encoding.h` | the name a header uses for an encoding: `"iso-8859-2"` |
| [nfkc_casefold](nfkc_casefold.md) | `identifier.h` | the form names are compared in, NFKC_Casefold |
| [normalize](normalize.md) | `normalize.h` | a text in one of the four forms of UAX #15 |
| [numeric_value_of](numeric_value_of.md) | `properties.h` | the value of a decimal digit in any script, -1 for none |
| [ordinal_of](ordinal_of.md) | `plural.h` | the ordinal plural category of an integer in a locale's language: `2nd` is `two` in English |
| [osa_distance](osa_distance.md) | `distance.h` | the optimal string alignment distance: Levenshtein's edits and transpositions, no unit edited twice |
| [paragraph_direction](paragraph_direction.md) | `bidi.h` | which way a text runs, by the first strong character of its first paragraph |
| [plural_of](plural_of.md) | `plural.h` | the cardinal plural category of a number in a locale's language, as written: `5 plików` is `many` |
| [restriction_level_of](restriction_level_of.md) | `identifier.h` | where a name stands on the ladder of UTS #39 §5.2 |
| [runtime](runtime.md) | `format.h` | a pattern the compiler never saw, asked for by name: `txt::format(txt::runtime(entry), n)` |
| [sanitize_html](sanitize_html.md) | `html.h` | HTML of an allowlist: parsed as a browser reads it, written with only what the list keeps |
| [script_of](script_of.md) | `properties.h` | the script a code point is written in |
| [skeleton](skeleton.md) | `identifier.h` | the key two texts that look alike share, UTS #39 §4 |
| [to_lower_full](to_lower_full.md) | `case.h` | a text in lower case by the full mapping: the final sigma, the Turkish and Lithuanian i |
| [to_title](to_title.md) | `case.h` | the first letter of every word in title case, the rest in lower case |
| [to_upper_full](to_upper_full.md) | `case.h` | a text in upper case by the full mapping: `"straße"` is `"STRASSE"` |
| [to_utf16](to_utf16.md) | `encoding.h` | a text as UTF-16 units |
| [to_utf32](to_utf32.md) | `encoding.h` | a text as code points |
| [truncate](truncate.md) | `segment.h` | a text cut to a number of columns at a grapheme boundary, the ellipsis counted |
| [unified_diff](unified_diff.md) | `diff.h` | the differences of two texts in the unified format of `diff -u` and `git diff` |
| [visual_order](visual_order.md) | `bidi.h` | the byte positions of the code points in the order they are drawn |
| [without_marks](without_marks.md) | `normalize.h` | a text with its nonspacing marks taken off |
| [wrap](wrap.md) | `segment.h` | a text laid into lines of a number of columns, UAX #14 |
| [write_padded](write_padded.md) | `format.h` | a text in its field — fill, alignment, width in columns, precision on a cluster — for the `format_value` of a type of one's own |

## Classes

| Class | Header | Description |
|---|---|---|
| [bidi_runs](bidi_runs/README.md) | `bidi.h` | the pieces of a text in the order they are drawn, each with its level |
| [byte_order_mark](byte_order_mark/README.md) | `encoding.h` | what a byte order mark says and how many bytes it takes |
| [catalog](catalog/README.md) | `catalog.h` | one language's translations as GNU gettext keeps them: .po and .mo files, contexts, plural forms by the catalog's own rule |
| [catalog_error](catalog_error/README.md) | `catalog.h` | where a .po or .mo file stopped being one, and why |
| [code_error](code_error/README.md) | `locale.h` | why a text is not a code of a currency, a region or a script: `offset()`, `what()`, `message()` |
| [collated_matches](collated_matches/README.md) | `collate.h` | every occurrence of a pattern by a collator's equality, the text weighed once |
| [collated_searcher](collated_searcher/README.md) | `collate.h` | a pattern weighed once by a collator |
| [collated_text](collated_text/README.md) | `collate.h` | a text weighed once by a collator and asked many questions |
| [collator](collator/README.md) | `collate.h` | the order a reader expects (UTS #10): a comparator, a sort key and a search, in the root order of the DUCET or of 88 languages from CLDR |
| [currency](currency/README.md) | `currency.h` | a currency of ISO 4217: its digits, cash rounding, symbols in every locale, the currency of a region |
| [decode_error](decode_error/README.md) | `encoding.h` | why bytes are not text in an encoding: the first byte that means nothing |
| [diff_edit](diff_edit.md) | `diff.h` | one run of an edit script: equal, inserted or removed, as byte ranges of both texts |
| [diff_options](diff_options.md) | `diff.h` | how an edit script is found: the algorithm, white space |
| [distance_options](distance_options.md) | `distance.h` | how a distance is measured: a cutoff, bytes or code points |
| [fold_matches](fold_matches/README.md) | `search.h` | every occurrence without regard to case, the text folded once |
| [fold_searcher](fold_searcher/README.md) | `search.h` | a pattern folded once, asked of many texts |
| [folded_text](folded_text/README.md) | `search.h` | a text folded once and asked many questions |
| [format_part](format_part.md) | `format.h` | one step of a pattern read where the program is compiled: a run of literal text and the field after it |
| [format_pattern\<A...\>](format_pattern/README.md) | `format.h` | the pattern of `format`, read and checked where the program is compiled; what a function of one's own takes to have its pattern checked |
| [format_sink](format_sink/README.md) | `format.h` | where a value is written: room the caller lends, and a count of what the whole takes |
| [format_spec](format_spec.md) | `format.h` | one field's specification as it was read, `[[fill]align][sign][#][0][width][.precision][type]` |
| [formatter\<T\>](formatter/README.md) | `format.h` | what a type takes in a field and how it is written; the library's specializations and the program's own |
| [graphemes](graphemes/README.md) | `segment.h` | the grapheme clusters of a text: what a reader calls a character |
| [growing_sink](growing_sink/README.md) | `format.h` | a sink whose room grows, for a long text written in steps, one step written again when it ran off the end |
| [html_attribute](html_attribute.md) | `html.h` | an attribute of a parsed element |
| [html_document](html_document/README.md) | `html.h` | a document parsed by the WHATWG HTML algorithm, as a browser reads it: every insertion mode, SVG and MathML, templates, fragments |
| [html_node](html_node/README.md) | `html.h` | a node of a parsed document: its kind, name, attributes, children, text and HTML |
| [html_options](html_options.md) | `html.h` | how a document is parsed: the scripting flag, whether errors are kept |
| [html_parse_error](html_parse_error.md) | `html.h` | an error a parse recovered from: its offset and the standard's code |
| [html_sanitizer_options](html_sanitizer_options.md) | `html.h` | the allowlist of sanitize_html: elements, attributes, URL schemes |
| [html_stencil](html_stencil/README.md) | `html_stencil.h` | a template whose page is HTML: stencil's syntax, every field escaped for its place (text, attribute, URL, script, style), Go's `html/template` |
| [jaro_options](jaro_options.md) | `distance.h` | Winkler's prefix scale, prefix length and threshold |
| [line_breaks](line_breaks/README.md) | `segment.h` | the pieces of a text that must stay on one line, UAX #14 |
| [list](list/README.md) | `stencil.h` | a list of values written as data, `txt::list{1, 2, 3}` |
| [locale](locale/README.md) | `locale.h` | a language, script and region from a BCP-47 tag or a POSIX name: what the case mappings, the collation and CLDR's formatting depend on; likely subtags, parents, autonyms |
| [mapped_text](mapped_text.md) | `search.h` | a text as a folded or normalized search sees it: its code points and the byte each came from |
| [markdown_document](markdown_document/README.md) | `markdown.h` | Markdown read into a tree to walk or to write as HTML |
| [markdown_node](markdown_node/README.md) | `markdown.h` | a node of a Markdown document: a block or an inline, its children and what its kind holds |
| [markdown_options](markdown_options.md) | `markdown.h` | GitHub's extensions, raw HTML, line breaks and URL safety of Markdown |
| [match](match/README.md) | `regex.h` | one match of a regex: its text, its place and its groups |
| [merge_options](merge_options.md) | `diff.h` | the labels of a merge's conflict markers, and whether they show the base |
| [merge_result](merge_result.md) | `diff.h` | a merged text and the number of its conflicts |
| [message_error](message_error/README.md) | `message_format.h` | where a message stopped being one, and why |
| [message_format](message_format/README.md) | `message_format.h` | a message of ICU's MessageFormat 1 read once for a locale: plurals, selects, numbers, dates |
| [nfc_t, nfd_t, nfkc_t, nfkd_t](nfc_t.md) | `normalize.h` | the tags of the four normalization forms |
| [normalized_matches](fold_matches/README.md) | `search.h` | every occurrence without regard to how the text was written, the text decomposed once |
| [normalized_searcher](fold_searcher/README.md) | `search.h` | a pattern decomposed once, asked of many texts |
| [normalized_text](folded_text/README.md) | `search.h` | a text decomposed once and asked many questions |
| [number_format](number_format/README.md) | `number.h` | a locale's way of writing numbers with options, resolved once, for many numbers |
| [number_options](number_options.md) | `number.h` | how a number is written: style, digits, grouping, sign, rounding, currency |
| [object](object/README.md) | `stencil.h` | a mapping of names to values written as data, in the order written, `txt::object{{"a", 1}}` |
| [occurrence](occurrence.md) | `search.h` | where a folded or normalized search found its pattern: the position and the bytes it covers |
| [patch_error](patch_error/README.md) | `diff.h` | why a patch did not apply: the hunk, its line in the patch, the reason |
| [patch_options](patch_options.md) | `diff.h` | how a patch is applied: the fuzz, reversal |
| [percent_set](percent_set/README.md) | `percent.h` | the ASCII characters a percent encoding leaves alone, a mask of 128 bits that composes with `\|` and `-` |
| [program_syntax_t](program_syntax_t.md) | `identifier.h` | the tag of the profile of UAX #31 with `_` and `$` |
| [regex](regex/README.md) | `regex.h` | a pattern in the style of RE2, matched in time linear in the length of the text: no backreference and no lookaround |
| [regex_error](regex_error/README.md) | `regex.h` | why a pattern was refused, and where |
| [regex_matches](regex_matches/README.md) | `regex.h` | every match of a regex in a text, as a range |
| [region](region/README.md) | `locale.h` | a region of ISO 3166 or UN M.49: its code and its name in a language |
| [relative_options](relative_options.md) | `relative_time.h` | how a relative time is written: width, the number kept or a word |
| [runtime_pattern](runtime_pattern/README.md) | `format.h` | a pattern read where the program runs, keeping its string |
| [script_code](script_code/README.md) | `locale.h` | a script of ISO 15924: its code and its name in a language |
| [searcher](searcher/README.md) | `search.h` | a pattern of bytes prepared once, Boyer–Moore–Horspool |
| [sentences](sentences/README.md) | `segment.h` | the sentences of a text, UAX #29 |
| [stencil](stencil/README.md) | `stencil.h` | a template read once into steps and rendered many times: `{{ name }}`, paths, `if`, `range`, `with`, comments, white-space trimming, fields written by `format`'s own writers, a pipeline of functions; Go's `text/template` with a bare name for a field, nothing for a missing name, and no escaping that knows where a value lands ([html_stencil](html_stencil/README.md) has it) |
| [stencil_error](stencil_error/README.md) | `stencil.h` | where and why a source is not a template: `offset()`, `line()`, `column()`, `message()` |
| [stencil_function](stencil_function.md) | `stencil.h` | a function of a pipeline, `function<value(const value&, slice<const value>)>`; pure of side effects |
| [stencil_functions](stencil_functions/README.md) | `stencil.h` | the functions a pipeline may call: `upper`, `lower`, `title`, `trim`, `escape_html`, `default`, and the program's own |
| [strict_t](strict_t.md) | `encoding.h` | the tag of the strict `decode` |
| [tab_options](tab_options.md) | `tab_writer.h` | how tab-separated cells are aligned: widths, padding, alignment, HTML, escapes |
| [tab_writer](tab_writer/README.md) | `tab_writer.h` | `align_tabs` streamed: text taken in pieces, lines given back aligned when their columns end |
| [unified_options](unified_options.md) | `diff.h` | how a unified diff is written: the context, the names, the algorithm |
| [value](value/README.md) | `stencil.h` | one value handed to a template: nothing, a truth, a number, text, a list or a mapping, in thirty-two bytes |
| [word_breaks](word_breaks/README.md) | `segment.h` | the words of a text and the runs between them, UAX #29 |
| [words](words/README.md) | `segment.h` | the words of a text alone |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [bidi](bidi.md) | `bidi.h` | the bidirectional class of a code point, UAX #9 |
| [case_order](case_order.md) | `collate.h` | which case a collator puts first |
| [category](category.md) | `properties.h` | the general category of a code point, spelled out as ICU spells it |
| [currency_display](currency_display.md) | `number.h` | what is written for a currency: its symbol, narrow symbol or code |
| [diff_algorithm](diff_algorithm.md) | `diff.h` | how an edit script is found: Myers or patience |
| [diff_kind](diff_kind.md) | `diff.h` | what a run of an edit script does: equal, insert, remove |
| [direction](direction.md) | `bidi.h` | which way a paragraph runs: automatic, left to right, right to left |
| [encoding](encoding.md) | `encoding.h` | UTF-8, UTF-16 and UTF-32 in both orders, ASCII, ISO-8859-1 and the 27 single byte encodings of the WHATWG's list |
| [html_namespace](html_namespace.md) | `html.h` | the namespace of an element: html, svg, mathml |
| [html_node_kind](html_node_kind.md) | `html.h` | what a node is: document, doctype, element, text, comment, fragment |
| [identifier_status](identifier_status.md) | `identifier.h` | whether UTS #39 allows a code point in a name: restricted, allowed |
| [identifier_type](identifier_type.md) | `identifier.h` | what UTS #39 says of a code point in a name, a set of bits |
| [list_type](list_type.md) | `list_format.h` | what a list joins: all, one of them, the parts of a measure |
| [markdown_align](markdown_align.md) | `markdown.h` | a table column's alignment: none, left, center, right |
| [markdown_kind](markdown_kind.md) | `markdown.h` | what a node of a Markdown document is: the blocks and the inlines |
| [name_context](name_context.md) | `locale.h` | the form of a name: inside a phrase or standing alone |
| [number_style](number_style.md) | `number.h` | decimal, percent, per mille, scientific, compact, currency, accounting |
| [plural](plural.md) | `plural.h` | CLDR's plural categories: zero, one, two, few, many, other |
| [punctuation](punctuation.md) | `collate.h` | whether a collator counts punctuation or shifts it to the fourth level |
| [restriction_level](restriction_level.md) | `identifier.h` | the ladder of UTS #39 §5.2, from `ascii_only` to `unrestricted` |
| [script](script.md) | `properties.h` | the script of a code point, one for each script of Unicode 16, `common`, `inherited` and `unknown` |
| [sign_display](sign_display.md) | `number.h` | which numbers get a sign |
| [strength](strength.md) | `collate.h` | how much of a difference counts to a collator: letters, accents, case, punctuation |
| [time_unit](time_unit.md) | `relative_time.h` | the unit of a relative time: second to year |
| [value_kind](value_kind.md) | `stencil.h` | what a `value` holds: `none`, `boolean`, `integer`, `real`, `text`, `list`, `object` |
| [width](width.md) | `locale.h` | wide, abbreviated, narrow: the widths of CLDR's names and phrases |

## Constants

| Constant | Header | Description |
|---|---|---|
| `nfc` | `normalize.h` | the tag of the canonical composition, NFC ([nfc_t](nfc_t.md)) |
| `nfd` | `normalize.h` | the tag of the canonical decomposition, NFD ([nfc_t](nfc_t.md)) |
| `nfkc` | `normalize.h` | the tag of the compatibility composition, NFKC ([nfc_t](nfc_t.md)) |
| `nfkd` | `normalize.h` | the tag of the compatibility decomposition, NFKD ([nfc_t](nfc_t.md)) |
| `program_syntax` | `identifier.h` | the tag of the profile with `_` and `$` ([program_syntax_t](program_syntax_t.md)) |
| `strict` | `encoding.h` | the tag of the strict `decode`: `decode(bytes, from, txt::strict)` ([strict_t](strict_t.md)) |
| `version` | `properties.h` | the version of Unicode the tables are made from, `"16.0.0"`: [unicode::version](../core/unicode/README.md) |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [idna](idna/README.md) | `idna.h` | a domain name between Unicode and the ASCII the DNS carries: `to_ascii`, `to_unicode`, UTS #46 |
| [percent](percent/README.md) | `percent.h` | the escaping of RFC 3986 — `encode` and `decode` with the sets of the RFC and of the WHATWG URL Standard ready made, and a decoding that refuses a `%` that was cut |
| [punycode](punycode/README.md) | `idna.h` | one label between Unicode and ASCII, RFC 3492: `encode`, `decode` |

## See also

- [Display names](names.md): the optional headers of the names of languages, regions, scripts, currencies and zones,
  one a locale
- [Benchmarks](benchmarks.md): the tables, format, stencil, regex, the searches, the collation and the identifiers
  against `std`, Go and ICU
- [utf8](../core/utf8/README.md), [unicode](../core/unicode/README.md), [runes](../core/runes/README.md), [string](../core/string/README.md):
  text at the level of bytes and code points, in core
- [time](../time/README.md#formatting-with-txt): the patterns of time in `format`
- [net::url](../net/url/README.md): the escaping of a URL, built on [percent](percent/README.md) and [idna](idna/README.md)
- [The modules](../README.md)
