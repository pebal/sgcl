[sgcl](../README.md) › [slog](README.md) › [syslog](syslog/README.md)

# sgcl::slog::syslog::options

```cpp
#include "sgcl/slog/syslog.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class syslog {
    public:
        struct options {
            syslog::facility facility = syslog::facility::user;
            string app_name;
            string hostname;
            syslog::format format = syslog::format::automatic;
            bool octet_counting = false;
            string structured_data_id;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::syslog::options` is what a [syslog](syslog/README.md) handler writes its messages with: a plain struct,
filled by designated initializers: `{.app_name = "api", .octet_counting = true}`.

## Member objects

| Member | Description |
|---|---|
| `facility` | the facility of every message ([syslog::facility](syslog-facility.md)); `user` by default |
| `app_name` | RFC 5424's APP-NAME, RFC 3164's TAG; empty, the default: the program's name, the base of `args()[0]` |
| `hostname` | the HOSTNAME; empty, the default: the system's, `-` when it has none |
| `format` | RFC 5424 or RFC 3164 ([syslog::format](syslog-format.md)); `automatic` by default: RFC 3164 for [local](syslog/local.md), RFC 5424 for a writer |
| `octet_counting` | the framing of a stream, the length and a space before each message (RFC 6587, RFC 5425): for TCP and TLS; `false` by default |
| `structured_data_id` | non-empty: the attributes as RFC 5424 SD-PARAMs under this SD-ID (`app@32473`, a name and a private enterprise number), their names flattened with dots and their values escaped, instead of the text after the message; RFC 5424 only |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::syslog::options o{.facility = slog::syslog::facility::local0, .app_name = "billing", .hostname = "h"};
    slog::logger log(slog::syslog(out, o));
    log.error("payment failed");
    println("{}", out.text().substr(0, 6));  // local0 is 16: 16 * 8 + 3
}
```

Output:

```text
<131>1
```

## See also

- [syslog::facility](syslog-facility.md), [syslog::format](syslog-format.md)
- [sgcl::slog::syslog](syslog/README.md)
