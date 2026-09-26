# txt::bidi

```cpp
#include "sgcl/txt/bidi.h"
```

Text that runs both ways at once. Arabic, Hebrew, Persian and Urdu are written right to left, but the numbers inside them run left to right, and so does a Latin word quoted in them. One line then has pieces going both ways, and the order the characters are **stored** in — the logical order, the order they are typed and read — is not the order they are **drawn** in.

```
stored:  Nazwa: שלום 123 OK
drawn:   Nazwa: OK 123 םולש
```

The algorithm of [UAX #9](https://www.unicode.org/reports/tr9/) gives every character a level: 0 runs left to right, 1 right to left, 2 left to right inside a right to left piece, and so on. What is drawn is the pieces of odd level turned round.

This is for whoever draws the text — a user interface, a terminal — and for moving a caret through it. Storing, searching and comparing need none of it.

## The names

```cpp
enum class direction : uint8_t { automatic, left_to_right, right_to_left };

bidi bidi_class_of(char32_t c) noexcept;                    // the class of one code point
direction paragraph_direction(const string& text);         // by its first strong character

vector<uint8_t> levels(const string& text, direction = direction::automatic);
vector<size_t> visual_order(const string& text, direction = direction::automatic);

class bidi_runs {
    explicit bidi_runs(const string& text, direction = direction::automatic);
    struct run { slice<const char> text; uint8_t level; bool right_to_left() const; };
    // a range of the library over the runs, in the order they are drawn
    direction paragraph() const noexcept;
};

bool is_mirrored(char32_t c) noexcept;              // Bidi_Mirrored
char32_t mirrored_of(char32_t c) noexcept;          // the mirrored shape, or the code point itself

string mirrored(const string& text, direction = direction::automatic);   // rule L4 over a text
string mirrored(const string& text, const vector<uint8_t>& levels);      // with the levels in hand
```

`bidi_runs` is what a renderer wants: it walks the pieces **in the order they go on the line**, left to right, and each piece carries the level it runs at. A piece of odd level has its characters drawn in reverse. Nothing is copied — every piece is a slice of the text.

`visual_order` is the same answer one character at a time: the byte position of every code point in the order it is drawn, the ones rule X9 removes left out. That is what a caret steps over in mixed text, where the right arrow key may move backwards through the bytes.

`levels` is the raw answer for a renderer that lays the text out itself.

## What the algorithm is told and what it works out

`direction::automatic` lets the text decide: the first strong character — a letter, not a digit — sets the paragraph. `"123 שלום"` runs right to left, because a number is not strong and the Hebrew behind it is. A paragraph with no strong character at all runs left to right. A caller who knows better (a user interface with a language setting, a protocol that says so) passes `left_to_right` or `right_to_left` instead.

Everything else the algorithm works out on its own: the embedding and override characters, the isolates of Unicode 6.3, the weak types (a number after an Arabic letter is an Arabic number), the neutrals between two directions, and the brackets — `(` and `)` take one direction together, so a parenthesis does not flip away from what it encloses.

## Mirroring

A bracket in a right to left run is drawn the other way round: the character that opens a parenthesis in Arabic has to be **shown** as `)`, because the line runs the other way and the shape has to follow it. That is rule L4 of the annex — a character is drawn mirrored when its resolved level is odd and its `Bidi_Mirrored` property is yes — and it is a substitution of one character for another, nothing more. It is **not** shaping: joining an Arabic letter to its neighbours, choosing an initial or a final form, forming a ligature and placing a mark are a font's work and none of it is here.

`mirrored(text)` is the rule over a whole text: every such character swapped for the code point of its mirrored shape, the rest left alone, and the whole kept in the order it is **stored** in. A renderer holds this and the pieces of `bidi_runs` and has what it needs — the levels say which pieces to turn round, this says which glyphs to change, and neither is any use without the other.

```cpp
auto glyphs = txt::mirrored(line);           // the characters to draw
for (auto run : txt::bidi_runs(line)) {      // the order to draw them in
    ...
}
```

Because whoever draws the text has worked the levels out already — `bidi_runs` and `levels` both run the whole algorithm — there is a second form that takes them. It is the rule and nothing else, and on a mixed line of fifty-one bytes that is **199 ns against 1464**. The levels are one to a code point, as `levels` gives them; a code point the vector does not reach is left where it stands, and levels of the caller's own making are obeyed as given.

```cpp
auto lv = txt::levels(line);
auto glyphs = txt::mirrored(line, lv);       // the paragraph not worked out a second time
```

A text with nothing to mirror comes back as **the same object**, which is most texts: the first pass over it only asks.

`is_mirrored` and `mirrored_of` are the two questions about one code point, objects of the shape the rest of the module uses. They are not the same set: **554** code points are `Bidi_Mirrored` and only **428** have a mirror of their own, an integral sign being drawn the other way round without there being a second one to name it. `mirrored_of` answers the code point itself where there is no other, and a font mirrors the glyph.

```cpp
is_mirrored(U'(');          // true
is_mirrored(U'a');          // false
mirrored_of(U'(');          // ')'
mirrored_of(U'≤');          // '≥'
mirrored_of(U'∫');          // '∫' — mirrored, but with no code point of its own
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A label, a Hebrew word, a number and a Latin word on one line: what is
// stored, and what a renderer would put on the line
int main() {
    string line = "Nazwa: שלום 123 OK";
    std::cout << "akapit biegnie "
              << (txt::paragraph_direction(line) == txt::direction::right_to_left ? "w lewo" : "w prawo") << '\n';
    for (auto run : txt::bidi_runs(line)) {
        std::cout << "  poziom " << int(run.level) << (run.right_to_left() ? " w lewo  " : " w prawo ")
                  << '[' << run.text << "]\n";
    }

    // the caret walks the text in the order it is shown
    string mixed = "aאבb";
    std::cout << "pozycje bajtów w kolejności rysowania:";
    for (auto at : txt::visual_order(mixed)) {
        std::cout << ' ' << at;
    }
    std::cout << '\n';

    // and the brackets inside the Hebrew point the other way when drawn
    string brackets = "א (ב) [ג]";
    std::cout << "zapisane: " << brackets << "\nrysowane:  " << txt::mirrored(brackets) << '\n';
    std::cout << "a po polsku: " << txt::mirrored(string("Ala (ma) kota")) << '\n';
    return 0;
}
```

The output:

```
akapit biegnie w prawo
  poziom 0 w prawo [Nazwa: ]
  poziom 2 w prawo [123]
  poziom 1 w lewo  [שלום ]
  poziom 0 w prawo [ OK]
pozycje bajtów w kolejności rysowania: 0 3 1 5
zapisane: א (ב) [ג]
rysowane:  א )ב( ]ג[
a po polsku: Ala (ma) kota
```

## What it is held to

`BidiCharacterTest.txt` of the UCD, which gives for every case the level the paragraph resolves to, the level of every character and the order they are drawn in. **All 91 707 of its cases pass**; the header the tests carry holds every third of them, 30 569, because the whole file is 7.8 MB of test data — four times everything else the tests carry — and the full set is run by hand before a change to this header lands (`python3 tools/unicode_tables.py --all-bidi`).

Rule L4 has two oracles of its own. `BidiMirroring.txt` is checked **whole**, all 428 of its lines as the file writes them and not as the table made from it has them: every mapping, that every mapping is its own inverse, that everything mapped is `Bidi_Mirrored`, and that no other code point of the 1 112 064 has a mirror. And the rule over a text is checked against the levels `BidiCharacterTest.txt` itself gives, over all 30 569 cases, 17 017 of which have something to mirror — an oracle that does not depend on the levels this library works out, which the test above already weighs against the same file. `BidiTest.txt` is no use here: it gives classes rather than characters, and a class has no glyph to mirror.

The tables are 13.4 KB: the class of every code point, 10.2, the 128 bracket pairs, 1.0, and the mirroring, 2.1 — the 428 pairs as `uint16` (both halves of every one of them are in the Basic Multilingual Plane) and the `Bidi_Mirrored` property as 114 ranges. The classes come from `DerivedBidiClass.txt` rather than from the assigned code points alone, because the file gives a class to the unassigned ones too — the ranges of Hebrew and Arabic run right to left before anything is put in them, and a text with a code point from a future version must still lay out sensibly.
