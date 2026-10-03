[sgcl](../../README.md) › [encoding](../README.md) › [pem](README.md)

# sgcl::encoding::pem::type

```cpp
const string& type() const noexcept;
```

The label of the block, what its `BEGIN` and `END` lines name: `"CERTIFICATE"`, `"PRIVATE KEY"`, `"X509 CRL"`. Go's
`Block.Type`. A program looking for one kind of block among others compares it.

## Parameters

None.

## Return value

The label; empty for a block whose lines name nothing (`-----BEGIN -----`), which RFC 7468 allows.

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
    string file = "-----BEGIN PRIVATE KEY-----\nMC4CAQA=\n-----END PRIVATE KEY-----\n"
                  "-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----\n";
    auto blocks = encoding::pem::parse_all(file);
    for (const auto& block : blocks.value()) {
        if (block.type() == "CERTIFICATE") {
            println("a certificate of {} bytes", block.bytes().size());
        }
    }
}
```

Output:

```text
a certificate of 3 bytes
```

## See also

- [bytes](bytes.md): what the block holds
- [sgcl::encoding::pem](README.md)
