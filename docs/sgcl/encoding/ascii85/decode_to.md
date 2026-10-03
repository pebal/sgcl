[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md)

# sgcl::encoding::ascii85::decode_to

```cpp
/*(1)*/ static expected<size_t, error> decode_to(const slice<byte>& out, const string& text);
/*(2)*/ static expected<size_t, error> decode_to(const slice<byte>& out,
                                                 const slice<const char>& text);
/*(3)*/ template<class T>
        static expected<size_t, error> decode_to(const slice<byte>& out, const T& text);
```

The bytes of a text written into the caller's buffer, nothing allocated: Go's `ascii85.Decode` of the whole text.
The text is read as [decode](decode.md) reads it. `out` holds at least the most this text decodes to — four
bytes for every `z`, four for every five of the other characters and one for each of the rest — which
[max_decoded_size](max_decoded_size.md)`(text.size())` always covers; a smaller one is a mistake in the program,
refused before anything is read.

1. The text of a string.
2. Characters read where they lie, no string made: a file's bytes, a secret's.
3. A literal, a character array or a `std::string_view`, read where it lies, a literal to its first `'\0'`; it
   takes part only for those.

A text found wrong has had the bytes before the error written into `out`. `out` does not overlap the text.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the bytes are written into |
| `text` | the text to decode |

## Return value

The number of bytes written, or the [error](../error.md) with its offset, as [decode](decode.md)'s.

## Complexity

Linear in the size of `text`.

## Exceptions

`length_error` when `out` is smaller than the most `text` decodes to.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 6> out = {};
    auto n = encoding::ascii85::decode_to(out, "87cURDZ");
    println("{} {}", *n, string(out.as_slice(0, *n)));

    try {
        array<byte, 5> small = {};
        encoding::ascii85::decode_to(small, "87cURDZ");
    } catch (const length_error& e) {
        println(e.what());
    }
}
```

Output:

```text
5 Hello
sgcl: the buffer is smaller than the most the text decodes to
```

## See also

- [max_decoded_size](max_decoded_size.md): a size that always serves
- [decode](decode.md): into a vector of its own
- [encode_to](encode_to.md): the other way
- [sgcl::encoding::ascii85](../ascii85.md)
