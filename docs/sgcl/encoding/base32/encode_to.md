[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md)

# sgcl::encoding::base32::encode_to

```cpp
size_t encode_to(const slice<char>& out, const slice<const byte>& data) const;
```

The text of `data` written into the caller's buffer, nothing allocated: Go's `Encode`. `out` holds at least
[encoded_size](encoded_size.md)`(data.size())` characters; a smaller one is a mistake in the program, refused
before anything is written. The characters past the text are left as they were. `out` does not
overlap `data`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the text is written into |
| `data` | the bytes to encode |

## Return value

The number of characters written, `encoded_size(data.size())`.

## Complexity

Linear in the size of `data`.

## Exceptions

`length_error` when `out` is smaller than `encoded_size(data.size())`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 5> data = {byte('h'), byte('e'), byte('l'), byte('l'), byte('o')};
    array<char, 8> text = {};
    size_t n = encoding::base32::standard.encode_to(text, data);
    println("{} {}", n, string(text.data(), n));

    try {
        array<char, 7> small = {};
        encoding::base32::standard.encode_to(small, data);
    } catch (const length_error& e) {
        println(e.what());
    }
}
```

Output:

```text
8 NBSWY3DP
sgcl: the buffer is smaller than the encoding
```

## See also

- [encoded_size](encoded_size.md): the size the buffer needs
- [encode](encode.md): into a string of its own
- [decode_to](decode_to.md): the other way
- [sgcl::encoding::base32](../base32.md)
