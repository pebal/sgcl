[sgcl](../README.md) › [slog](README.md) › [syslog](syslog/README.md)

# sgcl::slog::syslog::facility

```cpp
#include "sgcl/slog/syslog.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class syslog {
    public:
        enum class facility : uint8_t {
            kern = 0,
            user = 1,
            mail = 2,
            daemon = 3,
            auth = 4,
            syslog = 5,
            lpr = 6,
            news = 7,
            uucp = 8,
            cron = 9,
            authpriv = 10,
            ftp = 11,
            local0 = 16,
            local1 = 17,
            local2 = 18,
            local3 = 19,
            local4 = 20,
            local5 = 21,
            local6 = 22,
            local7 = 23
        };
    };
}
```

The facility of a syslog message, the kind of program it comes from, with the numbers of RFC 5424 §6.2.1: the PRI of
a message is the facility times 8 plus the severity. A program of one's own takes `user`, or one of `local0` to
`local7` that the site's syslog sends where it wants.

| Value | Description |
|---|---|
| `kern` | the kernel (0) |
| `user` | a program of a user (1), the default |
| `mail` | the mail system (2) |
| `daemon` | a system daemon (3) |
| `auth` | security and authorization (4) |
| `syslog` | the syslog daemon itself (5) |
| `lpr` | the printers (6) |
| `news` | network news (7) |
| `uucp` | UUCP (8) |
| `cron` | the clock daemon (9) |
| `authpriv` | private security messages (10) |
| `ftp` | the FTP daemon (11) |
| `local0` … `local7` | the site's own (16 to 23) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    io::buffer out;
    slog::logger log(slog::syslog(out, {.facility = slog::syslog::facility::mail, .app_name = "a", .hostname = "h"}));
    log.info("queued");
    println("{}", out.text().substr(0, 4));  // 2 * 8 + 6
}
```

Output:

```text
<22>
```

## See also

- [syslog::options](syslog-options.md)
- [sgcl::slog::syslog](syslog/README.md)
