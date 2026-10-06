[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [recipient](README.md)

# sgcl::crypto::hpke::recipient::suite

```cpp
hpke::suite suite() const noexcept;
```

The suite the context was made with.

## Parameters

None.

## Return value

The suite.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app");
    println("{}", r->suite() == s->suite());
}
```

Output:

```text
true
```

## See also

- [suite](../hpke-suite.md)
- [sgcl::crypto::hpke::recipient](README.md)
