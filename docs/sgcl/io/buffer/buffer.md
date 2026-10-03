[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::buffer

```cpp
buffer() noexcept;                                             // (1)
buffer(const buffer&) noexcept = default;                      // (2)
buffer(buffer&&) noexcept = default;                           // (3)
explicit buffer(const slice<const byte>& initial) noexcept;    // (4)
explicit buffer(const string& initial) noexcept;               // (5)
template<class T>
explicit buffer(const T& initial) noexcept;                    // (6)
```

Constructs a buffer. Every constructor but the copy and the move makes the state at once, so that a copy made
later always shares it.

1. An empty buffer, its state made.
2. The same buffer as the other handle: the word copied, the bytes shared.
3. The same, the word moved: a move of a tracked word is a copy, so the handle moved from is still the same buffer.
4. A buffer holding a copy of the bytes of `initial`; a `vector<byte>` or another slice converts to it.
5. A buffer holding a copy of the characters of `initial`.
6. A buffer holding a copy of the characters of `initial`, a literal or a character array (read to its first NUL
   or its end, whichever comes first) or a `std::string_view`. Takes part only for those, an exact match where the
   conversions to a string and to bytes would tie.

The write position of (4)–(6) is at the end: a write appends.

## Parameters

| Parameter | Description |
|---|---|
| `initial` | the bytes or the text the buffer starts with |

The copy and the move (2–3) take the other handle, unnamed.

## Complexity

- (1)–(3) Constant.
- (4)–(6) Linear in the size of `initial`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer empty;
    io::buffer from_text("text");
    io::buffer from_bytes(vector<byte>{byte('b'), byte('y')});
    io::buffer from_view(std::string_view("a view"));
    io::buffer same = from_text;
    same.write("!");
    println("{} [{}] [{}]", empty.size(), from_text.text(), from_bytes.text());
    println("[{}]", from_view.text());
}
```

Output:

```text
0 [text!] [by]
[a view]
```

## See also

- [operator=](operator_assign.md)
- [release](release.md): the bytes out of a buffer
- [sgcl::io::buffer](README.md)
