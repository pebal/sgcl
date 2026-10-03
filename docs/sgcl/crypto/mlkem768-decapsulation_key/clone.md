[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](README.md)

# sgcl::crypto::mlkem768::decapsulation_key::clone

```cpp
decapsulation_key clone() const;
```

A second key of the same seed: the copy the class does not have, made by name so that a secret is never copied by
accident. The two keys are independent objects; each is zeroed when it goes.

## Parameters

None.

## Return value

A new key equal to this one.

## Complexity

Linear in the size of the object.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::generate();
    auto copy = key.clone();
    println("{}", copy == key);

    auto sent = key.encapsulation_key().encapsulate();
    println("{}", copy.decapsulate(sent.ciphertext) == sent.shared_key);
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](mlkem768-decapsulation_key.md): the move
- [seed](seed.md): the key's seed, to make it again later
- [sgcl::crypto::mlkem768::decapsulation_key](README.md)
