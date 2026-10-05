[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::errc

```cpp
#include "sgcl/net/acme/error.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    enum class errc {
        account_does_not_exist = 1, already_revoked, bad_csr, bad_nonce, bad_public_key,
        bad_revocation_reason, bad_signature_algorithm, caa, compound, connection, dns,
        external_account_required, incorrect_response, invalid_contact, malformed, order_not_ready,
        rate_limited, rejected_identifier, server_internal, tls, unauthorized, unsupported_contact,
        unsupported_identifier, user_action_required, already_replaced, invalid_profile,
        unknown_problem, malformed_response, unsupported, order_invalid, authorization_invalid,
        no_challenge, host_not_allowed
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::acme::errc> : std::true_type {};
```

The codes of the category `"acme"` ([category](category.md)): the error types of RFC 8555 §6.7 by their names (a
problem document's `urn:ietf:params:acme:error:rateLimited` is `rate_limited`), RFC 9773's and the profiles', and the
client's own. An `io::error` compares with them, `e.code() == net::acme::errc::rate_limited`.

| Value | Description |
|---|---|
| `account_does_not_exist` | the account does not exist (accountDoesNotExist) |
| `already_revoked` | the certificate is already revoked |
| `bad_csr` | the CSR is unacceptable: other names, the account's own key |
| `bad_nonce` | the nonce is unacceptable (the client sends again, 5 times by default) |
| `bad_public_key` | the JWS key is not supported |
| `bad_revocation_reason` | the revocation reason is not allowed |
| `bad_signature_algorithm` | the JWS algorithm is not supported |
| `caa` | CAA records forbid the CA from issuing |
| `compound` | several errors, in the subproblems |
| `connection` | the CA could not connect to the validation target |
| `dns` | a DNS query failed during validation |
| `external_account_required` | the CA requires an external account binding |
| `incorrect_response` | the response to the challenge was incorrect |
| `invalid_contact` | a contact URL is invalid |
| `malformed` | the request is malformed |
| `order_not_ready` | the order is not ready to be finalized |
| `rate_limited` | rate limited |
| `rejected_identifier` | the CA will not issue for the identifier; the client's refusal of a name that cannot be one |
| `server_internal` | the CA had an internal error |
| `tls` | a TLS error during validation |
| `unauthorized` | unauthorized |
| `unsupported_contact` | a contact URL of an unsupported scheme |
| `unsupported_identifier` | an identifier of an unsupported type |
| `user_action_required` | visit the problem's instance URL |
| `already_replaced` | the certificate was already replaced (RFC 9773) |
| `invalid_profile` | the profile is not one the CA offers |
| `unknown_problem` | a problem document of a type not in this list |
| `malformed_response` | a response that breaks RFC 8555 |
| `unsupported` | a resource the CA's directory does not offer (keyChange, renewalInfo, revokeCert) |
| `order_invalid` | an order that ended invalid |
| `authorization_invalid` | an authorization that ended invalid |
| `no_challenge` | no challenge of a type the client or the manager solves |
| `host_not_allowed` | a name the manager's names and host policy do not allow |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    ca.fail_next(net::acme::errc::rate_limited, 1, "new-order", std::chrono::hours(1));
    auto refused = acme.new_order({"example.com"});
    println("{}", refused.error().code() == net::acme::errc::rate_limited);
    println("{}", refused.error().message());
}
```

Output:

```text
true
acme new-order a fault the test asked for (retry after 3600 s): acme: rate limited
```

## See also

- [category](category.md), [make_error_code](make_error_code.md)
- [problem](problem/README.md)
- [net::acme](README.md)
