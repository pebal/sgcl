[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::status

```cpp
#include "sgcl/net/http/status.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http::status {
    inline constexpr int continue_ = 100;
    inline constexpr int ok = 200;
    inline constexpr int not_found = 404;
    // ... every code of the registry, in the table below
}
```

`net::http::status` holds the status codes of the IANA registry (RFC 9110 §15 and the RFCs it lists) as plain `int`
constants, Go's `http.StatusOK` and the rest: a status is a number on the wire, and a program compares it with one,
as [response::status](response/status.md) gives it and as a handler sets it. Each is named after its phrase in snake
case. `continue_` carries an underscore, since `continue` is a keyword; `payload_too_large` and
`header_fields_too_large` stand beside the names RFC 9110 gave 413 and 431. The phrase of a code is
[reason](reason.md).

## Member objects

| Constant | Value | Reason phrase |
|---|---|---|
| `continue_` | 100 | Continue (`continue` is a keyword) |
| `switching_protocols` | 101 | Switching Protocols |
| `processing` | 102 | Processing |
| `early_hints` | 103 | Early Hints |
| `ok` | 200 | OK |
| `created` | 201 | Created |
| `accepted` | 202 | Accepted |
| `non_authoritative_information` | 203 | Non-Authoritative Information |
| `no_content` | 204 | No Content |
| `reset_content` | 205 | Reset Content |
| `partial_content` | 206 | Partial Content |
| `multi_status` | 207 | Multi-Status |
| `already_reported` | 208 | Already Reported |
| `im_used` | 226 | IM Used |
| `multiple_choices` | 300 | Multiple Choices |
| `moved_permanently` | 301 | Moved Permanently |
| `found` | 302 | Found |
| `see_other` | 303 | See Other |
| `not_modified` | 304 | Not Modified |
| `use_proxy` | 305 | Use Proxy |
| `temporary_redirect` | 307 | Temporary Redirect |
| `permanent_redirect` | 308 | Permanent Redirect |
| `bad_request` | 400 | Bad Request |
| `unauthorized` | 401 | Unauthorized |
| `payment_required` | 402 | Payment Required |
| `forbidden` | 403 | Forbidden |
| `not_found` | 404 | Not Found |
| `method_not_allowed` | 405 | Method Not Allowed |
| `not_acceptable` | 406 | Not Acceptable |
| `proxy_authentication_required` | 407 | Proxy Authentication Required |
| `request_timeout` | 408 | Request Timeout |
| `conflict` | 409 | Conflict |
| `gone` | 410 | Gone |
| `length_required` | 411 | Length Required |
| `precondition_failed` | 412 | Precondition Failed |
| `content_too_large` | 413 | Content Too Large |
| `payload_too_large` | 413 | Content Too Large, under its name before RFC 9110 |
| `uri_too_long` | 414 | URI Too Long |
| `unsupported_media_type` | 415 | Unsupported Media Type |
| `range_not_satisfiable` | 416 | Range Not Satisfiable |
| `expectation_failed` | 417 | Expectation Failed |
| `im_a_teapot` | 418 | I'm a teapot |
| `misdirected_request` | 421 | Misdirected Request |
| `unprocessable_content` | 422 | Unprocessable Content |
| `locked` | 423 | Locked |
| `failed_dependency` | 424 | Failed Dependency |
| `too_early` | 425 | Too Early |
| `upgrade_required` | 426 | Upgrade Required |
| `precondition_required` | 428 | Precondition Required |
| `too_many_requests` | 429 | Too Many Requests |
| `request_header_fields_too_large` | 431 | Request Header Fields Too Large |
| `header_fields_too_large` | 431 | Request Header Fields Too Large, under a shorter name |
| `unavailable_for_legal_reasons` | 451 | Unavailable For Legal Reasons |
| `internal_server_error` | 500 | Internal Server Error |
| `not_implemented` | 501 | Not Implemented |
| `bad_gateway` | 502 | Bad Gateway |
| `service_unavailable` | 503 | Service Unavailable |
| `gateway_timeout` | 504 | Gateway Timeout |
| `http_version_not_supported` | 505 | HTTP Version Not Supported |
| `variant_also_negotiates` | 506 | Variant Also Negotiates |
| `insufficient_storage` | 507 | Insufficient Storage |
| `loop_detected` | 508 | Loop Detected |
| `not_extended` | 510 | Not Extended |
| `network_authentication_required` | 511 | Network Authentication Required |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    int code = 404;
    switch (code) {
        case net::http::status::ok:
            println("fine");
            break;
        case net::http::status::not_found:
        case net::http::status::gone:
            println("{}: nothing there", net::http::reason(code));
            break;
        default:
            println("something else");
    }
    println("{}", net::http::status::payload_too_large == net::http::status::content_too_large);
}
```

Output:

```text
Not Found: nothing there
true
```

## See also

- [reason](reason.md): the phrase of a code
- [response::status](response/status.md), [response::ok](response/ok.md): the status a client receives
- [response_writer](response_writer.md): the status a handler sends
- [sgcl::net::http](README.md)
