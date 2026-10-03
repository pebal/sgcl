[sgcl](../../README.md) › [encoding](../README.md) › [base32](README.md)

# sgcl::encoding::base32::decode_to

```cpp
expected<size_t, error> decode_to(const slice<byte>& out, const string& text) const;    // (1)
expected<size_t, error> decode_to(const slice<byte>& out,                               // (2)
                                  const slice<const char>& text) const;
template<class T>
expected<size_t, error> decode_to(const slice<byte>& out, const T& text) const;         // (3)
```

The bytes of a text written into the caller's buffer, nothing allocated: Go's `Decode`. The text is read as
[decode](decode.md) reads it, strict or lenient as the codec is. `out` holds at least
[max_decoded_size](max_decoded_size.md)`(text.size())` bytes; a smaller one is a mistake in the program, refused
before anything is read.

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

The number of bytes written, or the [error](../error/README.md) with its offset, as [decode](decode.md)'s.

## Complexity

Linear in the size of `text`.

## Exceptions

`length_error` when `out` is smaller than `max_decoded_size(text.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 10> out = {};
    auto n = encoding::base32::standard.decode_to(out, "NBSWY3DPEE======");
    println("{} {}", *n, string(out.as_slice(0, *n)));

    try {
        array<byte, 5> small = {};
        encoding::base32::standard.decode_to(small, "NBSWY3DPEE======");
    } catch (const length_error& e) {
        println(e.what());
    }
}
```

Output:

```text
6 hello!
sgcl: the buffer is smaller than the most the text decodes to
```

## See also

- [max_decoded_size](max_decoded_size.md): the size the buffer needs
- [decode](decode.md): into a vector of its own
- [encode_to](encode_to.md): the other way
- [sgcl::encoding::base32](README.md)
