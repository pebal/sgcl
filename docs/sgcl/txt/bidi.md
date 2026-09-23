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

bidi direction_of(char32_t c) noexcept;                    // the class of one code point
direction paragraph_direction(const string& text);         // by its first strong character

vector<uint8_t> levels(const string& text, direction = direction::automatic);
vector<size_t> visual_order(const string& text, direction = direction::automatic);

class bidi_runs {
    explicit bidi_runs(const string& text, direction = direction::automatic);
    struct run { slice<const char> text; uint8_t level; bool right_to_left() const; };
    // a range of the library over the runs, in the order they are drawn
    direction paragraph() const noexcept;
};
```

`bidi_runs` is what a renderer wants: it walks the pieces **in the order they go on the line**, left to right, and each piece carries the level it runs at. A piece of odd level has its characters drawn in reverse. Nothing is copied — every piece is a slice of the text.

`visual_order` is the same answer one character at a time: the byte position of every code point in the order it is drawn, the ones rule X9 removes left out. That is what a caret steps over in mixed text, where the right arrow key may move backwards through the bytes.

`levels` is the raw answer for a renderer that lays the text out itself.

## What the algorithm is told and what it works out

`direction::automatic` lets the text decide: the first strong character — a letter, not a digit — sets the paragraph. `"123 שלום"` runs right to left, because a number is not strong and the Hebrew behind it is. A paragraph with no strong character at all runs left to right. A caller who knows better (a user interface with a language setting, a protocol that says so) passes `left_to_right` or `right_to_left` instead.

Everything else the algorithm works out on its own: the embedding and override characters, the isolates of Unicode 6.3, the weak types (a number after an Arabic letter is an Arabic number), the neutrals between two directions, and the brackets — `(` and `)` take one direction together, so a parenthesis does not flip away from what it encloses.

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
```

## What it is held to

`BidiCharacterTest.txt` of the UCD, which gives for every case the level the paragraph resolves to, the level of every character and the order they are drawn in. **All 91 707 of its cases pass**; the header the tests carry holds every third of them, 30 569, because the whole file is 7.8 MB of test data — four times everything else the tests carry — and the full set is run by hand before a change to this header lands (`python3 tools/unicode_tables.py --all-bidi`).

The tables are 6.9 KB: the class of every code point, 5.9, and the 128 bracket pairs, 1.0. The classes come from `DerivedBidiClass.txt` rather than from the assigned code points alone, because the file gives a class to the unassigned ones too — the ranges of Hebrew and Arabic run right to left before anything is put in them, and a text with a code point from a future version must still lay out sensibly.
