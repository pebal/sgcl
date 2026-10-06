[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::sign

```cpp
expected<string, io::error> sign(const string& message) const;                           // (1)
expected<string, io::error> sign(const string& message, const sign_options& o) const;    // (2)
```

The message with a `DKIM-Signature` field put at its head (RFC 6376 §5): the body canonicalized and hashed, the
fields named in `h=` canonicalized and hashed with the new field itself, the hash signed with the signer's key. The
fields signed are those of RFC 6376 §5.4.1 the message has — From, To, Cc, Subject, Date, Reply-To, the Resent and
List fields, In-Reply-To and References — with Message-ID, MIME-Version and the content fields, each named once more
than the message has it (`oversign`), so that a second From or Subject cannot be added after; or the fields the
options name.

1. Relaxed both ways, the recommended fields, `t=` now.
2. As the [sign_options](../sign_options.md) say.

The field is folded at 76 columns. A message of LF lines is signed as its CRLF form, which is what it returns.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message's text: its head, an empty line, its body |
| `o` | the canonicalizations, the fields, `i=`, `l=`, `t=`, `x=` |

## Return value

The signed message; `net::errc::malformed_message` for a message without a head, without From, with a line of the
head that is no field, or a body shorter than `l=`; `net::errc::invalid_address` for an identity outside the
signer's domain.

## Complexity

Linear in the message, and one signature by the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::from_seed(crypto::sha256::of("the example's key"));
    net::dkim::signer s("example.com", "s2026", key->to_pem());
    net::dkim::sign_options o;
    o.time = time::datetime::from_unix(1791244800, time::zone::utc());
    string m = "From: alice@example.com\r\nTo: bob@example.org\r\nSubject: Hi\r\n\r\nHello, Bob.\r\n";
    string signed_message = s.sign(m, o).value();
    for (auto line : signed_message.split("\r\n")) {
        println("{}", line);
    }
}
```

Output:

```text
DKIM-Signature: v=1; a=ed25519-sha256; c=relaxed/relaxed; d=example.com;
	s=s2026; t=1791244800; h=from:from:subject:subject:to:to;
	bh=IYs6mAcukH6WZP9HzM2oRWIYtAx5T8CyTDPPmbYwHso=; b=GBjlF4ExpZRTvDhla
	 RraTCb3PjwykRdnEWremtjRIJ8X8cPyTOodQWCpr5jZCQSu0I4Fuoq6MMcMAU9zRqeE
	 CA==
From: alice@example.com
To: bob@example.org
Subject: Hi

Hello, Bob.

```

## See also

- [verify](../verify.md)
- [sign_options](../sign_options.md)
- [smtp::options](../../smtp/options.md): `dkim`, a client that signs everything it sends
- [signer](README.md)
