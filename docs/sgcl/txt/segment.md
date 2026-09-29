# sgcl::txt::segment

```cpp
#include "sgcl/txt/segment.h"   // or "sgcl/txt/txt.h"

class graphemes;      // the grapheme clusters: what a reader calls a character
class word_breaks;    // the words and the runs between them
class words;          // the words alone
class sentences;      // the sentences, the space after the stop with the sentence it closes
class line_breaks;    // the pieces that must stay on one line, their trailing spaces included

size_t grapheme_count(const string& text) noexcept;        // also slice<const char>, char array, char pointer
size_t grapheme_start(const string& text, size_t pos) noexcept;
size_t grapheme_next(const string& text, size_t pos) noexcept;
size_t grapheme_prev(const string& text, size_t pos) noexcept;

vector<slice<const char>> wrap(const string& text, size_t width);
string truncate(const string& text, size_t width);
string truncate(const string& text, size_t width, const string& ellipsis);
```

Where a text may be cut: between the characters a human counts (UAX #29), between words, between sentences, and where a line may be broken (UAX #14). Each of the four is a range whose element is a slice of the text; nothing is copied, nothing is allocated per element, and the slice holds the text's object, so a loop over a temporary is safe.

## A character is not a code point

A grapheme is one combining sequence, one flag (two regional indicators), one emoji with its skin tone or its joined family, one Hangul syllable written as jamo, one Devanagari consonant with its vowel sign or its conjunct, and one `"\r\n"`. It is what a caret steps over, what a backspace deletes and what a limit of "twenty characters" should count.

`grapheme_next` and `grapheme_prev` move a cursor, `grapheme_start` puts a position that fell inside a character back on its start. A position past the end is the end; `prev` of 0 is 0.

## The words and what is between them

`word_breaks` cuts at every word boundary, which gives the words **and** the runs between them — that is what the annex defines and what a double click, or a text put back together word by word, needs. A run of spaces is one segment while each punctuation mark is its own. `words` gives the words alone: the same segments, those with a letter or a digit in them.

The rules keep an apostrophe and a decimal point inside a word (`don't`, `3.14`, `192.168.0.1`) and cut at a hyphen (`e-mail` is two).

## The sentences

A full stop is not enough and not always needed: `"i.e."` and `"U.S.A."` stay inside one sentence, a stop followed by a lower case letter ends nothing, and a paragraph separator ends a sentence without any punctuation. Every byte belongs to exactly one sentence, the space after the stop with the sentence it closes.

These are the rules of the annex and nothing more, so an initial before a capitalised name — `"Pan J. Kowalski"` — is two sentences. Doing better needs a list of a language's abbreviations, which the annex leaves to the caller.

## Lines

`line_breaks` gives the pieces that must stay together. A line may be broken after a space or a hyphen and between two ideographs; it may not be broken between a number and its decimal mark, inside `"(a)"`, or at a no-break space.

`wrap` lays those pieces into lines no wider than `width` **columns** ([`columns`](properties.md), terminal cells, not bytes and not code points), dropping the trailing spaces of each line. The lines are slices of the text, so nothing is copied. A piece wider than the limit takes a line of its own and overflows it, because the alternative is cutting a word in half; the hard breaks the text already has are kept.

`truncate` cuts to `width` columns with the ellipsis counted inside that number and the cut made at a grapheme boundary — never inside a character. A text that already fits comes back as the same object.

Unlike the other three, `line_breaks` carries state in its iterator: rule LB15a asks what stood before an opening quotation mark, and that may be on the other side of a break opportunity, so the scan runs from the beginning of the text. Walking the range is still linear.

**The scripts that write without spaces are the limit of this.** In Thai, Lao, Khmer and Burmese a line may be broken between words, and the words are not marked: finding them takes a dictionary of the language, a few hundred kilobytes of one. UAX #14 gives those characters the class SA and says an implementation without a dictionary resolves it from the category — which is what rule LB1 does here, making them ordinary letters. So a text in those scripts breaks between characters rather than between words: correct by the rules, and not what somebody who reads them expects. The day it matters it is a dictionary and a segmentation of its own, not another rule.

## What it is held to

The oracle is the UCD's own test files, every case of each, compiled into the tests with the tables:

| file | cases |
|---|---|
| `GraphemeBreakTest.txt` | 1093 |
| `WordBreakTest.txt` | 1826 |
| `SentenceBreakTest.txt` | 512 |
| `LineBreakTest.txt` | 16672 |

All of them pass, with no rule tailored and none skipped. The tables are 55 KB: Sentence_Break 17.7, Line_Break 14.8 with rule LB1 already resolved in it, Grapheme_Cluster_Break 9.5, Word_Break 9.1, Indic_Conjunct_Break 3.3 (rule GB9c) and the East Asian set of UAX #14 0.7 — each split at the end of the Basic Multilingual Plane, as [the properties](properties.md#the-tables) are.

## Members

### The ranges

```cpp
explicit graphemes(const string& text);          // word_breaks, words, sentences, line_breaks alike
explicit graphemes(const slice<const char>& text);
template<size_t N>
explicit graphemes(const char (&text)[N]);       // to its first NUL or its end, copied into a string it holds
explicit graphemes(const char* text);            // char* too: to its NUL, copied likewise
```

Each is constructed from a [`string`](../core/string.md), a [`slice<const char>`](../core/slice.md), an array of `char` (read up to its first NUL or its end, never past it) or a `const char*` or `char*` (up to its NUL) — `graphemes(s)` looks like a call and is a construction, as [`runes(s)`](../core/utf8.md) is — and each is a range of the library ([`mixin::enumerable`](../core/mixin/enumerable.md)), so `count`, `count_of`, `exists`, `find_if` and `for_each` work on it. The iterator's `pos()` is the byte position of the segment and `size()` its length, for the code that goes back to the bytes.

## Examples

A character is not a code point:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string s = "é\U0001F1F5\U0001F1F1";  // e with a combining acute, then a Polish flag
    println("{} bytes, {} code points, {} characters", s.size(), s.rune_count(),
            txt::grapheme_count(s));
    return 0;
}
```

Output:

```text
11 bytes, 4 code points, 2 characters
```

The words, and the words with what is between them:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string s = "can't stop, won't stop";
    println("{} words, {} segments", txt::words(s).count(), txt::word_breaks(s).count());
    return 0;
}
```

Output:

```text
4 words, 8 segments
```

One text counted every way there is:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    println("{} bytes, {} code points, {} characters, {} columns", text.size(), text.rune_count(),
            txt::grapheme_count(text), txt::columns(text));
    println("{} words, {} sentence", txt::words(text).count(), txt::sentences(text).count());
    return 0;
}
```

Output:

```text
67 bytes, 46 code points, 45 characters, 48 columns
10 words, 1 sentence
```

A paragraph laid out in a column of a given width:

```cpp
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    for (auto line : txt::wrap(text, 24)) {
        print("|{}", line);
        for (int i : range(24 - txt::columns(line))) {
            print(" ");
        }
        println("|");
    }
    return 0;
}
```

Output:

```text
|Zażółć gęślą jaźń — a   |
|potem 漢字 i 🇵🇱 na      |
|koniec.                 |
```

Cut to twenty columns, the ellipsis counted inside them:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    println("{}", txt::truncate(text, 20));
    return 0;
}
```

Output:

```text
Zażółć gęślą jaźń —…
```

The caret steps over the flag as one character, not four bytes:

```cpp
#include "sgcl/io/io.h"
#include "sgcl/txt/txt.h"

using namespace sgcl;

int main() {
    string text = "Zażółć gęślą jaźń — a potem 漢字 i 🇵🇱 na koniec.";
    size_t caret = text.find("🇵🇱");
    println("{} -> {}", caret, txt::grapheme_next(text, caret));
    return 0;
}
```

Output:

```text
48 -> 56
```

## See also

[The module](README.md); [`properties`](properties.md), `columns`; [`utf8`](../core/utf8.md), `runes`, the code points of a text; [`bidi`](bidi.md), the order a line is drawn in; [`search`](search.md), whose matches take whole characters.
