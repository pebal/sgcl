// LDAP in Go's standard library alone: Go has no LDAP (go-ldap is not taken),
// so this side is a minimal client by hand from RFC 4511 — the requests
// written as BER, the responses read by encoding/asn1 into RawValues —
// against bench_ldap's server (benchmarks/net/ldap.cpp has the SGCL side,
// the same cases). Prints one line, ns per operation.
//
//	ldap_bind ADDR [n]     a simple bind and its answer: per bind
//	ldap_search ADDR [n]   a search of 100 entries, each read into a DN and attributes: per entry
//	ldap_filter [n]        the same filter of 5 terms written as BER: per filter (by hand, no parser: the
//	                       lower bound of what a parser does)
package main

import (
	"bufio"
	"encoding/asn1"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"time"
)

func report(what string, d time.Duration, n int) {
	fmt.Printf("ldap %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

func tlv(tag byte, content []byte) []byte {
	out := []byte{tag}
	n := len(content)
	switch {
	case n < 128:
		out = append(out, byte(n))
	case n < 256:
		out = append(out, 0x81, byte(n))
	case n < 65536:
		out = append(out, 0x82, byte(n>>8), byte(n))
	default:
		out = append(out, 0x84, byte(n>>24), byte(n>>16), byte(n>>8), byte(n))
	}
	return append(out, content...)
}

func octets(s string) []byte { return tlv(0x04, []byte(s)) }

func integer(v int) []byte {
	b := []byte{}
	for v > 127 || len(b) == 0 {
		b = append([]byte{byte(v)}, b...)
		v >>= 8
		if v == 0 {
			break
		}
	}
	if b[0]&0x80 != 0 {
		b = append([]byte{0}, b...)
	}
	return tlv(0x02, b)
}

func cat(parts ...[]byte) []byte {
	var out []byte
	for _, p := range parts {
		out = append(out, p...)
	}
	return out
}

func message(id int, op []byte) []byte { return tlv(0x30, cat(integer(id), op)) }

func filter() []byte {
	eq := func(a, v string) []byte { return tlv(0xa3, cat(octets(a), octets(v))) }
	sub := tlv(0xa4, cat(octets("cn"), tlv(0x30, cat(tlv(0x80, []byte("Jo")), tlv(0x81, []byte("n"))))))
	not := tlv(0xa2, tlv(0xa4, cat(octets("mail"), tlv(0x30, tlv(0x82, []byte("@spam.example"))))))
	ge := tlv(0xa5, cat(octets("age"), octets("30")))
	return tlv(0xa0, cat(eq("objectClass", "person"), tlv(0xa1, cat(eq("sn", "Smith"), sub)), not, ge))
}

// one LDAPMessage off the reader
func read(r *bufio.Reader) (asn1.RawValue, error) {
	var head [6]byte
	if _, err := io.ReadFull(r, head[:2]); err != nil {
		return asn1.RawValue{}, err
	}
	n := int(head[1])
	hl := 2
	if n&0x80 != 0 {
		k := n & 0x7f
		if _, err := io.ReadFull(r, head[2:2+k]); err != nil {
			return asn1.RawValue{}, err
		}
		n = 0
		for i := 0; i < k; i++ {
			n = n<<8 | int(head[2+i])
		}
		hl += k
	}
	buf := make([]byte, hl+n)
	copy(buf, head[:hl])
	if _, err := io.ReadFull(r, buf[hl:]); err != nil {
		return asn1.RawValue{}, err
	}
	var raw asn1.RawValue
	_, err := asn1.UnmarshalWithParams(buf, &raw, "")
	return raw, err
}

type attribute struct {
	Type []byte
	Vals [][]byte `asn1:"set"`
}

type entry struct {
	DN    []byte
	Attrs []attribute
}

func main() {
	if os.Args[1] == "ldap_filter" {
		n := count(2, 500000)
		total := 0
		start := time.Now()
		for i := 0; i < n; i++ {
			total += len(filter())
		}
		report("ldap_filter", time.Since(start), n)
		if total == 0 {
			os.Exit(1)
		}
		return
	}
	conn, err := net.Dial("tcp", os.Args[2])
	if err != nil {
		panic(err)
	}
	r := bufio.NewReaderSize(conn, 65536)
	id := 0
	bind := func() bool {
		id++
		conn.Write(message(id, tlv(0x60, cat(integer(3), octets("cn=admin,dc=example,dc=com"), tlv(0x80, []byte("secret"))))))
		m, err := read(r)
		if err != nil {
			return false
		}
		var parts []asn1.RawValue
		rest := m.Bytes
		for len(rest) > 0 {
			var v asn1.RawValue
			rest, err = asn1.Unmarshal(rest, &v)
			if err != nil {
				return false
			}
			parts = append(parts, v)
		}
		return len(parts) == 2 && parts[1].Tag == 1
	}
	switch os.Args[1] {
	case "ldap_bind":
		n := count(3, 50000)
		start := time.Now()
		for i := 0; i < n; i++ {
			if !bind() {
				os.Exit(1)
			}
		}
		report("ldap_bind", time.Since(start), n)
	case "ldap_search":
		n := count(3, 2000)
		total := 0
		start := time.Now()
		for i := 0; i < n; i++ {
			id++
			req := tlv(0x63, cat(octets("ou=people,dc=example,dc=com"), tlv(0x0a, []byte{2}), tlv(0x0a, []byte{0}), integer(0), integer(0),
				tlv(0x01, []byte{0}), tlv(0xa3, cat(octets("objectClass"), octets("person"))), tlv(0x30, nil)))
			conn.Write(message(id, req))
			got := 0
			for {
				m, err := read(r)
				if err != nil {
					os.Exit(1)
				}
				var idv int
				rest, _ := asn1.Unmarshal(m.Bytes, &idv)
				var op asn1.RawValue
				asn1.Unmarshal(rest, &op)
				if op.Tag == 5 {
					break
				}
				// SearchResultEntry: the DN and the attributes, read
				var e entry
				if _, err := asn1.UnmarshalWithParams(op.FullBytes, &e, "application,tag:4"); err != nil {
					fmt.Fprintln(os.Stderr, err)
					os.Exit(1)
				}
				got++
			}
			total += got
		}
		report("ldap_search", time.Since(start), total)
	}
	conn.Write(message(id+1, []byte{0x42, 0x00}))
	conn.Close()
}
