[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::alert

```cpp
#include "sgcl/net/tls/error.h"   // or "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class alert : uint8_t {
        close_notify = 0,
        unexpected_message = 10,
        bad_record_mac = 20,
        record_overflow = 22,
        handshake_failure = 40,
        bad_certificate = 42,
        unsupported_certificate = 43,
        certificate_revoked = 44,
        certificate_expired = 45,
        certificate_unknown = 46,
        illegal_parameter = 47,
        unknown_ca = 48,
        access_denied = 49,
        decode_error = 50,
        decrypt_error = 51,
        protocol_version = 70,
        insufficient_security = 71,
        internal_error = 80,
        inappropriate_fallback = 86,
        user_canceled = 90,
        missing_extension = 109,
        unsupported_extension = 110,
        unrecognized_name = 112,
        bad_certificate_status_response = 113,
        unknown_psk_identity = 115,
        certificate_required = 116,
        no_application_protocol = 120,
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::tls::alert> : std::true_type {};
```

The alerts of TLS 1.3 (RFC 8446 §6.2), by their numbers on the wire. An alert this side sent ends a connection
with an [io::error](../../io/error.md) of the category `"tls"` ([category](category.md)) whose code is the alert's
number: `std::is_error_code_enum` is specialized, so an `alert` converts to an `error_code`
([make_error_code](make_error_code.md)) and `e.code() == net::tls::alert::decode_error` asks for it. An alert the
peer sent has a code of its own in the category, and [alert_of](alert_of.md) gives the alert of either, with
[is_remote](is_remote.md) to tell them apart. The text of each is Go's, after `tls: ` for this side's and
`remote error: tls: ` for the peer's.

| Value | Description |
|---|---|
| `close_notify` | "close notify": the sender sends nothing more; the end of the stream, not a failure |
| `unexpected_message` | "unexpected message": a message where another was due |
| `bad_record_mac` | "bad record MAC": a record that does not decrypt |
| `record_overflow` | "record overflow": a record longer than the limit of its kind |
| `handshake_failure` | "handshake failure": no cipher suite, group or signature scheme both sides have |
| `bad_certificate` | "bad certificate": a certificate broken, or whose signature does not verify |
| `unsupported_certificate` | "unsupported certificate": a certificate of a type not supported |
| `certificate_revoked` | "revoked certificate": a certificate its signer revoked |
| `certificate_expired` | "expired certificate": a certificate out of its validity |
| `certificate_unknown` | "unknown certificate": a certificate refused for another reason |
| `illegal_parameter` | "illegal parameter": a field out of its range or at odds with another |
| `unknown_ca` | "unknown certificate authority": a chain that leads to no trusted root |
| `access_denied` | "access denied": a valid certificate the peer does not let through |
| `decode_error` | "error decoding message": a message that does not parse |
| `decrypt_error` | "error decrypting message": a signature or a `Finished` that does not verify |
| `protocol_version` | "protocol version not supported": a peer without TLS 1.3 |
| `insufficient_security` | "insufficient security level": parameters weaker than the peer accepts |
| `internal_error` | "internal error": a failure of the sender's own, not of the protocol |
| `inappropriate_fallback` | "inappropriate fallback": a retry at a lower version the server refuses |
| `user_canceled` | "user canceled": the handshake abandoned by the sender |
| `missing_extension` | "missing extension": an extension the message must carry is not there |
| `unsupported_extension` | "unsupported extension": an extension where the message may not carry it |
| `unrecognized_name` | "unrecognized name": no server of the name sent (SNI) |
| `bad_certificate_status_response` | "bad certificate status response": an OCSP response that is not valid |
| `unknown_psk_identity` | "unknown PSK identity": a pre-shared key the server does not know |
| `certificate_required` | "certificate required": a client certificate the server asked for and did not get |
| `no_application_protocol` | "no application protocol": no ALPN protocol both sides have |

## Example

The server refuses a client whose protocols it does not speak; its error is the alert it sent:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

async::task<> serve(net::connection transport, net::tls::config cfg) {
    auto c = co_await net::tls::async_server(transport, cfg);
    if (!c) {
        println("{}", c.error().code() == net::tls::alert::no_application_protocol);
        println("{}", c.error().code().message());
    }
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    auto [near, far] = net::connection::in_memory();
    auto serving = async::spawn(serve(far, server_cfg));
    auto refused = net::tls::client(near, {.alpn = {"h2"}, .insecure_skip_verify = true});
    serving.wait();

    error_code code = net::tls::alert::decode_error;
    println("{} {}", code.value(), code.message());
}
```

Output:

```text
true
tls: no application protocol
50 tls: error decoding message
```

## See also

- [alert_of](alert_of.md), [is_remote](is_remote.md): the alert of an error and its side
- [category](category.md), [make_error_code](make_error_code.md): the codes
- [io::error](../../io/error.md)
- [net::tls](README.md)
