[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::to_bytes

```cpp
vector<byte> to_bytes() const noexcept;        // (1)
vector<byte> to_bytes(size_t length) const;    // (2)
```

1. The magnitude as bytes, most significant first, as short as it goes, as Go's `Bytes`: no bytes for 0.
2. The same padded with zeros on the left to `length` bytes, as Go's `FillBytes`; a magnitude that takes more is an
   error (Go's `FillBytes` panics there).

- (1–2) The sign is not written: `-258` gives the bytes of `258`. The two's complement form of an INTEGER of ASN.1
  is the business of `encoding`. [from_bytes](from_bytes.md) reads the bytes back.

## Parameters

| Parameter | Description |
|---|---|
| `length` | the number of bytes to write |

## Return value

The bytes of `|a|`, most significant first: (1) `(bit_length() + 7) / 8` of them, (2) `length` of them.

## Complexity

Linear in the number of bytes written.

## Exceptions

- (1) None.
- (2) `length_error` when the magnitude takes more than `length` bytes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer a = 258;
    println("{} {} {}", a.to_bytes(), (-a).to_bytes(4), math::big_integer(0).to_bytes());
    math::big_integer big = math::big_integer(2).pow(100) - 1;
    println("{}", math::big_integer::from_bytes(big.to_bytes()) == big);
    try {
        a.to_bytes(1);
    } catch (const length_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
[1, 2] [0, 0, 1, 2] []
true
sgcl::math::big_integer::to_bytes: the value takes more bytes than given
```

## See also

- [from_bytes](from_bytes.md): the number of the bytes
- [bit_length](bit_length.md): the number of bits
- [sgcl::math::big_integer](../big_integer.md)
