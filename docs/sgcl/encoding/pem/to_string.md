[sgcl](../../README.md) › [encoding](../README.md) › [pem](README.md)

# sgcl::encoding::pem::to_string

```cpp
string to_string() const;
```

The block as text, in the strict form of section 3 of RFC 7468: the `BEGIN` line, the headers — `Proc-Type` first,
as RFC 1421 wants it, and the others in their order — and an empty line after them when there are any, the bytes
in base64 in lines of 64 characters, and the `END` line; `"\n"` at the end of every line. It is what Go's
`EncodeToMemory` writes for the same block, but for the order of the headers after `Proc-Type`, which Go sorts by
name. The text reads back with [parse](parse.md) as the same type, bytes and headers, `Proc-Type` first among them.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the number of bytes and the length of the headers.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, string> headers;
    headers.insert_or_assign("DEK-Info", "AES-128-CBC,00FF00FF");
    headers.insert_or_assign("Proc-Type", "4,ENCRYPTED");
    encoding::pem key("RSA PRIVATE KEY", vector<byte>(50), headers);
    string text = key.to_string();
    print("{}", text);
    println("{}", encoding::pem::parse(text)->bytes() == key.bytes());
}
```

Output:

```text
-----BEGIN RSA PRIVATE KEY-----
Proc-Type: 4,ENCRYPTED
DEK-Info: AES-128-CBC,00FF00FF

AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAA=
-----END RSA PRIVATE KEY-----
true
```

## See also

- [parse](parse.md): the text read back
- [(constructor)](pem.md): a block of its parts
- [sgcl::encoding::pem](README.md)
