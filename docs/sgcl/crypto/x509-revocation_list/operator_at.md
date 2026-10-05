[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::operator[]

```cpp
revoked_certificate operator[](size_t index) const;
```

Returns the entry at the index, in the list's order, as a value made of the list's bytes ([revoked_certificate](../x509-revoked_certificate.md)): its serial number, when it was revoked, why, and since when its key is suspect when the list says so.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the entry's place, below [size](size.md) |

## Return value

The entry.

## Complexity

Constant.

## Exceptions

`std::out_of_range` for an index past the end: the index is the program's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    for (int i : range(crl.size())) {
        auto e = crl[i];
        println("{} {}", encoding::hex::encode(e.serial_number), int(e.reason));
    }
}
```

Output:

```text
2002 1
2003 6
```

## See also

- [lookup](lookup.md): the entry of a serial number
- [sgcl::crypto::x509::revocation_list](README.md)
