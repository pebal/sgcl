[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::from

```cpp
static dotenv from(std::initializer_list<member> members) noexcept;    // (1)
static dotenv from(const vector<member>& members) noexcept;            // (2)
```

The entries in their order; a key given twice keeps its first place and its last value, as in a file.

1. Of a list written out, `{key, value}` each.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `members` | the keys and their values |

## Return value

The entries.

## Complexity

Linear in the count of the entries, and in it again for each key given twice.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv env = encoding::dotenv::from({{"HOST", "example.com"}, {"PORT", "8080"}, {"HOST", "localhost"}});
    print(env.to_string());
}
```

Output:

```text
HOST=localhost
PORT=8080
```

## See also

- [set](set.md)
- [sgcl::encoding::dotenv](README.md)
