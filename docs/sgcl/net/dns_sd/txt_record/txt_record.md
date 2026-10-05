[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::txt_record

```cpp
txt_record() noexcept;                                            // (1)
txt_record(std::initializer_list<pair<string, string>> pairs);    // (2)
txt_record(const txt_record& other);                              // (3), implicitly declared
txt_record(txt_record&& other) noexcept;                          // (4), implicitly declared
```

1. An empty record.
2. A record of the pairs, each a key and its value, set in their order as [set](set.md) sets them: a key given twice
   keeps its first place and its last value.
3. A copy: the entries copied, the strings shared, as a copy of a [vector](../../../core/vector/README.md) of strings is.
4. A move: `other` left empty.

## Parameters

| Parameter | Description |
|---|---|
| `pairs` | the keys and their values |
| `other` | the record copied or moved |

## Complexity

- (1, 4) Constant.
- (2) Quadratic in the pairs: each key looked for among those before it.
- (3) Linear in the entries.

## Exceptions

- (2) `invalid_argument` as [set](set.md) throws it: a key that is empty, holds `=` or a byte outside `0x20`–`0x7E`, an
  entry past 255 bytes.
- (1, 3, 4) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

#include <stdexcept>

using namespace sgcl;

int main() {
    net::dns_sd::txt_record none;
    net::dns_sd::txt_record txt = {{"a", "1"}, {"b", "2"}, {"A", "3"}};
    println("{} {}", none.size(), txt.size());
    println("{}", txt.get("a").value());
    try {
        net::dns_sd::txt_record bad = {{"a=b", "1"}};
    } catch (const std::invalid_argument& e) {
        println("refused");
    }
}
```

Output:

```text
0 2
3
refused
```

## See also

- [set](set.md): one key at a time
- [sgcl::net::dns_sd::txt_record](README.md)
