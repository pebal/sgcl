[sgcl](../../README.md) › [txt](../README.md) › [bidi_runs](../bidi_runs.md)

# sgcl::txt::bidi_runs::bidi_runs

```cpp
bidi_runs() noexcept = default;                                                                 // (1)
explicit bidi_runs(const string& text, direction paragraph = direction::automatic) noexcept;    // (2)
template<size_t N>
explicit bidi_runs(const char (&text)[N], direction paragraph = direction::automatic);          // (3)
template<class P>
requires std::same_as<P, const char*> || std::same_as<P, char*>
explicit bidi_runs(P text, direction paragraph = direction::automatic);                         // (4)
explicit bidi_runs(const slice<const char>& text,                                               // (5)
                   direction paragraph = direction::automatic) noexcept;
```

Constructs the pieces of a text in the order they are drawn: the bidirectional algorithm of
[UAX #9](https://www.unicode.org/reports/tr9/) is run over the whole text, a paragraph at a time (rule P1), and the
pieces are cut and held. `txt::bidi_runs(s)` looks like a call and is a construction, as [runes](../../core/runes.md) is.

1. No pieces, over no text.
2. The pieces of a string; the range holds a slice of it, and with it the string's object.
3. The pieces of an array of `char`, a literal among them, read up to its first NUL or its end, never past it, and
   copied into a string the range holds.
4. The pieces of a C text, `const char*` or `char*`, read up to its NUL and copied likewise.
5. The pieces of a slice of UTF-8 bytes, a piece of a buffer as much as a piece of a string.

- (2–5) The direction of every paragraph is `paragraph`, or its first strong character's when it is
  `direction::automatic` ([direction](../direction.md)). An invalid byte of UTF-8 is one code point, `U+FFFD`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `paragraph` | the direction of every paragraph, or `direction::automatic` for each to decide |

## Complexity

Linear in the length of the text times the number of levels it reaches, two or three in a real text.

## Exceptions

- (1–2), (5) None.
- (3–4) `length_error` when the text is longer than the [max_size()](../../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::bidi_runs none;
    println("{}", none.empty());

    string line = "Nazwa: שלום abc";
    txt::bidi_runs automatic(line.as_slice(7));
    txt::bidi_runs ltr(line.as_slice(7), txt::direction::left_to_right);
    for (auto& runs : {automatic, ltr}) {
        for (auto piece : runs) {
            print("[{}] {}  ", piece.text, piece.level);
        }
        println();
    }
}
```

Output:

```text
true
[abc] 2  [שלום ] 1  
[שלום] 1  [ abc] 0  
```

## See also

- [paragraph](paragraph.md): the direction the paragraph resolved to
- [text](text.md): the slice the pieces are cut from
- [sgcl::txt::bidi_runs](../bidi_runs.md)
