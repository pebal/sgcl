[sgcl](../../README.md) › [encoding](../README.md) › [pem](../pem.md)

# sgcl::encoding::pem::headers

```cpp
const ordered_map<string, string>& headers() const noexcept;
```

The headers of RFC 1421 between the `BEGIN` line and the base64, `Proc-Type: 4,ENCRYPTED` and `DEK-Info` of an
older encrypted key, by name, in the order of the text; a name given twice keeps its last value. Go's
`Block.Headers`, which is a map without an order.

A block's lines are headers when the first line after `BEGIN` has a colon. When they end in an empty line, as RFC
1421 ends them, a line that starts with white space goes on with the value before it (RFC 822's folding); when they
do not, they are read as Go reads them: the headers are the lines with a colon, and the first line without one,
indented or not, is the start of the base64. A name and a value are trimmed of white space at both ends.

## Parameters

None.

## Return value

The headers, empty for a block without them.

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
    auto key = encoding::pem::parse("-----BEGIN RSA PRIVATE KEY-----\n"
                                    "Proc-Type: 4,ENCRYPTED\n"
                                    "DEK-Info: AES-128-CBC,\n"
                                    "  00FF00FF\n"
                                    "\n"
                                    "QUJD\n"
                                    "-----END RSA PRIVATE KEY-----\n");
    for (const auto& [name, value] : key->headers()) {
        println("{} = {}", name, value);
    }
}
```

Output:

```text
Proc-Type = 4,ENCRYPTED
DEK-Info = AES-128-CBC,  00FF00FF
```

## See also

- [to_string](to_string.md): the headers written back, `Proc-Type` first
- [ordered_map](../../core/ordered_map.md): a map in the order of insertion
- [sgcl::encoding::pem](../pem.md)
