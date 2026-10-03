[sgcl](../README.md) › [core](README.md)

# sgcl::runes

```cpp
#include "sgcl/core/slice.h"   // or "sgcl/core.h"

namespace sgcl {
    class runes;
}
```

`sgcl::runes` is the code points of a UTF-8 text, decoded as they are walked: what `runes()` of a
[string](string.md) or of a text [slice](slice.md) returns, and what Go's `for i, r := range s` walks. It is a forward
range of `char32_t` over a slice of the text, which holds the text's object for as long as the range lives, so
`for (char32_t c : string("żółw").runes())` walks a temporary string safely. Nothing is stored: each code point is
decoded when the walk reaches it, and an invalid byte is one code point, `utf8::replacement`.

The [iterator](runes-iterator.md) knows the byte position of its code point and the bytes it takes, for the code that
goes back to the bytes. The range is a range of the library ([mixin::enumerable](mixin/enumerable.md)): `contains`,
`count_of`, `exists`, `for_each` and the other questions are asked of the code points, and a
`const req::enumerable auto&` parameter takes it.

## Rules

- A `runes` holds a slice, so it lives where a `tracked_ptr` may: on a stack or inside a managed object
  ([The rules](README.md#the-rules), 1).
- The text is read as it is when the walk reaches it: a range over a string reads an immutable text; a range over a
  slice of a buffer reads what the buffer holds at that moment, as the slice does.
- An iterator refers to the text, not to the range: it is valid while the text's object lives, which the range and
  every slice of the text keep.

## Member types

| Type | Definition |
|---|---|
| `value_type` | `char32_t` |
| `size_type` | `size_t` |
| [iterator](runes-iterator.md) | a forward iterator whose `*` is a code point, `char32_t`, with its byte position and its width |
| `const_iterator` | `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](runes/runes.md) | constructs the range over a text |
| `(destructor)` | drops the slice of the text |

#### Iterators

| Function | Description |
|---|---|
| [begin](runes/begin.md) | an iterator to the first code point |
| [end](runes/end.md) | the iterator past the last code point |

#### Capacity

| Function | Description |
|---|---|
| [empty](runes/empty.md) | checks whether the text has no bytes |
| [count](runes/count.md) | the number of code points, walked |

#### Observers

| Function | Description |
|---|---|
| [text](runes/text.md) | the slice of the text the range walks |

#### From mixin::enumerable

The questions asked of the code points, carried by every range of the library
([mixin::enumerable](mixin/enumerable.md)).

| Function | Description |
|---|---|
| [contains](mixin/enumerable/contains.md) | checks whether the text has a code point |
| `index_of` | the position, in code points, of the first one equal to a value |
| `last_index_of` | the position, in code points, of the last one equal to a value |
| `find_index` | the position, in code points, of the first one the predicate accepts |
| `find_if` | the first code point the predicate accepts, in an `optional`: the iterator gives values, not elements |
| `exists` | checks whether the predicate accepts some code point |
| `all` | checks whether the predicate accepts every code point |
| `count_of` | the number of code points the predicate accepts |
| `min`, `max` | the smallest, the largest code point |
| `for_each` | calls a function with every code point |

## Complexity

A step of the iterator decodes one code point: constant. `count`, and every question of
[mixin::enumerable](mixin/enumerable.md), walks the text: linear in its bytes.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "Żółw 😀";
    for (auto it = s.runes().begin(); it != s.runes().end(); ++it) {
        print("{}:{:x}{}", it.pos(), uint32_t(*it), it.pos() + it.width() < s.size() ? " " : "\n");
    }

    runes letters = s.runes();
    println("{} code points in {} bytes", letters.count(), s.size());
    println("{} upper, emoji {}", letters.count_of(unicode::is_upper),
            letters.exists([](char32_t c) { return c >= 0x1F600; }));
}
```

Output:

```text
0:17b 2:f3 4:142 6:77 7:20 8:1f600
6 code points in 12 bytes
1 upper, emoji true
```

## See also

- [utf8](utf8.md): the decoding a step does
- [unicode](unicode.md): the questions asked of a code point
- [string](string.md), [slice](slice.md), [mixin::text](mixin/text.md): `runes()`, `rune_count()` and `decode(pos)`
  of a text
