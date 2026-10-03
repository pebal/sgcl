[sgcl](../../README.md) › [crypto](../README.md) › [aes](../aes.md)

# sgcl::crypto::aes::clone

```cpp
aes clone() const;
```

A second object with the same key schedule: a copy of a secret, made on purpose and by name, where a copy
constructor would make one by accident. The two encrypt alike and are destroyed, and zeroed, each on its own.

## Parameters

None.

## Return value

An object with the key schedule of this one.

## Complexity

Constant.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes cipher(vector<byte>(24));
    crypto::aes copy = cipher.clone();
    array<byte, 16> block = {};
    println("{}", cipher.encrypt_block(block) == copy.encrypt_block(block));

    crypto::aes taken = std::move(cipher);
    try {
        crypto::aes again = cipher.clone();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::aes: used after being moved from
```

## See also

- [(constructor)](aes.md): sets up a key schedule, or takes another object's over
- [sgcl::crypto::aes](../aes.md)
