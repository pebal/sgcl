[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::error

```cpp
#include "sgcl/crypto/error.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class error;
}
```

`sgcl::crypto::error` is the error of the whole module: what went wrong in data it was given, in the
[expected](../core/expected.md) that a function taking such data returns — `open` of an AEAD, a key or a signature
read from bytes, DER or PEM, a certificate chain verified. It holds a code of [errc](errc.md), the byte of the input
where the error was found when the input is an encoding, why a chain does not verify, and a text that says more
than the code's own words when there is more to say. Where Go returns a plain `error` whose kind is told by its
type or by comparing with a sentinel, here the kind is the [code](error/code.md), read by a `switch`.

A broken contract of the program is not an error of this kind: a key of the wrong length written into the program,
a nonce of the wrong size, more output than an algorithm gives (`hkdf::expand` past 255 blocks), `update` of a SHAKE
after a read, a `hash_id` outside the list, a key used after being moved from — each is an exception,
`std::invalid_argument` or `std::logic_error`, thrown, as in the hash and compress modules. No random bytes from the
system is neither: [random](random.md) ends the program.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- A value: copied, compared, held in an `expected`. It holds a [string](../core/string.md), so it lives where a
  string does: on a stack, in a managed object, in a container of the library.
- What an error says is not secret: a failed `open` of an AEAD is `errc::authentication` and nothing more, so that
  it tells nothing of which byte was wrong.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error/error.md) | constructs an error of a code, with an offset, a text, or the reason a chain does not verify |

#### Observers

| Function | Description |
|---|---|
| [code](error/code.md) | the code |
| [offset](error/offset.md) | the byte of the encoded input where the error was found |
| [reason](error/reason.md) | why a certificate chain does not verify |
| [message](error/message.md) | the error as a text |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](error/operator_cmp.md) | compares the codes, the reasons, the offsets and the texts |
| [make_error_code](make_error_code.md) | a code of [errc](errc.md) as a `std::error_code` |
| [crypto_category](crypto_category.md) | the `std::error_category` of the module, `"crypto"` |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto short_key = crypto::x25519::public_key::from_bytes(encoding::hex::decode("00ff"));
    println("{}", short_key.error().message());

    // an Ed25519 key where an X25519 key is expected (RFC 8032's TEST 1)
    auto ed = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    auto wrong = crypto::x25519::public_key::from_pkix_der(ed->public_key().to_pkix_der());
    crypto::error e = wrong.error();
    println("{} {}", e.code() == crypto::errc::unsupported, e.offset());
    println("{}", e.message());
}
```

Output:

```text
an X25519 public key is 32 bytes
true 6
offset 6: the key is of another algorithm
```

## See also

- [errc](errc.md): the codes
- [x509::reason](x509-reason.md): why a chain does not verify
- [expected](../core/expected.md): the result that holds a value or the error
