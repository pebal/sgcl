[sgcl](../../README.md) › [net](../README.md) › dns_sd

# sgcl::net::dns_sd

```cpp
#include "sgcl/net/mdns.h"   // namespace sgcl::net::dns_sd, or "sgcl/net.h"
```

DNS-based service discovery, RFC 6763, over multicast DNS ([mdns](../mdns/README.md), RFC 6762): the services of the
link found and published by name and type, with no server and no configuration, the way printers, speakers and
development servers find each other. A service instance is a name (`"Living Room"`) of a type (`"_airplay._tcp"`)
in a domain (`local.`); a PTR of the type lists its instances, an SRV of the instance names its host and port, a TXT
carries its settings as keys and values. Go's standard library has none of it; Apple's `dns-sd` and Avahi are the
system's, and the module is the peer itself, both ways, beside them.

The idea it rests on is that discovery is a stream and a publication is a handle. [browse](browse.md) gives a
[browser](browser/README.md) whose `next` yields an [event](event.md) as an instance comes or goes, for as long as it
is open; [resolve](resolve.md) turns an instance into a [service](service.md) with its host, port, TXT and the host's
addresses; [types](types.md) lists the types of the link. [publish](publish.md) gives the
[mdns::responder](../mdns-responder/README.md) that holds the service on the link until it is closed.

## The rules

1. The domain is `local.` unless a service or a type says another; a type is `_name._tcp` or `_name._udp` (the name
   1 to 15 letters, digits and `-`, RFC 6335), a subtype is browsed as `"_printer._sub._ipp._tcp"` (§7.1), and `"_services._dns-sd._udp"` browses the types themselves (§9).
2. A browse asks again and again (one second, then twice each time, up to an hour), lists the instances it knows so
   that responders leave them out, and asks again before their records run out; an instance is gone when its
   goodbye comes (a second later, RFC 6762 §10.1) or its record expires.
3. An instance's name is any UTF-8 of at most 63 bytes, dots and spaces among them; a TXT record holds keys of
   printable ASCII without `=`, each once without regard to case, each entry at most 255 bytes ([txt_record](txt_record/README.md)).
4. The functions take [mdns::options](../mdns-options.md): the interfaces and families, the wait of a resolve or the
   types (2 s by default), the QU question, a responder's host name.
5. The blocking forms run on the scheduler and wait on the calling thread, so they are for a thread, as
   [task::wait](../../async/task/wait.md) is; a task awaits the `async_` forms. A browser and a responder are
   handles of one word: a copy is the same browse, the same responder.

## Functions

| Function | Header | Description |
|---|---|---|
| [browse](browse.md) | `mdns.h` | the instances of a service type as they come and go |
| [publish, async_publish](publish.md) | `mdns.h` | a service published by a responder of its own |
| [resolve, async_resolve](resolve.md) | `mdns.h` | an instance's host, port, TXT record and addresses |
| [types, async_types](types.md) | `mdns.h` | the service types of the link |

## Classes

| Class | Header | Description |
|---|---|---|
| [browser](browser/README.md) | `mdns.h` | a browse: the events of a service type, waited for one by one |
| [event](event.md) | `mdns.h` | what a browse saw: an instance come or gone |
| [service](service.md) | `mdns.h` | a service instance: its name, type, host, port, TXT record, subtypes, addresses |
| [txt_record](txt_record/README.md) | `mdns.h` | a TXT record of DNS-SD: keys with values or alone |

## See also

- [mdns](../mdns/README.md): the names of the link and the responder
- [dns::lookup_srv](../dns/lookup_srv.md): an instance's SRV through the resolver (a name under `.local` goes to
  multicast DNS)
- [The net module](../README.md)
