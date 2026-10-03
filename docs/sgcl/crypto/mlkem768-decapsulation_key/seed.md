[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](../mlkem768-decapsulation_key.md)

# sgcl::crypto::mlkem768::decapsulation_key::seed

```cpp
secret<64> seed() const;
```

The seed d‖z the key is kept as (FIPS 203 §7.1), Go's `Bytes`: the one form of a decapsulation key, from which
[from_seed](from_seed.md) makes the same key again. The seed is 64 bytes in every parameter set.

## Parameters

None.

## Return value

The seed, as a [secret\<64\>](../secret.md): zeroed when it goes, never in managed memory.

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::generate();
    auto seed = key.seed();
    println("{} bytes", seed.size);

    auto again = crypto::mlkem768::decapsulation_key::from_seed(seed);
    println("{}", again == key);
}
```

Output:

```text
64 bytes
true
```

## See also

- [from_seed](from_seed.md): the key of a seed
- [sgcl::crypto::mlkem768::decapsulation_key](../mlkem768-decapsulation_key.md)
