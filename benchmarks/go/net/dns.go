// The DNS cases of the net benchmark (benchmarks/net/net.cpp has the SGCL
// side, the same cases and bytes).
//
//	dns_parse [n]   a DNS response read as a stub resolver reads it: MX of five,
//	                TXT of three, SRV of three in turn, per message
//	dns_lookup [n]  net.Resolver (PreferGo, its Dial to a server on the loopback
//	                that answers from memory) LookupMX of five records, per lookup
//
// Go's own parser of DNS messages is internal to net (its vendored copy of
// golang.org/x/net/dns/dnsmessage), and the benchmarks take nothing from
// outside the standard library, so dns_parse reads the message with the
// decoder below, written here from RFC 1035 for the same work the SGCL side
// does: the id and the question checked, every section walked (names with
// their compression pointers), the OPT's extended RCODE, the records of the
// type asked decoded into strings as net.MX, net.SRV and LookupTXT give them.
// dns_lookup is Go's whole resolver, its own parser inside.
package main

import (
	"context"
	"encoding/hex"
	"errors"
	"net"
	"strings"
	"time"
)

var dnsResponses = []string{
	"123481800001000500000001076578616d706c6503636f6d00000f0001c00c000f000100000e10001b00050d676d61696c2d736d74702d696e016c06676f6f676c65c014c00c000f000100000e100009000f04616c7431c02bc00c000f000100000e100009001904616c7432c02bc00c000f000100000e100009002304616c7433c02bc00c000f000100000e100009002d04616c7434c02b00002904d0000000000000",
	"123481800001000300000001076578616d706c6503636f6d0000100001c00c001000010000012c002524763d7370663120696e636c7564653a5f7370662e6578616d706c652e636f6d207e616c6cc00c001000010000012c004544676f6f676c652d736974652d766572696669636174696f6e3d7744384e3769314a544e546b657a4a34397377765757343866385f39787665524556346f422d304866356fc00c001000010000012c002c2b4d533d4534413638423941423242423936373042434531353431324636323931363136344330423230424200002904d0000000000000",
	"1234818000010003000000010c5f786d70702d736572766572045f746370076578616d706c6503636f6d0000210001c00c002100010000012c001900050032149505786d707031076578616d706c6503636f6d00c00c002100010000012c001900050032149505786d707032076578616d706c6503636f6d00c00c002100010000012c001f000a000014950b786d70702d6261636b7570076578616d706c65036e65740000002904d0000000000000",
}

var errDNS = errors.New("bad message")

type dnsRecord struct {
	name   string
	text   string
	first  uint16
	second uint16
	third  uint16
}

func u16(m []byte, at int) uint16 {
	return uint16(m[at])<<8 | uint16(m[at+1])
}

// The name at pos, as text with a dot after each label; the position after it
func readName(m []byte, pos int, build bool) (string, int, error) {
	var sb strings.Builder
	p, limit, after, jumped, total := pos, pos, 0, false, 0
	for {
		if p >= len(m) {
			return "", 0, errDNS
		}
		l := int(m[p])
		switch {
		case l == 0:
			if !jumped {
				after = p + 1
			}
			if total == 0 && build {
				sb.WriteByte('.')
			}
			return sb.String(), after, nil
		case l&0xC0 == 0xC0:
			if p+1 >= len(m) {
				return "", 0, errDNS
			}
			t := (l&0x3F)<<8 | int(m[p+1])
			if t >= limit {
				return "", 0, errDNS
			}
			if !jumped {
				after, jumped = p+2, true
			}
			limit, p = t, t
		case l&0xC0 != 0:
			return "", 0, errDNS
		default:
			if p+1+l > len(m) || total+l+2 > 255 {
				return "", 0, errDNS
			}
			if build {
				sb.Write(m[p+1 : p+1+l])
				sb.WriteByte('.')
			}
			total += l + 1
			p += l + 1
		}
	}
}

func sameName(m []byte, pos int, want string) (bool, int, error) {
	n, after, err := readName(m, pos, true)
	return err == nil && strings.EqualFold(n, want), after, err
}

// skipRecord: the record at pos, its owner, type, class, TTL, rdata bounds
func readRecord(m []byte, pos int, build bool) (owner string, typ, class uint16, ttl uint32, rd, rdlen, next int, err error) {
	owner, p, err := readName(m, pos, build)
	if err != nil || p+10 > len(m) {
		return "", 0, 0, 0, 0, 0, 0, errDNS
	}
	typ, class = u16(m, p), u16(m, p+2)
	ttl = uint32(u16(m, p+4))<<16 | uint32(u16(m, p+6))
	rdlen = int(u16(m, p+8))
	rd = p + 10
	if rd+rdlen > len(m) {
		return "", 0, 0, 0, 0, 0, 0, errDNS
	}
	return owner, typ, class, ttl, rd, rdlen, rd + rdlen, nil
}

func parseAnswer(m []byte, id uint16, qname string, qtype uint16) ([]dnsRecord, error) {
	if len(m) < 12 || u16(m, 0) != id || m[2]&0x80 == 0 || u16(m, 4) != 1 {
		return nil, errDNS
	}
	an, ns, ar := int(u16(m, 6)), int(u16(m, 8)), int(u16(m, 10))
	ok, p, err := sameName(m, 12, qname)
	if err != nil || !ok || p+4 > len(m) || u16(m, p) != qtype || u16(m, p+2) != 1 {
		return nil, errDNS
	}
	p += 4
	answers := p
	for i := 0; i < an+ns; i++ {
		if _, _, _, _, _, _, p, err = readRecord(m, p, false); err != nil {
			return nil, err
		}
	}
	ext := 0
	for i := 0; i < ar; i++ {
		var typ uint16
		var ttl uint32
		if _, typ, _, ttl, _, _, p, err = readRecord(m, p, false); err != nil {
			return nil, err
		}
		if typ == 41 {
			ext = int(ttl >> 24)
		}
	}
	if ext<<4|int(m[3]&0xF) != 0 {
		return nil, errDNS
	}
	var out []dnsRecord
	p = answers
	for i := 0; i < an; i++ {
		owner, typ, class, _, rd, rdlen, next, err := readRecord(m, p, true)
		if err != nil {
			return nil, err
		}
		p = next
		if typ != qtype || class != 1 || !strings.EqualFold(owner, qname) {
			continue
		}
		var r dnsRecord
		switch typ {
		case 15:
			r.first = u16(m, rd)
			var end int
			if r.name, end, err = readName(m, rd+2, true); err != nil || end != rd+rdlen {
				return nil, errDNS
			}
		case 33:
			r.first, r.second, r.third = u16(m, rd), u16(m, rd+2), u16(m, rd+4)
			var end int
			if r.name, end, err = readName(m, rd+6, true); err != nil || end != rd+rdlen {
				return nil, errDNS
			}
		case 16:
			var sb strings.Builder
			for q := rd; q < rd+rdlen; {
				l := int(m[q])
				if q+1+l > rd+rdlen {
					return nil, errDNS
				}
				sb.Write(m[q+1 : q+1+l])
				q += l + 1
			}
			r.text = sb.String()
		}
		out = append(out, r)
	}
	return out, nil
}

func benchDNSParse(n int64) bool {
	if n == 0 {
		n = 5000000
	}
	var msgs [3][]byte
	for i, h := range dnsResponses {
		msgs[i], _ = hex.DecodeString(h)
	}
	names := []string{"example.com.", "example.com.", "_xmpp-server._tcp.example.com."}
	types := []uint16{15, 16, 33}
	check := int64(0)
	t0 := time.Now()
	for i := int64(0); i < n; i++ {
		k := i % 3
		r, err := parseAnswer(msgs[k], 0x1234, names[k], types[k])
		if err == nil {
			check += int64(len(r))
		}
	}
	report("dns_parse", time.Since(t0).Seconds(), float64(n), "")
	want := n/3*11 + map[int64]int64{0: 0, 1: 5, 2: 8}[n%3]
	return check == want
}

// The server of dns_lookup: each query answered from memory, its header
// and question echoed, five MX records after them whose owner points to
// the question's name (the same bytes as the SGCL side's server)
func dnsServe(c *net.UDPConn) {
	in := make([]byte, 2048)
	out := make([]byte, 2048)
	record := []byte{0xc0, 0x0c, 0, 15, 0, 1, 0, 0, 0x0e, 0x10, 0, 8, 0, 10, 3, 'm', 'x', '0', 0xc0, 0x0c}
	for {
		size, from, err := c.ReadFromUDP(in)
		if err != nil {
			return
		}
		end := 12
		for end < size && in[end] != 0 {
			end += int(in[end]) + 1
		}
		end += 5
		if size < end {
			continue
		}
		copy(out, in[:end])
		out[2], out[3] = 0x81, 0x80
		out[6], out[7] = 0, 5
		out[8], out[9], out[10], out[11] = 0, 0, 0, 0
		k := end
		for i := 0; i < 5; i++ {
			copy(out[k:], record)
			out[k+13] = byte(10 * (i + 1))
			out[k+17] = byte('1' + i)
			k += len(record)
		}
		c.WriteToUDP(out[:k], from)
	}
}

func benchDNSLookup(n int64) bool {
	if n == 0 {
		n = 20000
	}
	c, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		panic(err)
	}
	go dnsServe(c)
	server := c.LocalAddr().String()
	r := &net.Resolver{
		PreferGo: true,
		Dial: func(ctx context.Context, network, address string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, network, server)
		},
	}
	check := int64(0)
	run := func(count int64) {
		for i := int64(0); i < count; i++ {
			mx, err := r.LookupMX(context.Background(), "example.test.")
			if err == nil {
				check += int64(len(mx))
			}
		}
	}
	run(100) // warm, as the SGCL side
	check = 0
	t0 := time.Now()
	run(n)
	report("dns_lookup", time.Since(t0).Seconds(), float64(n), "")
	c.Close()
	return check == n*5
}
