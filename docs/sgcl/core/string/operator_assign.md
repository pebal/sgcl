[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::operator=

```cpp
/*(1)*/ basic_string& operator=(const basic_string&) noexcept = default;
/*(2)*/ basic_string& operator=(basic_string&&) noexcept = default;
/*(3)*/ template<size_t N> basic_string& operator=(const CharT (&s)[N]);
/*(4)*/ template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        basic_string& operator=(P s);
/*(5)*/ basic_string& operator=(view_type s);
/*(6)*/ template<class V>
        requires std::is_convertible_v<const V&, view_type>
              && (!std::is_convertible_v<const V&, const CharT*>)
              && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
        basic_string& operator=(const V& v);
```

Replaces the string with another one. The object the string held before is not touched: other strings may hold it,
and when none does, the collector frees it.

1. Copies the word of the other string: the two are the same object.
2. Copies the word as well, as the move of a `tracked_ptr` does: the moved-from string keeps its value.
3. A new string of the array `s` (a literal), up to its first NUL or its end, whichever comes first.
4. A new string of the text `s` points at, up to its NUL; only for `CharT*` and `const CharT*`.
5. A new string of the characters of the view `s`.
6. A new string of anything a `view_type` is made of that is not a pointer: a `std::basic_string`, a slice of
   characters. A slice is made a string as the constructor from a slice makes it, so a slice that is the whole of a
   string gives that string's object again.

- (3–6) The new string is made as the [constructor](string.md) makes it, then its word is assigned.

`s = nullptr` does not compile, and neither does an assignment of bytes (a `vector<byte>`): a string is made of bytes
only explicitly, by its constructor.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the characters: an array, a pointer to a NUL-terminated text, a view |
| `v` | a value a view is made of: a `std::basic_string`, a slice |

## Return value

`*this`.

## Complexity

- (1–2) Constant.
- (3–6) Linear in the number of characters: one allocation, none for an empty text; constant for a slice that is
  the whole of a string.

## Exceptions

- (1–2) None.
- (3–6) `length_error` when the characters are more than [max_size()](max_size.md).

If an exception is thrown, the string is as it was: the new one is made before it is assigned.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string>

using namespace sgcl;

int main() {
    string name = "alice";
    string other;
    other = name;
    println("{} {}", other, other.object() == name.object());

    std::string built = "bob";
    other = built;
    println("{} {}", other, other.object() == name.object());

    string_slice whole = name;
    other = whole;
    println("{} {}", other, other.object() == name.object());

    other = "";
    println("{} {}", other.empty(), other.object() == nullptr);
}
```

Output:

```text
alice true
bob false
alice true
true true
```

## See also

- [(constructor)](string.md): constructs a string
- [swap](swap.md): swaps the words of two strings
- [sgcl::string](../string.md)
