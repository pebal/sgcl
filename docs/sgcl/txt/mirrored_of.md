[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::mirrored_of

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ mirrored_of {};   // called as mirrored_of(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::mirrored_of(c)`, and passed as a projection. It takes a `char32_t` and refuses
everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the code point of the mirrored shape of `c`, a `char32_t`, by `BidiMirroring.txt`: `(` answers `)`,
   `≤` answers `≥`. Where the shape has no code point of its own, which is what the file leaves out and what a font
   draws by reflecting the glyph, the answer is `c` itself: an integral sign is
   [mirrored](is_mirrored.md) and has no second one to name it. A mirroring is its own inverse wherever the file
   gives one.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The mirrored code point, `char32_t`; `c` when it has none.

## Complexity

Constant: a test of the range the pairs lie in (U+0028 to U+FF63), then a binary search over the 428 pairs of
`BidiMirroring.txt`.

## Exceptions

None.

## Notes

- The 428 pairs are kept as `uint16` (both halves of every one of them are in the Basic Multilingual Plane), 2.1 KB
  with the 114 ranges of `Bidi_Mirrored`.
- `BidiMirroring.txt` is checked **whole**, all 428 of its lines as the file writes them and not as the table made
  from it has them: every mapping, that every mapping is its own inverse, that everything mapped is
  `Bidi_Mirrored`, and that no other code point of the 1 112 064 has a mirror.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'(', U'≤', U'«', U'∫'}) {
        print("{} -> {}   ", c, txt::mirrored_of(c));
    }
    println();
    println("{}", txt::mirrored_of(txt::mirrored_of(U'[')) == U'[');
}
```

Output:

```text
( -> )   ≤ -> ≥   « -> »   ∫ -> ∫   
true
```

## See also

- [is_mirrored](is_mirrored.md): whether a code point is drawn mirrored
- [mirrored](mirrored.md): rule L4 over a text
- [txt](README.md)
