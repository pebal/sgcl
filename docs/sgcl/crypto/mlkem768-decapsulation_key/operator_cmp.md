[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](README.md)

# sgcl::crypto::mlkem768::operator== (sgcl::crypto::mlkem768::decapsulation_key)

```cpp
friend bool operator==(const decapsulation_key& a, const decapsulation_key& b);
```

Compares two keys by their seeds, in constant time: the time does not depend on where the seeds differ. `!=` is its
negation, written by the language. A key moved from is compared with nothing: it is refused, as by every other
operation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys to compare |

## Return value

`true` when the keys have the same seed, `false` otherwise.

## Complexity

Constant: the 64 bytes of the seeds.

## Exceptions

`std::logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mlkem768::decapsulation_key::generate();
    auto same = crypto::mlkem768::decapsulation_key::from_seed(key.seed());
    println("{}", key == same);
    println("{}", key != crypto::mlkem768::decapsulation_key::generate());
}
```

Output:

```text
true
true
```

## See also

- [mlkem768::encapsulation_key: operator==](../mlkem768-encapsulation_key/operator_cmp.md): the public keys
- [constant_time](../constant_time/README.md): comparisons of secrets
- [sgcl::crypto::mlkem768::decapsulation_key](README.md)
