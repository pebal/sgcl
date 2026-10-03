[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](README.md)

# sgcl::crypto::mlkem768::decapsulation_key::decapsulation_key

```cpp
decapsulation_key(decapsulation_key&& other) noexcept;    // (1)
decapsulation_key(const decapsulation_key&) = delete;     // (2)
```

1. Takes the key of `other` over: the seed, the expanded key and the matrix are copied into the new object, and
   `other` is zeroed whole. `other` holds no key after: every operation on it throws `std::logic_error` until a key
   is assigned to it.
2. There is no copy: a second key of the same seed is made by name, [clone](clone.md).

A key is made by [generate](generate.md) or [from_seed](from_seed.md); there is no other public constructor.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

## Complexity

Linear in the size of the object (7 KB for ML-KEM-768, 3.7 KB for 512, 11 KB for 1024): the key lives in the
object, so a move copies it and zeroes the source.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto first = crypto::mlkem768::decapsulation_key::generate();
    auto sent = first.encapsulation_key().encapsulate();

    crypto::mlkem768::decapsulation_key second = std::move(first);
    println("{}", second.decapsulate(sent.ciphertext) == sent.shared_key);
    try {
        first.decapsulate(sent.ciphertext);
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::mlkem768: used after being moved from
```

## See also

- [operator=](operator_assign.md): the move assignment
- [clone](clone.md): a second key of the same seed
- [sgcl::crypto::mlkem768::decapsulation_key](README.md)
