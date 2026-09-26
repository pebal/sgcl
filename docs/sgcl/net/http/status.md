# sgcl::net::http::status, sgcl::net::http::reason

```cpp
#include "sgcl/net/http/status.h"   // or "sgcl/net/http/http.h"

namespace sgcl::net::http {
    namespace status {
        inline constexpr int ok = 200, created = 201, no_content = 204, moved_permanently = 301, found = 302,
            see_other = 303, not_modified = 304, temporary_redirect = 307, permanent_redirect = 308,
            bad_request = 400, unauthorized = 401, forbidden = 403, not_found = 404, method_not_allowed = 405,
            content_too_large = 413, request_header_fields_too_large = 431, internal_server_error = 500,
            not_implemented = 501, service_unavailable = 503, http_version_not_supported = 505 /* … */;
    }
    constexpr const char* reason(int code) noexcept;
}
```

The codes of the IANA registry (RFC 9110 §15 and the RFCs it lists) as plain `int`s, Go's `http.StatusOK` and the rest: a status is a number on the wire, and a program compares it with one. `continue_` carries an underscore, since `continue` is a keyword; `payload_too_large` and `header_fields_too_large` are there beside the names RFC 9110 gave 413 and 431. `reason(code)` is the registry's phrase, `""` for a code it does not have (Go's `StatusText`).

## Example

```cpp
#include "sgcl/net/http/status.h"
#include <iostream>

using namespace sgcl;

int main() {
    for (int code : {net::http::status::ok, net::http::status::not_found, 418, 599}) {
        std::cout << code << " \"" << net::http::reason(code) << "\"\n";
    }
}
```

Output:

```text
200 "OK"
404 "Not Found"
418 "I'm a teapot"
599 ""
```

## See also

- [response_writer](response_writer.md) (`set_status`, `error`), [response](response.md) (`status`, `ok`)
