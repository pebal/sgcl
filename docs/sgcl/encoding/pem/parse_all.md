[sgcl](../../README.md) › [encoding](../README.md) › [pem](README.md)

# sgcl::encoding::pem::parse_all

```cpp
static expected<vector<pem>, error> parse_all(const string& text) noexcept;
```

Every block of a text, in its order — a chain of certificates, a bundle of trusted roots — as [parse](parse.md)
reads each: the text before, between and after the blocks is skipped, and a malformed block is an error for the
whole text, never a block passed over. Go reads a chain by calling `pem.Decode` on the rest until it returns no
block.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text with the blocks |

## Return value

The blocks, empty for a text with none; or the [error](../error/README.md) of the first malformed block, with its line and
its column, as `parse` reports it.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string bundle = "# first\n-----BEGIN A-----\nQUJD\n-----END A-----\n"
                    "# second\n-----BEGIN B-----\nREVG\n-----END B-----\n";
    auto blocks = encoding::pem::parse_all(bundle);
    for (const auto& block : blocks.value()) {
        println("{} {}", block.type(), block.bytes().size());
    }
    println("{}", encoding::pem::parse_all("no blocks\n")->size());
    println("{}", encoding::pem::parse_all(bundle + "-----BEGIN C-----\n").error().message());
}
```

Output:

```text
A 3
B 3
0
10:1: no END line for BEGIN C
```

## See also

- [parse](parse.md): the first block
- [sgcl::encoding::pem](README.md)
