[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::push_back

```cpp
void push_back(const jwk& key);
```

Adds a key at the end of the set.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

None.

## Complexity

Amortized constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::jose::jwk_set set;
    set.push_back(crypto::jose::jwk::generate(crypto::jose::algorithm::hs256));
    println("{}", set.size());
}
```

Output:

```text
1
```

## See also

- [(constructor)](jose-jwk_set.md)
- [sgcl::crypto::jose::jwk_set](README.md)
