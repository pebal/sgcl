[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::tagged

```cpp
static cbor tagged(uint64_t number, const cbor& content) noexcept;
```

A tag (§3.4): a number that says what the content means — 32 a URI, 37 a UUID, 55799 the self-description that
marks a file as CBOR. [date_time](date_time.md), [epoch_time](epoch_time.md), [decimal](decimal.md) and the
[constructor](cbor.md) of a big integer make the tags this type reads.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the tag's number |
| `content` | the value tagged |

## Return value

The value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor uri = encoding::cbor::tagged(32, "http://www.example.com");
    println("{} {}", uri.to_string(), uri.tag());
    println(*uri.content().as_string());
}
```

Output:

```text
32("http://www.example.com") 32
http://www.example.com
```

## See also

- [tag](tag.md), [content](content.md)
- [sgcl::encoding::cbor](README.md)
