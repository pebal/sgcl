// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl/net/ip.h: Go's net/netip asked the same questions,
// its answers written out as a C++ header the test includes. Run it from
// the root of the tree:
//
//     go run tools/ip_oracle.go > tests/net/ip_oracle.h
//
// The corpus is the examples of RFC 4291 §2.2 and RFC 5952 §2 and §4,
// the edges written by hand (leading zeros, the embedded IPv4 address in
// every position, zones, the ranges of every predicate), a few thousand
// addresses drawn at random and written in several spellings, and random
// one-character edits of all of them, which is where a parser that
// accepts too much shows itself. A fixed seed: the header is the same on
// every run.
//
// What is asked, of each kind:
//
//   - an address (ParseAddr): whether it parses; String(); Is4, Is6,
//     Is4In6, IsLoopback, IsPrivate, IsUnspecified, IsMulticast,
//     IsLinkLocalUnicast, IsGlobalUnicast; Next() and Prev();
//   - an address and a port (ParseAddrPort): whether it parses; String();
//   - a network (ParsePrefix): whether it parses; String(); Masked();
//   - Contains of an address in a network and Overlaps of two networks;
//   - Compare of two addresses.
//
// Two differences, left in and named rather than bent:
//
//   - a zone longer than fifteen bytes. Go keeps the zone as an interned
//     string of any length; here it lives inside the thirty-two bytes of
//     the value, so such an address does not parse. Those cases carry a
//     flag, and the test checks that the parse fails for them.
//   - the unspecified address with a zone, "::%en0". Go's IsUnspecified
//     compares the whole value with IPv6Unspecified(), zone included, so
//     it says false there (and IsGlobalUnicast true); here the predicates
//     look at the address and not at the zone, as they do for every other
//     address in both. The predicates are asked of the address without its
//     zone, which is the same question for everything else.
package main

import (
	"fmt"
	"math/rand"
	"net/netip"
	"os"
	"sort"
	"strconv"
	"strings"
)

var handAddresses = []string{
	// RFC 4291 §2.2
	"ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", "2001:DB8:0:0:8:800:200C:417A", "FF01:0:0:0:0:0:0:101",
	"0:0:0:0:0:0:0:1", "0:0:0:0:0:0:0:0", "2001:DB8::8:800:200C:417A", "FF01::101", "::1", "::",
	"0:0:0:0:0:0:13.1.68.3", "0:0:0:0:0:FFFF:129.144.52.38", "::13.1.68.3", "::FFFF:129.144.52.38",
	// RFC 5952 §2 (one address, many spellings) and §4 (the rules)
	"2001:db8:0:0:1:0:0:1", "2001:0db8:0:0:1:0:0:1", "2001:db8::1:0:0:1", "2001:db8::0:1:0:0:1",
	"2001:0db8::1:0:0:1", "2001:db8:0:0:1::1", "2001:db8:0000:0:1::1", "2001:DB8:0:0:1::1",
	"2001:db8:aaaa:bbbb:cccc:dddd:eeee:0001", "2001:db8:aaaa:bbbb:cccc:dddd:eeee:001",
	"2001:db8:aaaa:bbbb:cccc:dddd:eeee:01", "2001:db8:aaaa:bbbb:cccc:dddd:eeee:1",
	"2001:db8:aaaa:bbbb:cccc:dddd::1", "2001:db8:aaaa:bbbb:cccc:dddd:0:1", "2001:db8:0:0:0::1",
	"2001:db8:0:0::1", "2001:db8:0::1", "2001:db8::1", "2001:db8::aaaa:0:0:1", "2001:db8:0:0:aaaa::1",
	"2001:db8:0:0:0:0:2:1", "2001:db8::0:1", "2001:db8:0:1:1:1:1:1", "2001:0:0:1:0:0:0:1",
	"2001:db8:0:0:1:0:0:1", "2001:DB8::1", "2001:db8::ABCD",
	// IPv4: the dotted form only, no leading zeros
	"0.0.0.0", "1.2.3.4", "255.255.255.255", "127.0.0.1", "010.0.0.1", "1.02.3.4", "00.0.0.0",
	"1.2.3.00", "256.0.0.1", "1.256.0.0", "1.2.3", "1.2.3.4.", ".1.2.3.4", "1..2.3", "1.2.3.4.5",
	" 1.2.3.4", "1.2.3.4 ", "1.2.3.4%x", "0x7f.0.0.1", "127.1", "1.2.3.-4", "+1.2.3.4", "1.2.3.4a",
	"1000.0.0.1", "01.1.1.1", "1.2.3.255", "1.2.3.2555",
	// IPv6: the shapes that must fail
	"1:2:3:4:5:6:7:8:9", "1:2:3:4:5:6:7", ":1:2:3:4:5:6:7", "1:2:3:4:5:6:7:", "12345::", "::12345",
	"1::2::3", ":::", "::::", "1:::2", "1:2:3:4:5:6:7:8::", "::1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7::",
	"::1:2:3:4:5:6:7", "g::1", "1:2:3:4:5:6:7:g", "[::1]", "::1]", "[::1", "", "%en0", ":", "1:",
	"::1 ", " ::1", "1.2.3.4:80", "0000:0000:0000:0000:0000:0000:0000:0000", "00000::",
	"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF",
	// the embedded IPv4 address, in every position
	"::ffff:1.2.3", "::ffff:1.2.3.04", "::ffff:0:0", "::ffff:0.0.0.0", "64:ff9b::1.2.3.4",
	"1:2:3:4:5:6::1.2.3.4", "1:2:3:4:5::1.2.3.4", "::1:2:3:4:5:1.2.3.4", "::1:2:3:4:5:6:1.2.3.4",
	"1:2:3:4:5:6:7:1.2.3.4", "1:2:3:4:5:6:1.2.3.4", "1.2.3.4::", "::1.2.3.4:1", "::1.2.3.4.5",
	"::ffff:1.2.3.4%en0", "::1.2.3.4", "::0.0.0.1", "::ffff:255.255.255.255", "::ffff:1.2.3.4",
	"0:0:0:0:0:ffff:1.2.3.4", "::FFFF:1.2.3.4", "::ffff:a.2.3.4", "::ffff:1234.2.3.4",
	// zones
	"fe80::1%", "fe80::1%en0", "fe80::1%a%b", "fe80::1%a:b", "fe80::1%1", "fe80::1%123456789012345",
	"fe80::1%1234567890123456", "fe80::1%%", "fe80::%en0", "::%en0", "::1%lo0", "ff02::1%en0",
	"fe80::1%en0%", "fe80::1% ", "1.2.3.4%en0",
	// the ranges of the predicates
	"127.255.255.255", "126.255.255.255", "128.0.0.0", "10.0.0.0", "9.255.255.255", "11.0.0.0",
	"172.15.0.1", "172.16.0.1", "172.31.255.255", "172.32.0.1", "192.168.1.1", "192.169.1.1",
	"192.167.255.255", "169.254.0.1", "169.253.0.1", "169.255.0.1", "224.0.0.1", "224.0.1.1",
	"239.255.255.255", "240.0.0.1", "223.255.255.255", "fc00::1", "fd12::1", "fbff::1", "fe00::1",
	"fe80::", "febf:ffff::1", "fec0::1", "ff00::", "ff02::1", "ff02::2", "ff05::1", "ff12::1",
	"ff01::1", "::2", "::ffff:127.0.0.1", "::ffff:10.1.1.1", "::ffff:169.254.1.1", "::ffff:224.0.0.1",
	"::ffff:192.168.0.1", "::ffff:8.8.8.8", "8.8.8.8", "2001:4860:4860::8888", "::ffff:0:1",
	// Next and Prev at the edges
	"0.0.0.1", "1.2.3.255", "1.2.255.255", "::ffff:ffff", "::fffe:ffff:ffff", "::ffff:0:0",
	"ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe", "::ffff", "::1:0",
}

var handPorts = []string{
	"1.2.3.4:80", "1.2.3.4:0", "1.2.3.4:65535", "1.2.3.4:65536", "1.2.3.4:080", "1.2.3.4:+80",
	"1.2.3.4:-1", "1.2.3.4:", "1.2.3.4", ":80", "[::1]:80", "[::1]:", "[::1]", "::1:80", "[1.2.3.4]:80",
	"[fe80::1%en0]:65535", "[fe80::1%en0]80", "[::ffff:1.2.3.4]:443", "[::1]]:80", "[[::1]]:80",
	"1.2.3.4:80:80", "[::1]:80:80", "[]:80", "[::1:80", "::1]:80", "1.2.3.4:0x50", "1.2.3.4: 80",
	"1.2.3.4:00000000000080", "1.2.3.4:99999999999999999999", "localhost:80", "010.0.0.1:80",
}

var handPrefixes = []string{
	"10.0.0.0/8", "10.1.2.3/8", "10.0.0.0/08", "10.0.0.0/+8", "10.0.0.0/-1", "10.0.0.0/", "10.0.0.0",
	"10.0.0.0/33", "10.0.0.0/32", "10.0.0.0/0", "0.0.0.0/0", "2001:db8::/32", "2001:db8::1/128",
	"2001:db8::1/129", "::/0", "fe80::1%en0/64", "::ffff:1.2.3.4/100", "::ffff:1.2.3.4/96",
	"1.2.3.4/1000", "1.2.3.4/32/1", "/8", "1.2.3.4/ 8", "1.2.3.4/8 ", "1.2.3.4/00", "1.2.3.4/7a",
	"255.255.255.255/31", "128.0.0.0/1", "fe80::/10", "ff00::/8", "::1/127",
}

func quote(s string) string {
	// The corpus is printable ASCII: Go's quoting is C's there
	return strconv.Quote(s)
}

func flagsOf(a netip.Addr) []string {
	var f []string
	add := func(b bool, name string) {
		if b {
			f = append(f, name)
		}
	}
	add(a.Is4(), "V4")
	add(a.Is6(), "V6")
	add(a.Is4In6(), "Mapped")
	add(a.IsLoopback(), "Loopback")
	add(a.IsPrivate(), "Private")
	add(a.IsUnspecified(), "Unspecified")
	add(a.IsMulticast(), "Multicast")
	add(a.IsLinkLocalUnicast(), "LinkLocal")
	add(a.IsGlobalUnicast(), "GlobalUnicast")
	if len(f) == 0 {
		return []string{"0"}
	}
	return f
}

// An address drawn at random: IPv4 a quarter of the time, IPv6 with each
// group zero half the time (so that runs of zeros of every length and
// position come up), IPv4-mapped now and then
func randomAddr(r *rand.Rand) netip.Addr {
	switch r.Intn(8) {
	case 0, 1:
		var b [4]byte
		for i := range b {
			if r.Intn(3) > 0 {
				b[i] = byte(r.Intn(256))
			}
		}
		return netip.AddrFrom4(b)
	case 2:
		var b [4]byte
		for i := range b {
			b[i] = byte(r.Intn(256))
		}
		return netip.AddrFrom16(netip.AddrFrom4(b).As16())
	default:
		var b [16]byte
		for g := 0; g < 8; g++ {
			if r.Intn(2) == 0 {
				v := uint16(r.Intn(65536))
				if r.Intn(3) == 0 {
					v = uint16(r.Intn(16))
				}
				b[2*g] = byte(v >> 8)
				b[2*g+1] = byte(v)
			}
		}
		return netip.AddrFrom16(b)
	}
}

// The same address in another spelling: every group in full with its
// leading zeros, upper case, or no "::" at all
func respell(a netip.Addr, r *rand.Rand) string {
	if a.Is4() {
		return a.String()
	}
	b := a.As16()
	parts := make([]string, 8)
	switch r.Intn(3) {
	case 0:
		for g := range parts {
			parts[g] = fmt.Sprintf("%04x", uint16(b[2*g])<<8|uint16(b[2*g+1]))
		}
	case 1:
		for g := range parts {
			parts[g] = fmt.Sprintf("%X", uint16(b[2*g])<<8|uint16(b[2*g+1]))
		}
	default:
		for g := range parts {
			parts[g] = fmt.Sprintf("%x", uint16(b[2*g])<<8|uint16(b[2*g+1]))
		}
	}
	return strings.Join(parts, ":")
}

const editAlphabet = "0123456789abcdefABCDEFgG.:%[]/ -+x"

// One character inserted, removed or replaced
func mutate(s string, r *rand.Rand) string {
	c := string(editAlphabet[r.Intn(len(editAlphabet))])
	if len(s) == 0 {
		return c
	}
	i := r.Intn(len(s))
	switch r.Intn(3) {
	case 0:
		return s[:i] + c + s[i:]
	case 1:
		return s[:i] + s[i+1:]
	default:
		return s[:i] + c + s[i+1:]
	}
}

func uniq(in []string) []string {
	seen := map[string]bool{}
	var out []string
	for _, s := range in {
		if !seen[s] {
			seen[s] = true
			out = append(out, s)
		}
	}
	return out
}

func main() {
	r := rand.New(rand.NewSource(20260924))
	w := os.Stdout

	// Addresses
	corpus := append([]string{}, handAddresses...)
	var drawn []netip.Addr
	for i := 0; i < 1200; i++ {
		a := randomAddr(r)
		drawn = append(drawn, a)
		corpus = append(corpus, a.String(), respell(a, r))
	}
	base := append([]string{}, corpus...)
	for i := 0; i < 2500; i++ {
		corpus = append(corpus, mutate(base[r.Intn(len(base))], r))
	}
	corpus = uniq(corpus)

	fmt.Fprintln(w, "// Generated by tools/ip_oracle.go from Go's net/netip; do not edit.")
	fmt.Fprintln(w, "#pragma once")
	fmt.Fprintln(w)
	fmt.Fprintln(w, "namespace ip_oracle {")
	fmt.Fprintln(w, "    enum : unsigned { V4 = 1, V6 = 2, Mapped = 4, Loopback = 8, Private = 16, Unspecified = 32, Multicast = 64, LinkLocal = 128, GlobalUnicast = 256, ZoneTooLong = 512 };")
	fmt.Fprintln(w)
	fmt.Fprintln(w, "    struct Address { const char* input; bool ok; const char* text; unsigned flags; const char* next; const char* prev; };")
	fmt.Fprintln(w, "    inline constexpr Address addresses[] = {")
	var parsed []netip.Addr
	for _, s := range corpus {
		a, err := netip.ParseAddr(s)
		if err != nil {
			fmt.Fprintf(w, "        {%s, false, \"\", 0, \"\", \"\"},\n", quote(s))
			continue
		}
		flags := flagsOf(a.WithZone(""))
		if len(a.Zone()) > 15 {
			flags = append(flags, "ZoneTooLong")
		} else {
			parsed = append(parsed, a)
		}
		fmt.Fprintf(w, "        {%s, true, %s, %s, %s, %s},\n", quote(s), quote(a.String()), strings.Join(flags, " | "), quote(a.Next().String()), quote(a.Prev().String()))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// Addresses and ports
	ports := append([]string{}, handPorts...)
	for i := 0; i < 400; i++ {
		a := drawn[r.Intn(len(drawn))]
		p := netip.AddrPortFrom(a, uint16(r.Intn(65536)))
		ports = append(ports, p.String())
	}
	pbase := append([]string{}, ports...)
	for i := 0; i < 800; i++ {
		ports = append(ports, mutate(pbase[r.Intn(len(pbase))], r))
	}
	ports = uniq(ports)
	fmt.Fprintln(w, "    struct Endpoint { const char* input; bool ok; const char* text; };")
	fmt.Fprintln(w, "    inline constexpr Endpoint endpoints[] = {")
	for _, s := range ports {
		p, err := netip.ParseAddrPort(s)
		if err != nil {
			fmt.Fprintf(w, "        {%s, false, \"\"},\n", quote(s))
			continue
		}
		if len(p.Addr().Zone()) > 15 {
			continue
		}
		fmt.Fprintf(w, "        {%s, true, %s},\n", quote(s), quote(p.String()))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// Networks
	prefixes := append([]string{}, handPrefixes...)
	var drawnPrefixes []netip.Prefix
	for i := 0; i < 400; i++ {
		a := drawn[r.Intn(len(drawn))]
		bits := r.Intn(a.BitLen() + 1)
		p := netip.PrefixFrom(a, bits)
		drawnPrefixes = append(drawnPrefixes, p)
		prefixes = append(prefixes, p.String())
	}
	qbase := append([]string{}, prefixes...)
	for i := 0; i < 800; i++ {
		prefixes = append(prefixes, mutate(qbase[r.Intn(len(qbase))], r))
	}
	prefixes = uniq(prefixes)
	fmt.Fprintln(w, "    struct Network { const char* input; bool ok; const char* text; const char* masked; };")
	fmt.Fprintln(w, "    inline constexpr Network networks[] = {")
	for _, s := range prefixes {
		p, err := netip.ParsePrefix(s)
		if err != nil {
			fmt.Fprintf(w, "        {%s, false, \"\", \"\"},\n", quote(s))
			continue
		}
		fmt.Fprintf(w, "        {%s, true, %s, %s},\n", quote(s), quote(p.String()), quote(p.Masked().String()))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// Contains: a network and an address near it (inside, just past its
	// edges) or anywhere; the address may be the other kind, or mapped
	fmt.Fprintln(w, "    struct Contains { const char* network; const char* address; bool result; };")
	fmt.Fprintln(w, "    inline constexpr Contains contains[] = {")
	for i := 0; i < 800; i++ {
		p := drawnPrefixes[r.Intn(len(drawnPrefixes))]
		var a netip.Addr
		switch r.Intn(5) {
		case 0:
			a = p.Masked().Addr()
		case 1:
			a = p.Masked().Addr().Prev()
		case 2:
			// the last address of the network: the host bits set
			b := p.Masked().Addr().AsSlice()
			for bit := p.Bits(); bit < len(b)*8; bit++ {
				b[bit/8] |= 0x80 >> (bit % 8)
			}
			a, _ = netip.AddrFromSlice(b)
			if r.Intn(2) == 0 {
				a = a.Next()
			}
		case 3:
			a = drawn[r.Intn(len(drawn))]
		default:
			a = p.Addr()
			if a.Is4() && r.Intn(2) == 0 {
				a = netip.AddrFrom16(a.As16())
			}
		}
		if !a.IsValid() {
			continue
		}
		fmt.Fprintf(w, "        {%s, %s, %v},\n", quote(p.String()), quote(a.String()), p.Contains(a))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	fmt.Fprintln(w, "    struct Overlaps { const char* a; const char* b; bool result; };")
	fmt.Fprintln(w, "    inline constexpr Overlaps overlaps[] = {")
	for i := 0; i < 600; i++ {
		p := drawnPrefixes[r.Intn(len(drawnPrefixes))]
		var q netip.Prefix
		if r.Intn(2) == 0 {
			q = drawnPrefixes[r.Intn(len(drawnPrefixes))]
		} else {
			bits := r.Intn(p.Addr().BitLen() + 1)
			q = netip.PrefixFrom(p.Addr(), bits)
		}
		fmt.Fprintf(w, "        {%s, %s, %v},\n", quote(p.String()), quote(q.String()), p.Overlaps(q))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// Compare: pairs of the addresses parsed, sorted as Go sorts them
	sample := make([]netip.Addr, 0, 300)
	for i := 0; i < 300; i++ {
		sample = append(sample, parsed[r.Intn(len(parsed))])
	}
	sort.Slice(sample, func(i, j int) bool { return sample[i].Less(sample[j]) })
	fmt.Fprintln(w, "    // Sorted by Go's Addr.Less: every adjacent pair compares as Go's Compare says")
	fmt.Fprintln(w, "    struct Ordered { const char* address; int compare_to_next; };")
	fmt.Fprintln(w, "    inline constexpr Ordered ordered[] = {")
	for i, a := range sample {
		c := 0
		if i+1 < len(sample) {
			c = a.Compare(sample[i+1])
		}
		fmt.Fprintf(w, "        {%s, %d},\n", quote(a.String()), c)
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "}")
}
