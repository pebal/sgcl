[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::criteria

```cpp
#include "sgcl/net/imap/criteria.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class criteria;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

What a [search](../client/search.md) looks for (RFC 9051 §6.4.4): a search key made by a static function, or keys
combined with `&&` (both), `||` (either) and `!` (not): `net::imap::criteria::unseen() &&
(net::imap::criteria::from("alice") || net::imap::criteria::from("bob"))`. A value, the text of the keys inside it;
the default is every message. [count](../client/count.md), [sort](../client/sort.md) and
[threads](../client/threads.md) take one too. A search the class has no function for is
[search](../client/search.md)'s text form, `session.search("OR NEW OLD")`.

## Rules

- **Strings** are written as the connection allows: quoted, or as a literal, with CHARSET UTF-8 for 8-bit text to a
  server without UTF-8 of its own. A NUL cannot be searched for and is left out. The server matches them as
  substrings without regard to case.
- **Dates** are days of the calendar ([time::date](../../../time/date/README.md)): RFC 9051 compares dates without
  their times and zones.
- **Combinations.** `a && b` is the keys one after the other; `a || b` is OR of the two, `!a` NOT, each of a
  combination parenthesized.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](criteria.md) | every message |
| [to_string](to_string.md) | the keys as the command writes them |

#### Search keys (static)

| Function | Description |
|---|---|
| [all](all.md) | every message |
| [seen](seen.md) | the messages with `\Seen` |
| [unseen](unseen.md) | the messages without `\Seen` |
| [answered](answered.md) | the messages with `\Answered` |
| [unanswered](unanswered.md) | the messages without `\Answered` |
| [flagged](flagged.md) | the messages with `\Flagged` |
| [unflagged](unflagged.md) | the messages without `\Flagged` |
| [deleted](deleted.md) | the messages with `\Deleted` |
| [undeleted](undeleted.md) | the messages without `\Deleted` |
| [draft](draft.md) | the messages with `\Draft` |
| [undraft](undraft.md) | the messages without `\Draft` |
| [keyword](keyword.md) | the messages with the keyword `k` (`$Forwarded`, `$Work`); a system flag given is its key (`\Seen` as SEEN, by the server) |
| [unkeyword](unkeyword.md) | the messages without the keyword `k` |
| [from](from.md) | the messages whose From: holds `s` (the address or the name, encoded words decoded) |
| [to](to.md) | the messages whose To: holds `s` |
| [cc](cc.md) | the messages whose Cc: holds `s` |
| [bcc](bcc.md) | the messages whose Bcc: holds `s` |
| [subject](subject.md) | the messages whose Subject: holds `s` |
| [body](body.md) | the messages whose body (the text after the header, its text parts decoded) holds `s` |
| [text](text.md) | the messages whose header or body holds `s` |
| [header](header.md) | the messages with a field `field` holding `value`; an empty value: any message with the field |
| [before](before.md) | the messages received (INTERNALDATE) before the day |
| [on](on.md) | the messages received on the day |
| [since](since.md) | the messages received on or after the day |
| [sent_before](sent_before.md) | the messages whose Date: is before the day |
| [sent_on](sent_on.md) | the messages whose Date: is the day |
| [sent_since](sent_since.md) | the messages whose Date: is the day or after |
| [older](older.md) | the messages received more than `d` ago (WITHIN, RFC 5032), to the second |
| [younger](younger.md) | the messages received less than `d` ago (WITHIN), to the second |
| [larger](larger.md) | the messages of more octets than `octets` (RFC822.SIZE) |
| [smaller](smaller.md) | the messages of fewer octets than `octets` |
| [uid](uid.md) | the messages whose UIDs are in the set |
| [numbers](numbers.md) | the messages whose sequence numbers are in the set |
| [modseq](modseq.md) | the messages whose mod-sequence is `n` or past it (CONDSTORE, RFC 7162) |

## Non-member functions

| Function | Description |
|---|---|
| [operator&&, operator\|\|, operator!](operator_logical.md) | criteria combined |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    using net::imap::criteria;
    criteria wanted = criteria::unseen() && (criteria::from("alice") || criteria::larger(1 << 20));
    println("{}", wanted.to_string());
}
```

Output:

```text
UNSEEN OR FROM "alice" LARGER 1048576
```

## See also

- [search](../client/search.md), [count](../client/count.md), [sort](../client/sort.md)
- [sequence_set](../sequence_set/README.md)
