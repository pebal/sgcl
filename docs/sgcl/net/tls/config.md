[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::config

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    struct config {
        string server_name;
        optional<crypto::x509::certificate_pool> roots;
        vector<tls::identity> identities;
        vector<tls::group> groups = {group::x25519_mlkem768, group::x25519, group::secp256r1,
                                     group::secp384r1};
        vector<tls::cipher> ciphers = {cipher::aes_128_gcm_sha256, cipher::chacha20_poly1305_sha256,
                                       cipher::aes_256_gcm_sha384,
                                       cipher::ecdhe_ecdsa_aes_128_gcm_sha256,
                                       cipher::ecdhe_rsa_aes_128_gcm_sha256,
                                       cipher::ecdhe_ecdsa_chacha20_poly1305_sha256,
                                       cipher::ecdhe_rsa_chacha20_poly1305_sha256,
                                       cipher::ecdhe_ecdsa_aes_256_gcm_sha384,
                                       cipher::ecdhe_rsa_aes_256_gcm_sha384};
        tls::version min_version = version::tls12;
        tls::version max_version = version::tls13;
        vector<string> alpn;
        bool insecure_skip_verify = false;
        duration handshake_timeout = 10 * second;
        tls::client_auth client_auth = client_auth::none;
        optional<crypto::x509::certificate_pool> client_roots;
        optional<tls::session_cache> session_cache;
        bool session_tickets = true;
        duration ticket_lifetime = 24 * hour;
        tls::ticket_keys ticket_keys;
        tls::revocation_mode revocation = revocation_mode::off;
        duration revocation_timeout = 5 * second;
        vector<crypto::x509::revocation_list> crls;
        bool fetch_crls = true;
        optional<tls::revocation_cache> revocation_cache;
        bool ocsp_stapling = false;
        identity_function identity_for;
    };

    using identity_function =
        function<async::task<expected<tls::identity, io::error>>(const client_hello&)>;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::config` is the settings of a connection, Go's `tls.Config`: a plain value, copied into the connection, so
that a change after the call reaches none made before. One type serves both sides, each reading its own members: a
client the name, the roots, `insecure_skip_verify` and the session cache; a server the client certificate it asks
for and its roots, the tickets and the stapling of its OCSP responses; both the check of the peer's chain for
revocation, the identities (a server's, and a client's for a server that asks for one),
the groups, the cipher suites, the versions, the ALPN protocols and the timeout, a server's lists being its
preferences, in order. The defaults are Go's and the
browsers' ([the rules](README.md#the-rules)); a default config is a client that verifies the server against the
system's roots for the host it dials. A designated initializer names what differs:
`net::tls::connect(a, {.alpn = {"h2"}})`.

## Rules

- A config the handshake cannot start with is refused with `EINVAL` before a record is sent: one without a group or
  a cipher suite; one whose `min_version` is above its `max_version` or either no [version](version.md); a client's
  with no version of the range that has a suite of its own in `ciphers`, or of 1.2 alone without X25519, P-256 or
  P-384; a server's whose range has no 1.3 or whose list has no 1.3 suite (the server speaks 1.3 alone); a client's without a name to verify (no `server_name`, no `insecure_skip_verify`, and an address
  without a host, or [client](client.md), which has no address); a server's without an identity or an `identity_for`, with a
  `client_auth` of no value of its enumeration, or with tickets on and a `ticket_lifetime` under a second or past
  seven days; one with an ALPN protocol of 0 or more than 255 bytes; one with a `revocation` of no value of its
  enumeration.
- Copies of a config share the identities and their keys, which are never copied, the session cache, the ticket keys
  and the revocation cache: a copy of a server's config resumes the tickets of the original.
- The check of revocation runs after the handshake verified the chain and before [connect](connect.md),
  [client](client.md) or a listener's `accept` gives the connection out, within `handshake_timeout` and
  `revocation_timeout` both; a chain it refuses ends the connection with the check's alert
  ([revocation_mode](revocation_mode.md)). A resumed session is not checked again, nor a chain taken with
  `insecure_skip_verify`. Its fetches are plain HTTP (the `http` URIs of the certificates; RFC 5280 and RFC 6960 name
  no other), each over a connection of its own.

## Member objects

| Member | Description |
|---|---|
| `server_name` | the client's: the name sent as SNI (never an IP address) and checked against the leaf's names, or its IP addresses for an address (Go's `ServerName`); empty, the default, is the host of the address [connect](connect.md) was given |
| `roots` | the client's: the certificates the server's chain must lead to, a [crypto::x509::certificate_pool](../../crypto/x509.md) (Go's `RootCAs`); `nullopt`, the default, is the system's |
| `identities` | the certificate chains with their keys ([identity](identity/README.md), Go's `Certificates`). A server's, at least one: the first whose leaf is for the name the client sent is chosen, else the first. A client's, sent when the server asks for a certificate: the first whose key signs a scheme the server takes and whose chain one of the authorities it names issued, else none; empty by default |
| `groups` | the key exchange groups, in order of preference ([group](group.md), Go's `CurvePreferences`); a client sends a key share of the first, and of X25519 beside the hybrid when the list has it; by default X25519MLKEM768, X25519, P-256, P-384 |
| `ciphers` | the cipher suites, in order of preference ([cipher](cipher.md)); Go's TLS 1.3 suites are not configurable; by default AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM of 1.3, then the six ECDHE suites of 1.2 in the same order of AEADs; a client offers those of the versions it offers, a server takes the 1.3 ones |
| `min_version` | the oldest version a client offers ([version](version.md), Go's `MinVersion`): `tls12` by default, for servers without 1.3; `tls13` requires 1.3, and a server of 1.2 alone then fails with `protocol_version`. A server's range must hold 1.3 |
| `max_version` | the newest version a client offers (Go's `MaxVersion`): `tls13` by default; `tls12` keeps a client to 1.2 |
| `alpn` | the application protocols (RFC 7301), in order of preference (Go's `NextProtos`): a client offers them, a server takes the first of its list the client offers and refuses a client that offers some and none of them; empty by default, none offered and none answered |
| `insecure_skip_verify` | the client's: the server's chain taken unchecked (Go's `InsecureSkipVerify`); for tests only: with it, any machine in the path can read and change the traffic; `false` by default |
| `client_auth` | the server's: whether it asks the client for a certificate ([client_auth](client_auth.md), Go's `ClientAuth`): `none` by default, `request`, `require` |
| `client_roots` | the server's: the certificates a client's chain must lead to, for client authentication (Go's `ClientCAs`); their subjects are the authorities the `CertificateRequest` names; `nullopt`, the default, is the system's roots, and names none |
| `session_cache` | the client's: where the sessions of its servers' tickets are kept and taken from, a 1.2 server's ticket or session id among them ([session_cache](session_cache/README.md), Go's `ClientSessionCache`); `nullopt`, the default, keeps none and resumes nothing. An [http::client](../http/client/README.md)'s config has one of its own |
| `session_tickets` | the server's: whether it issues a session ticket after each handshake and resumes the sessions of its tickets (Go's `SessionTicketsDisabled`, the other way round); a client's with a `session_cache`: whether it asks a server of 1.2 for a ticket (RFC 5077) and offers its tickets, else 1.2 sessions resume by their session ids alone; `true` by default |
| `ticket_lifetime` | the server's: how long a ticket resumes, from its issue (the `ticket_lifetime` of its NewSessionTicket), and the age past which the current ticket key is replaced; a second to seven days, a day by default |
| `ticket_keys` | the server's: the keys its tickets are sealed under ([ticket_keys](ticket_keys/README.md)), made at random with the config and shared by its copies |
| `revocation` | whether and how the peer's chain is checked for revocation once it has verified ([revocation_mode](revocation_mode.md)): the server's chain by a client, which then asks for a staple (`status_request`), a client's by a server that asks for one; `off` by default, as in Go; `staple_only`, `soft_fail`, `hard_fail` |
| `revocation_timeout` | how long the online checks of one handshake may take together, every responder and every CRL; a status not had by then is unknown (`soft_fail` takes it, `hard_fail` refuses); 5 seconds by default |
| `crls` | CRLs of the program's ([crypto::x509::revocation_list](../../crypto/x509-revocation_list/README.md)), checked before anything online, in every mode but `off`: a complete list of a certificate's issuer that covers it, with a delta of it from the same vector when there is one; none by default |
| `fetch_crls` | `soft_fail` and `hard_fail`: whether the CRLs of a certificate's distribution points are fetched when OCSP gives no status (a certificate without a responder, a responder down); `true` by default |
| `revocation_cache` | where the online checks keep what they fetched, until its `nextUpdate` ([revocation_cache](revocation_cache/README.md)); `nullopt`, the default, is the process's own |
| `ocsp_stapling` | the server's: whether it fetches the OCSP response of each identity's leaf from the responders of the leaf's authority information access, verified under the issuer the chain holds, staples it, and fetches the next halfway through its validity (a failed fetch tried again a minute later, the response there kept until its `nextUpdate`); the fetches run in tasks of their own, the first when the listener is made; `false` by default: an identity staples what [set_ocsp_staple](identity/set_ocsp_staple.md) gave it |
| `identity_for` | the server's: its identity chosen for each connection (Go's `GetCertificate`), a [function](../../core/function/README.md) given the [client_hello](client_hello.md) (the name and the protocols the client asked for) and returning a task of the identity or of an error. The handshake waits for it once the first ClientHello is in, and serves the identity it gives; an error, or an exception of the function, ends the handshake with `internal_error`, as Go's server answers a `GetCertificate` that fails. In place of `identities`, which it is not combined with; empty by default. What [acme::manager](../acme/manager/README.md)'s [tls_config](../acme/manager/tls_config.md) sets |
| `handshake_timeout` | how long the handshake may take: for [connect](connect.md), the lookup, the TCP connect and the handshake together; for [listen](listen.md), each connection's handshake. It ends them with `ETIMEDOUT`: zero or less at once, before anything is sent; `duration::max()` is no limit; 10 seconds by default |

## Example

A client that narrows the lists to a group and a cipher suite the server does not prefer: the server asks for the
group with a HelloRetryRequest, and both settle on them. A config without a group is refused.

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {
        net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                           crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    net::listener incoming = net::tls::listen("127.0.0.1:0", server_cfg);
    string address = "localhost:" + to_string(incoming.local_endpoint().port());

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.groups = {net::tls::group::secp384r1};
    cfg.ciphers = {net::tls::cipher::chacha20_poly1305_sha256};
    auto c = net::tls::connect(address, cfg);
    auto s = net::tls::state_of(*c);
    println("{} {}", s->group == net::tls::group::secp384r1,
            s->cipher == net::tls::cipher::chacha20_poly1305_sha256);
    c->close();

    cfg.groups = {};
    println("{}", net::tls::connect(address, cfg).error().message());
    incoming.close();
}
```

Output:

```text
true true
tls a config without cipher suites or groups: Invalid argument
```

## See also

- [connect](connect.md), [client](client.md), [listen](listen.md), [server](server.md): the functions that take it
- [identity](identity/README.md): a certificate and its key
- [client_auth](client_auth.md), [session_cache](session_cache/README.md), [ticket_keys](ticket_keys/README.md): client certificates and resumption
- [state](state.md): what the handshake settled of it
- [net::tls](README.md)
