[sgcl](../README.md) › [net](README.md)

# Benchmarks: net

The setup, the machine and how the timers are read are described with
[the benchmarks of the engine](../../garbage_collector/benchmarks.md). The single-operation cases of the module are in
`benchmarks/net/net.cpp` and `benchmarks/go/net`, run by `benchmarks/compare.sh` (`CASES=net`). The server under load,
HTTP/1.1, https and HTTP/2 against Go's, is on [the benchmarks of http](http/benchmarks.md).

## Single operations

`CASES=net VARIANTS="sgcl go" benchmarks/compare.sh build-release` in a clean environment (`env -i`): every case one
process a side, the sides alternated case by case, the best of three processes; the whole script three times, and
the table gives the median of the three bests with the smallest and the largest. Nanoseconds per operation, less is
better; the last column is SGCL's time over Go's, under 1 where SGCL is faster. Apple M2 Ultra (24 cores), Go 1.27.1,
SGCL at `-O3`, 5 October 2026, no other build or benchmark running (a load of about 3.5 from the system's own
indexing, one core busy). The cases:

- `pingpong`: 64 B there and back over one TCP connection, per round trip;
- `connect`: a TCP connect and its accept on the loopback, both closed, per connection;
- `parse`, `format`: an [ip_address](ip_address/README.md) from and to text, against `netip.Addr`, per address;
- `url`: [url::parse](url/parse.md) against `net/url`, per URL;
- `http_parse`: a request head parsed, per head;
- `http_hello`: a GET and its response over one kept connection, per request;
- `dns_parse`: a DNS response of MX, TXT or SRV records read as a stub resolver reads it, per message;
- `dns_lookup`: a lookup of five MX records against a server on the loopback answering from memory, per lookup;
- `socks5_connect`: a connection through a SOCKS5 server on the loopback, per connection;
- `proxy_get`: `http_hello` through a minimal HTTP forward proxy, per request;
- `multipart_parse`, `multipart_write`: a form of 20 fields and a file of 1 MB read or written, per body;
- `udp_multicast`: 64 B through a group joined on the loopback interface, sent and received in turn, per datagram;
- `tls_handshake`: a TCP connection and a full TLS 1.3 handshake, X25519 and an ECDSA P-256 leaf, per connection;
- `tls_resume`: the same handshake resumed from the session before;
- `tls_mtls`: a full handshake with a client certificate;
- `tls12_handshake`: a full TLS 1.2 handshake, both clients against one Go server of TLS 1.2 alone (the module's
  server speaks 1.3 alone), per connection;
- `ws_echo`: a WebSocket message of 64 B there and back, per round trip;
- `ws_throughput`: WebSocket messages of 1 MB one way, per message;
- `sse_events`: Server-Sent Events through one stream, per event;
- `ocsp_verify`: an OCSP response of a delegated responder parsed and verified for a certificate
  ([ocsp_response::verify](../crypto/x509-ocsp_response/verify.md)), per response; Go's `golang.org/x/crypto/ocsp`
  `ParseResponseForCert` (benchmarks/go/ocsp, a module of its own);
- `crl_1k`, `crl_100k`: a CRL of 1000 or 100 000 entries parsed, its signature checked and its last serial looked
  up ([revocation_list](../crypto/x509-revocation_list/README.md)), per list; Go's `x509.ParseRevocationList`;
- `tls_staple`: `tls_handshake` of a chain of two, the server stapling an OCSP response the client verifies
  (`revocation_mode::staple_only`; Go: `VerifyConnection` with `ocsp.ParseResponseForCert`), per connection;
- `tls_nostaple`: the same handshake without the staple and its check.

| Case | SGCL | Go | SGCL / Go |
|---|---|---|---|
| pingpong | 15 438 (15 128–15 542) | 18 989 (18 870–19 447) | 0.81 |
| connect | 41 514 (41 215–42 032) | 49 724 (49 113–50 318) | 0.83 |
| parse | 20.4 (20.3–20.5) | 26.2 (26.1–26.2) | 0.78 |
| format | 16.9 (16.9–16.9) | 34.8 (34.8–34.8) | 0.49 |
| url | 71.9 (71.8–72.7) | 156 (155–156) | 0.46 |
| http_parse | 779 (778–779) | 2 192 (2 188–2 198) | 0.36 |
| http_hello | 25 208 (25 148–25 214) | 34 941 (34 875–35 074) | 0.72 |
| dns_parse | 366 (366–367) | 570 (570–570) | 0.64 |
| dns_lookup | 75 639 (69 217–93 134) | 74 527 (69 754–75 046) | 1.01 |
| socks5_connect | 170 411 (168 305–175 384) | 142 599 (141 454–143 042) | 1.20 |
| proxy_get | 45 591 (45 496–45 665) | 52 785 (52 767–52 826) | 0.86 |
| multipart_parse | 108 880 (108 699–108 898) | 136 005 (135 876–136 723) | 0.80 |
| multipart_write | 49 428 (47 382–49 850) | 60 716 (58 289–60 768) | 0.81 |
| udp_multicast | 16 502 (16 456–16 531) | 13 785 (13 775–13 827) | 1.20 |
| tls_handshake | 506 635 (502 590–509 040) | 427 517 (427 403–427 757) | 1.19 |
| tls_resume | 231 850 (231 819–232 284) | 353 966 (353 734–354 344) | 0.66 |
| tls_mtls | 813 063 (810 934–843 567) | 716 536 (715 602–717 513) | 1.13 |
| tls12_handshake | 564 231 (561 177–565 344) | 611 380 (611 228–612 713) | 0.92 |
| ws_echo | 17 242 (16 990–17 243) | 16 922 (16 880–16 978) | 1.02 |
| ws_throughput | 172 383 (170 172–177 613) | 225 361 (222 014–225 733) | 0.76 |
| sse_events | 4 138 (3 950–4 318) | 2 095 (2 075–2 173) | 1.97 |

`stream` (32 KB writes one way over one TCP connection) is out of the table: its time per byte rounds to a tenth of
a nanosecond. Run by hand for about two seconds a side, three processes each (`bench_net stream sgcl 8192`, 8 GB;
`net stream 16384`, 16 GB), SGCL moved 4.30 GB/s (4.19–4.58) and Go 9.24 GB/s (9.01–9.35): SGCL takes 2.15 times
Go's time, and 0.75 ns of CPU per byte against Go's 0.21.

Where SGCL is behind: the stream and `sse_events` by about two times; `socks5_connect`, `udp_multicast` and the full
TLS 1.3 handshakes (`tls_handshake`, `tls_mtls`) by 13 to 20 per cent. `dns_lookup` and `ws_echo` are even with Go
(one of the three runs of `dns_lookup` read 93 µs on SGCL's side, the other two 69 and 76). Everything that parses
or writes text — addresses, URLs, request heads, DNS messages, multipart bodies — is faster than Go's, by up to
nearly three times for a request head, and so are the exchanges over a kept connection, the resumed TLS 1.3
handshake and the TLS 1.2 handshake.

## Mail

[encoding::email](../encoding/email/README.md) and [net::smtp](smtp/README.md) against Go's `net/mail`, `mime`,
`mime/multipart`, `mime/quotedprintable` and `net/smtp`: `benchmarks/net/mail.cpp` and `benchmarks/go/mail`, run by
`CASES=mail VARIANTS="sgcl go" benchmarks/compare.sh build-release`. The cases:

- `mime_build`: a message of a text of 2 KB and an HTML of 4 KB (both past ASCII, quoted-printable), an attachment
  of 1 MB (base64) and a subject in encoded words, written, per message;
- `mime_parse`: that message parsed, its text and HTML decoded to UTF-8 and its attachment's bytes decoded, per
  message;
- `smtp_client`: messages of 4 KB over one session to one minimal Go server on the loopback (EHLO, MAIL, RCPT,
  DATA), the module's client against `net/smtp`'s, per message;
- `smtp_server`: the same messages from `net/smtp`'s client, the module's server against a minimal Go server written
  by hand, per message.
