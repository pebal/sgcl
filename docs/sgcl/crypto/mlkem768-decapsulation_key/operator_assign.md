[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](README.md)

# sgcl::crypto::mlkem768::decapsulation_key::operator=

```cpp
decapsulation_key& operator=(decapsulation_key&& other) noexcept;    // (1)
decapsulation_key& operator=(const decapsulation_key&) = delete;     // (2)
```

1. Replaces the key with the key of `other` and zeroes `other` whole: the key this object held before is
   overwritten, and `other` holds no key after, so every operation on it throws `std::logic_error` until a key is
   assigned to it. An object moved from may be assigned to again. A key assigned to itself stays as it is.
2. There is no copy assignment: a second key is made by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

## Return value

`*this`.

## Complexity

Linear in the size of the object.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::generate();
    auto next = crypto::mlkem768::decapsulation_key::generate();
    auto sent = next.encapsulation_key().encapsulate();

    key = std::move(next);  // the old key is overwritten
    println("{}", key.decapsulate(sent.ciphertext) == sent.shared_key);

    next = key.clone();  // a key moved from takes a key again
    println("{}", next == key);
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](mlkem768-decapsulation_key.md): the move
- [clone](clone.md): a second key of the same seed
- [sgcl::crypto::mlkem768::decapsulation_key](README.md)
