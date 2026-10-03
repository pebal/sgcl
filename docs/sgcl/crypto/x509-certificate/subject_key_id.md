[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate](../x509-certificate.md)

# sgcl::crypto::x509::certificate::subject_key_id

```cpp
const vector<byte>& subject_key_id() const noexcept;
```

Returns the subjectKeyIdentifier: an identifier of the certificate's key, most often the SHA-1 of its bits, which the
certificates it issues name in their [authority_key_id](authority_key_id.md).

## Parameters

None.

## Return value

The identifier, empty when there is none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto root_text = io::read_text("tests/net/tls_testdata/ca.pem");  // the tree's test CA
    crypto::x509::certificate root = crypto::x509::certificate::from_pem(root_text);

    println("{}", encoding::hex::encode(root.subject_key_id()));
}
```

Output:

```text
a09a230dac6877136ec4c3e8460c0b7e13b85f11
```

## See also

- [authority_key_id](authority_key_id.md)
- [sgcl::crypto::x509::certificate](../x509-certificate.md)
