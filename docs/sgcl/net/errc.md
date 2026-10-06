[sgcl](../README.md) › [net](README.md)

# sgcl::net::errc

```cpp
#include "sgcl/net/error.h"   // or "sgcl/net.h"

namespace sgcl::net {
    enum class errc {
        invalid_address = 1,
        host_not_found,
        no_suitable_address,
        invalid_url,
        unsupported_scheme,
        malformed_response,
        header_too_large,
        body_too_large,
        too_many_redirects,
        server_closed,
        invalid_cookie,
        http_status,
        no_data,
        server_failure,
        server_misbehaving,
        proxy_failure,
        proxy_refused,
        proxy_unsupported,
        proxy_auth_required,
        malformed_proxy_response,
        malformed_multipart,
        too_many_parts,
        not_multipart,
        websocket_handshake,
        websocket_protocol,
        websocket_closed,
        ssh_handshake,
        ssh_host_key_unknown,
        ssh_host_key_mismatch,
        ssh_host_key_revoked,
        ssh_auth_failed,
        ssh_protocol,
        ssh_disconnected,
        ssh_channel_refused,
        ssh_request_refused
        smtp_reply,
        smtp_auth_failed,
        smtp_tls_required,
        smtp_unsupported,
        malformed_smtp_reply,
        sftp_failure,
        sftp_protocol,
        malformed_message,
        malformed_dmarc
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::errc> : std::true_type {};
```

The failures of net that neither `errno` nor [io](../io/errc.md) names: the sockets' and the resolver's,
HTTP's ([net::http](http/README.md)), a proxy's ([socks5](socks5/README.md), the HTTP client's proxies), and SSH's ([net::ssh](ssh/README.md)). They form the category of the module, `"net"` ([category](category.md)),
beside the system one, io's and the resolver's ([lookup_category](lookup_category.md)). Everything net reports is an
[io::error](../io/error/README.md) in an `expected<T, io::error>`, one error for all where Go has `net.ErrClosed`,
`os.ErrDeadlineExceeded` and `*net.DNSError`: its code is an `errno` value in the system category
(`ECONNREFUSED`, `ETIMEDOUT` for a deadline, `ECANCELED` for a stop, `EPIPE`), io's `errc::closed` for an
operation on a connection the program closed, one of these, or an `EAI_*` code of the resolver. The operation names
what failed as Go does (`dial tcp`, `listen tcp`, `read`, `accept`, `lookup`), the path what it was on, so that
`message()` reads `lookup db.internal: no such host`.

`std::is_error_code_enum` is specialized, so an `errc` converts to an `error_code`
([make_error_code](make_error_code.md)) and `e.code() == net::errc::host_not_found` compares directly. The
predicates of `io::error` answer across the categories: `is_timeout()` for a deadline, `is_closed()` for a close by
the program, `is_not_found()` for a unix socket's path that is not there.

| Value | Description |
|---|---|
| `invalid_address` | "invalid address": a text that [ip_address](ip_address/README.md), [ip_network](ip_network/README.md) or [endpoint](endpoint/README.md)`::parse` does not read as one; a `"host:port"` that cannot be taken apart (no port, a port past 65535 or not a number, an IPv6 host without brackets); a unix path too long for `sun_path` |
| `host_not_found` | "no such host": the resolver knows no address of the name (`EAI_NONAME`), a DNS server says the name does not exist (NXDOMAIN); an empty host to look up, a text that is no DNS name |
| `no_suitable_address` | "no suitable address found": a dial with nothing to dial, every address the name resolved to of no use |
| `invalid_url` | "invalid URL": a text [url](url/README.md)`::parse` does not read as one, or a value a setter of `url` refuses; a text past 512 MiB, or one whose URL would pass it ([the limit](url/README.md#rules)); a query text past 512 MiB, or pairs [query_params](query_params/README.md) would write past it ([its limit](query_params/README.md#rules)); a request's URL that is not one, reported by the send |
| `unsupported_scheme` | "unsupported protocol scheme": a URL whose scheme the client cannot speak, neither `http` nor `https` |
| `malformed_response` | "malformed HTTP response": a response that breaks RFC 9112, its head or its framing |
| `header_too_large` | "header too large": a head past its limit |
| `body_too_large` | "body too large": a read of a body past its limit |
| `too_many_redirects` | "stopped after too many redirects": more than the client's `max_redirects`, 10 by default |
| `server_closed` | "server closed": `serve` after `shutdown()` or `close()`, Go's `ErrServerClosed` |
| `invalid_cookie` | "invalid cookie": a `Set-Cookie` value that [cookie](http/cookie/README.md)`::parse` finds no cookie in |
| `http_status` | "the response's status is not 2xx": [http::download](http/download.md) of a URL that answered 404 or 500, nothing written |
| `no_data` | "no DNS record of the type asked": a name that exists without a record of the type a [lookup_mx](dns/lookup_mx.md), [lookup_txt](dns/lookup_txt.md), [lookup_srv](dns/lookup_srv.md) or [lookup_ns](dns/lookup_ns.md) asked for (NODATA, RFC 2308) |
| `server_failure` | "DNS server failure": the servers answered SERVFAIL (RCODE 2) |
| `server_misbehaving` | "DNS server misbehaving": a server's other refusals (REFUSED, FORMERR, NOTIMP, an extended RCODE), an answer that does not read, a referral where an answer was due, a chain of CNAMEs past eight |
| `proxy_failure` | "proxy failure": a SOCKS5 reply of general failure, or of a code RFC 1928 does not define |
| `proxy_refused` | "the proxy refused the connection": a SOCKS5 reply of a connection its rules do not allow; an HTTP proxy's answer to CONNECT other than 2xx and 407 |
| `proxy_unsupported` | "the proxy does not support the request": a SOCKS5 reply of a command or an address type not supported |
| `proxy_auth_required` | "proxy authentication required": a SOCKS5 proxy that takes none of the methods offered or refused the credentials; an HTTP proxy's 407 |
| `malformed_proxy_response` | "malformed proxy response": a SOCKS5 answer that breaks RFC 1928 (a version other than 5, a method never offered, an unknown address type), an HTTP proxy's answer to CONNECT that is not a response |
| `malformed_multipart` | "malformed multipart body": a [multipart body](http/multipart_reader/README.md) with no boundary, a part's head that is not fields, a boundary of no character or of more than 70 |
| `too_many_parts` | "too many parts": a multipart body of more parts than its [limits](http/multipart_reader-limits.md) allow, 1000 by default |
| `not_multipart` | "the body is not multipart": [request::multipart](http/request/multipart.md) of a request whose `Content-Type` is not `multipart/...` |
| `websocket_handshake` | "WebSocket handshake failed": a server's answer that is not an upgrade ([websocket::connect](http/websocket/connect.md)), a request a server refused ([websocket::accept](http/websocket/accept.md)) |
| `websocket_protocol` | "WebSocket protocol violation": a frame or a message that breaks RFC 6455 (a reserved bit or opcode, a control frame fragmented or too long, a mask on the wrong side, text not UTF-8), the connection failed with its close code |
| `websocket_closed` | "WebSocket closed by the peer": the peer's close frame, its code in [close_status](http/websocket/close_status.md) |
| `ssh_handshake` | "SSH handshake failed": no SSH version line or another version than 2.0, no algorithm in common (the message names the list), a host key whose signature does not verify, a strict key exchange broken ([ssh::client::connect](ssh/client/connect.md)) |
| `ssh_host_key_unknown` | "SSH host key unknown": a host [known_hosts](ssh/known_hosts/README.md) does not know |
| `ssh_host_key_mismatch` | "SSH host key mismatch": a host known_hosts knows with another key of the same type |
| `ssh_host_key_revoked` | "SSH host key revoked": a key or an authority a `@revoked` line names |
| `ssh_auth_failed` | "SSH authentication failed": no method succeeded (the message names those tried and those the server takes), or the server ended it after too many failures |
| `ssh_protocol` | "SSH protocol error": a packet whose MAC or tag does not match, a message that breaks RFC 4253 or 4254 |
| `ssh_disconnected` | "SSH connection closed by the peer": the peer's disconnect (its reason in the message), the connection's end |
| `ssh_channel_refused` | "SSH channel refused": a session or a forwarded connection the peer would not open (the reason in the message) |
| `ssh_request_refused` | "SSH request refused": a request the peer answered with a failure (a command, a terminal, a forwarding), an agent's refusal |
| `smtp_reply` | "the SMTP server refused": a reply refusing a command of [net::smtp](smtp/README.md), the reply in the error's path ([reply_of](smtp/reply_of.md)) |
| `smtp_auth_failed` | "SMTP authentication failed": AUTH refused (535), the reply in the path |
| `smtp_tls_required` | "TLS required but not available": `require_tls` and no STARTTLS, or credentials over clear text without `allow_insecure_auth` |
| `smtp_unsupported` | "the SMTP server does not support what the message needs": an address past ASCII and no SMTPUTF8, a message past SIZE, no AUTH mechanism of both sides |
| `malformed_smtp_reply` | "malformed SMTP reply": a reply that breaks RFC 5321, bytes after STARTTLS's 220 in clear text |
| `sftp_failure` | "SFTP failure": a request of [net::sftp](sftp/README.md) the server failed with no more specific status (a directory that is there, one not empty), the server's message after the path |
| `sftp_protocol` | "SFTP protocol error": an SFTP packet that breaks the protocol, a version other than 3, no answer to INIT in time |
| `malformed_message` | "malformed mail message": a message [dkim::signer::sign](dkim/signer/sign.md) cannot sign (no head, no From, a line of the head that is no field, `l=` past the body), an Authentication-Results value that breaks RFC 8601 ([smtp::authentication_results::parse](smtp/authentication_results/parse.md)) |
| `malformed_dmarc` | "malformed DMARC record": a record that breaks RFC 7489 ([dmarc::record::parse](dmarc/record/parse.md)), an aggregate report that cannot be read ([dmarc::report::parse](dmarc/report/parse.md)) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto c = net::tcp::connect("127.0.0.1");  // no port
    println("{} {}", c.error().code() == net::errc::invalid_address, c.error().message());

    auto found = net::dns::lookup("");
    println("{}", found.error().code() == net::errc::host_not_found);

    error_code code = net::errc::too_many_redirects;
    println("{}: {}", code.category().name(), code.message());
}
```

Output:

```text
true dial tcp 127.0.0.1: invalid address
true
net: stopped after too many redirects
```

## See also

- [io::error](../io/error/README.md): the code, the operation, the path, the predicates
- [category](category.md), [lookup_category](lookup_category.md), [make_error_code](make_error_code.md)
- `tests/net/socket.cpp` (`ErrorsOfConnectAndListen`), `tests/net/dial.cpp` (`Names`)
