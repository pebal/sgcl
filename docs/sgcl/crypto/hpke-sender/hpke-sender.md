[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [sender](README.md)

# sgcl::crypto::hpke::sender::sender

```cpp
sender(sender&& other) noexcept;
```

The context of `other`, moved in, its messages counted on from where `other` was; `other` is left zeroed, and its
calls are `std::logic_error`. A context is not copied: two contexts sealing under one key and one nonce would give away
the messages. Contexts are made by [setup](setup.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the context moved from |

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
    auto made = crypto::hpke::sender::setup(key.public_key(), {});
    crypto::hpke::sender s(std::move(made.value()));
    println("{}", s.enc().size());
}
```

Output:

```text
32
```

## See also

- [setup](setup.md)
- [sgcl::crypto::hpke::sender](README.md)
