[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [recipient](README.md)

# sgcl::crypto::hpke::recipient::recipient

```cpp
recipient(recipient&& other) noexcept;
```

The context of `other`, moved in, its count of messages with it; `other` is left zeroed, and its calls are
`std::logic_error`. Contexts are made by [setup](setup.md).

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
    auto s = crypto::hpke::sender::setup(key.public_key(), {});
    auto made = crypto::hpke::recipient::setup(s->enc(), key, {});
    crypto::hpke::recipient r(std::move(made.value()));
    println("{}", string(r.open(s->seal("moved")).value()));
}
```

Output:

```text
moved
```

## See also

- [setup](setup.md)
- [sgcl::crypto::hpke::recipient](README.md)
