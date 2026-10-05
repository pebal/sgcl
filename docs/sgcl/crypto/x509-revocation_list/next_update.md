[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [revocation_list](README.md)

# sgcl::crypto::x509::revocation_list::next_update

```cpp
optional<time::datetime> next_update() const noexcept;
```

Returns when the next list is due, its nextUpdate, in UTC: a list past it is no longer current ([status_of](status_of.md)), and a cache keeps the list until then.

## Parameters

None.

## Return value

The time, or `nullopt` for a list that does not say.

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
    println("{}", crl.next_update().has_value());
}
```

Output:

```text
true
```

## See also

- [parse](parse.md): the list read
- [sgcl::crypto::x509::revocation_list](README.md)
