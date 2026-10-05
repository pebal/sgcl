[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::distribution_point

```cpp
const vector<string>& distribution_point() const noexcept;
```

Returns the URIs of the full name of the list's issuingDistributionPoint (RFC 5280 §5.2.5): the distribution point it is the list of. A certificate whose own points are none of them is not covered by it ([status_of](status_of.md)).

## Parameters

None.

## Return value

The URIs; empty for a list without the extension, or with a point of no URI.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto crl = crypto::x509::revocation_list::parse(io::read_file(dir + "int.crl").value()).value();
    println("{}", crl.distribution_point().size());
}
```

Output:

```text
0
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
