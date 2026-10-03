[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](../mlkem768-decapsulation_key.md)

# sgcl::crypto::mlkem768::decapsulation_key::generate

```cpp
static decapsulation_key generate() noexcept;
```

A new key, Go's `GenerateKey768`: a seed d‖z of 64 bytes from [crypto::random](../random.md), and the key FIPS 203's
ML-KEM.KeyGen_internal makes of it. `mlkem512::decapsulation_key::generate` and
`mlkem1024::decapsulation_key::generate` make keys of their sets the same way.

## Parameters

None.

## Return value

The new key.

## Complexity

Constant: the key generation of FIPS 203, the matrix sampled once.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = crypto::mlkem768::decapsulation_key::generate();
    auto b = crypto::mlkem768::decapsulation_key::generate();
    println("{}", a == b);
    println("{} bytes to publish", a.encapsulation_key().bytes().size());
}
```

Output:

```text
false
1184 bytes to publish
```

## See also

- [from_seed](from_seed.md): the key of a known seed
- [sgcl::crypto::mlkem768::decapsulation_key](../mlkem768-decapsulation_key.md)
