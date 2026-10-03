[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::contains

```cpp
/*(1)*/ bool contains(std::basic_string_view<CharT> s) const noexcept;
/*(2)*/ bool contains(value_type c) const noexcept;
/*(3)*/ bool contains(char32_t c) const noexcept;
/*(4)*/ bool contains(std::same_as<int> auto) const = delete;
/*(5)*/ bool contains(const auto& value) const;
```

Checks whether the slice holds a piece of text or an element. The name is in two bases of a slice,
[mixin::text](../mixin/text.md) (a substring or a character) and [mixin::enumerable](../mixin/enumerable.md) (an
element), where it would be ambiguous, so the slice says which: the text's for text, the element's otherwise.

1. Whether the text holds the substring `s`.
2. Whether the text holds the code unit `c`.
3. Whether the text holds the code point `c`, encoded into its units: `contains(U'ż')`. Not for a slice of
   `char32_t`, whose (2) takes a code point.
4. An `int` is refused: `'ż'` in a UTF-8 source is an `int`, not a character; `U'ż'` is written instead.
5. Whether an element is equal to `value`, as `mixin::enumerable::contains` answers.

- (1–4) Take part only for a text slice, `slice<const CharT>`.
- (5) Takes part only for a slice of other elements, when they have `==`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the substring to look for |
| `c` | the character to look for |
| `value` | the value an element is compared with |

## Return value

`true` when the text or the elements hold it, `false` otherwise.

## Complexity

Linear in the size of the slice (times the size of `s` at worst for a substring).

## Exceptions

- (1–3) None.
- (5) What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "żółty kot";
    string_slice s = text;
    println("{} {} {}", s.contains("kot"), s.contains(' '), s.contains(U'ż'));

    vector v = {1, 2, 3};
    slice<int> numbers = v;
    println("{} {}", numbers.contains(2), numbers.contains(5));
}
```

Output:

```text
true true true
true false
```

## See also

- [mixin::text](../mixin/text.md): `find` and the other searches of a text
- [contains](../mixin/enumerable/contains.md): the question of `mixin::enumerable`
- [sgcl::slice\<T\>](../slice.md)
