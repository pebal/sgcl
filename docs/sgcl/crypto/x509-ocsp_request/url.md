[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [ocsp_request](README.md)

# sgcl::crypto::x509::ocsp_request::url

```cpp
string url(const string& responder) const;
```

Returns the GET of the request (RFC 6960 Appendix A.1): the responder's URL, a `/` when it does not end with one,
and the base64 of the request's DER with `+`, `/` and `=` escaped. A GET of a short request can be cached on the
way (RFC 5019 §5 has it for requests of up to 255 bytes); a longer one is a POST of [raw](raw.md).

## Parameters

| Parameter | Description |
|---|---|
| `responder` | the responder's URL, one of the certificate's [ocsp_servers](../x509-certificate/ocsp_servers.md) |

## Return value

The URL of the GET.

## Complexity

Linear in the size of the request.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = "tests/crypto/data/revocation/";
    auto leaf = crypto::x509::certificate::from_pem(io::read_text(dir + "good.pem")).value();
    auto issuer = crypto::x509::certificate::from_pem(io::read_text(dir + "int.pem")).value();
    auto request = crypto::x509::ocsp_request::make(leaf, issuer).value();
    println("{}", request.url(leaf.ocsp_servers()[0]));
}
```

Output:

```text
http://127.0.0.1:47811/MEMwQTA%2FMD0wOzAJBgUrDgMCGgUABBTzcZnxDtmD2Z8IJyr169PtS40KjAQUBz9j0XzHAoyEQVP%2FGrxM3sz0I%2BACAiAB
```

## See also

- [raw](raw.md): the body of a POST
- [sgcl::crypto::x509::ocsp_request](README.md)
