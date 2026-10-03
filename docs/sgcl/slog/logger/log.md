[sgcl](../../README.md) › [slog](../README.md) › [logger](README.md)

# sgcl::slog::logger::log, debug, info, warn, error

```cpp
template<class... A>
void log(slog::level l, message m, const A&... kv) const;    // (1)
template<class... A>
void debug(message m, const A&... kv) const;                 // (2)
template<class... A>
void info(message m, const A&... kv) const;                  // (3)
template<class... A>
void warn(message m, const A&... kv) const;                  // (4)
template<class... A>
void error(message m, const A&... kv) const;                 // (5)
```

Writes a record: the time, the level, the [message](../message/README.md) `m` and the attributes `kv`, after the logger's
own ([with](with.md)), slog's `Log`, `Debug`, `Info`, `Warn` and `Error`. A record below the logger's level is one
comparison and makes nothing ([enabled](enabled.md)).

1. A record of the level `l`, any `int8_t` ([level](../level.md)).
2. A record of `level::debug`.
3. A record of `level::info`.
4. A record of `level::warn`.
5. A record of `level::error`.

- (1–5) The attributes are pairs, a key then a value, and [groups](../group/README.md) standing where a key would: the key
  a literal or a `const char*`, the value one of the kinds the module takes
  ([The kinds of values](../README.md#the-kinds-of-values)). An odd count, a key that is not a string, a group given
  as a value, a character or a value of another kind is an error of the build. The line is made whole in the
  thread's plain memory and written by one `write`, or batched with `options::buffered`; with a handler of the
  program, the handler's `handle` is called on this thread with the [record](../record/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `l` | the level of the record |
| `m` | the text of the record and the place of the call, made where the call is written |
| `kv` | the attributes: pairs and groups |

## Return value

None. A write that fails is counted by [dropped](dropped.md), and the first failure of an output is said in one
line on `io::stderr`.

## Complexity

Below the level, constant. Otherwise linear in the size of the line; nothing on the managed heap but the string a
value's `to_text`, `to_string` or `io::error` message makes.

## Exceptions

What the text of a value of the program throws (its `to_text`, `to_string` or `format_value`), what a handler of
the program throws, what a writer of the program throws; a failed write throws nothing. A writer that throws on a buffered logger's batch
loses the lines in it, and the batch takes the next record.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger log(io::stdout, slog::level::debug);
    log.debug("cache", "hits", 41, "misses", 1);
    log.info("server started", "port", 8080, "tls", true);
    log.warn("slow request", "path", "/users", "took", 1500 * millisecond);
    log.error("read failed", "error", io::read_file("/no/such/file").error());
    log.log(slog::level(2), "between info and warn", "ratio", 0.25);
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=DEBUG msg=cache hits=41 misses=1
time=2026-09-28T14:05:01.123+02:00 level=INFO msg="server started" port=8080 tls=true
time=2026-09-28T14:05:01.123+02:00 level=WARN msg="slow request" path=/users took=1.5s
time=2026-09-28T14:05:01.123+02:00 level=ERROR msg="read failed" error="open /no/such/file: No such file or directory"
time=2026-09-28T14:05:01.123+02:00 level=INFO+2 msg="between info and warn" ratio=0.25
```

## See also

- [debug, info, warn, error](../debug.md): the same through the default logger
- [enabled](enabled.md): whether a record of a level is written
- [sgcl::slog::logger](README.md)
