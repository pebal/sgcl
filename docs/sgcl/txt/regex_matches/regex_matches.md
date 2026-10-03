[sgcl](../../README.md) › [txt](../README.md) › [regex_matches](../regex_matches.md)

# sgcl::txt::regex_matches::regex_matches

```cpp
regex_matches() noexcept = default;                                                  // (1)
regex_matches(const regex& re, const slice<const char>& text) noexcept;              // (2)
template<size_t N> regex_matches(const regex& re, const char (&text)[N]);            // (3)
template<class P> requires std::same_as<P, const char*> || std::same_as<P, char*>
regex_matches(const regex& re, P text);                                              // (4)
```

Constructs the range of the matches of a pattern in a text. [regex::all](../regex/all.md) is the usual way to
make one, and makes it the same way.

1. An empty range, of no pattern and no text.
2. The matches of `re` in the slice `text`; the range holds the slice and shares the compiled pattern.
3. An array of `char` up to its first NUL or its end, copied into a string the range holds: the slice of a
   literal's array would count its terminating zero.
4. The characters at a pointer up to their NUL, copied into a string the range holds.

## Parameters

| Parameter | Description |
|---|---|
| `re` | the pattern |
| `text` | the text, UTF-8 |

## Complexity

- (1–2) Constant: nothing is searched until the range is walked.
- (3–4) Linear in the length of the text, which is copied.

## Exceptions

- (1–2) None.
- (3–4) `length_error` when the text is longer than 4294967295 bytes, the most a [string](../../core/string.md)
  holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex digits("\\d+");
    txt::regex_matches none;
    txt::regex_matches some(digits, "rok 2026, dzień 275");
    println("{} {}", none.empty(), some.count());
}
```

Output:

```text
true 2
```

## See also

- [regex::all](../regex/all.md): the same range from the pattern
- [sgcl::txt::regex_matches](../regex_matches.md)
