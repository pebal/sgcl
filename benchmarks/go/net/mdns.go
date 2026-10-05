// The multicast DNS cases of the net benchmark (benchmarks/net/net.cpp has
// the SGCL side, the same cases and bytes).
//
//	mdns_parse [n]      a response (a PTR of _bench._tcp.local., its SRV, TXT, A and AAAA, the names
//	                    compressed) read whole, the names inside the PTR's and SRV's rdata made
//	                    canonical (uncompressed): per message
//	mdns_build [n]      the same response written from its records, the names compressed: per message
//	mdns_roundtrip [n]  a legacy unicast query (RFC 6762 §6.7) of the PTR to 224.0.0.251:5353 on the
//	                    loopback, answered by a responder over net.ListenMulticastUDP with the PTR,
//	                    SRV, TXT and addresses: per round trip
//
// The standard library has no mDNS and no public DNS codec: the reader,
// the writer and the responder are written here from RFC 1035 and RFC 6762
// for the same work the SGCL side does. The responder is the minimal one a
// round trip needs (the question matched, the legacy answer written); the
// SGCL side's is the module's whole responder.
package main

import (
	"encoding/binary"
	"net"
	"strings"
	"syscall"
	"time"
)

type mdnsRecord struct {
	name   []byte // wire form
	typ    uint16
	class  uint16
	unique bool
	ttl    uint32
	rdata  []byte // canonical
}

type mdnsQuestion struct {
	name    []byte
	typ     uint16
	unicast bool
}

func mdnsWire(text string) []byte {
	var b []byte
	for _, l := range strings.Split(strings.TrimSuffix(text, "."), ".") {
		b = append(b, byte(len(l)))
		b = append(b, l...)
	}
	return append(b, 0)
}

func mdnsBenchRecords() []mdnsRecord {
	srv := []byte{0, 0, 0, 0, 0x1f, 0x90}
	srv = append(srv, mdnsWire("benchhost.local.")...)
	txt := []byte{}
	for _, e := range []string{"path=/api", "version=1", "secure"} {
		txt = append(txt, byte(len(e)))
		txt = append(txt, e...)
	}
	return []mdnsRecord{
		{mdnsWire("_bench._tcp.local."), 12, 1, false, 4500, mdnsWire("Bench Service._bench._tcp.local.")},
		{mdnsWire("Bench Service._bench._tcp.local."), 33, 1, true, 120, srv},
		{mdnsWire("Bench Service._bench._tcp.local."), 16, 1, true, 4500, txt},
		{mdnsWire("benchhost.local."), 1, 1, true, 120, []byte{0xc0, 0xa8, 0x01, 0x0a}},
		{mdnsWire("benchhost.local."), 28, 1, true, 120, []byte{0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x11, 0x22, 0xff, 0xfe, 0x33, 0x44, 0x55}},
	}
}

// A writer with name compression: each suffix written remembered by its bytes
type mdnsWriter struct {
	b      []byte
	seen   map[string]int
	counts [4]uint16
}

func newMdnsWriter(buf []byte, id, flags uint16) *mdnsWriter {
	w := &mdnsWriter{b: buf[:12], seen: map[string]int{}}
	binary.BigEndian.PutUint16(w.b, id)
	binary.BigEndian.PutUint16(w.b[2:], flags)
	return w
}

func (w *mdnsWriter) name(n []byte, compress bool) {
	for i := 0; n[i] != 0; i += int(n[i]) + 1 {
		if compress {
			if at, ok := w.seen[strings.ToLower(string(n[i:]))]; ok {
				w.b = binary.BigEndian.AppendUint16(w.b, uint16(0xC000|at))
				return
			}
		}
		if len(w.b) < 0x4000 {
			w.seen[strings.ToLower(string(n[i:]))] = len(w.b)
		}
		w.b = append(w.b, n[i:i+int(n[i])+1]...)
	}
	w.b = append(w.b, 0)
}

func (w *mdnsWriter) question(q mdnsQuestion) {
	w.name(q.name, true)
	class := uint16(1)
	if q.unicast {
		class |= 0x8000
	}
	w.b = binary.BigEndian.AppendUint16(w.b, q.typ)
	w.b = binary.BigEndian.AppendUint16(w.b, class)
	w.counts[0]++
}

func (w *mdnsWriter) record(section int, r mdnsRecord, ttl uint32, flush bool) {
	w.name(r.name, true)
	class := r.class
	if flush {
		class |= 0x8000
	}
	w.b = binary.BigEndian.AppendUint16(w.b, r.typ)
	w.b = binary.BigEndian.AppendUint16(w.b, class)
	w.b = binary.BigEndian.AppendUint32(w.b, ttl)
	at := len(w.b)
	w.b = append(w.b, 0, 0)
	switch r.typ {
	case 12:
		w.name(r.rdata, true)
	case 33:
		w.b = append(w.b, r.rdata[:6]...)
		w.name(r.rdata[6:], false)
	default:
		w.b = append(w.b, r.rdata...)
	}
	binary.BigEndian.PutUint16(w.b[at:], uint16(len(w.b)-at-2))
	w.counts[section+1]++
}

func (w *mdnsWriter) finish() []byte {
	for i, c := range w.counts {
		binary.BigEndian.PutUint16(w.b[4+2*i:], c)
	}
	return w.b
}

func mdnsBuild(records []mdnsRecord, buf []byte) []byte {
	w := newMdnsWriter(buf, 0, 0x8400)
	w.record(0, records[0], records[0].ttl, false)
	for _, r := range records[1:] {
		w.record(2, r, r.ttl, true)
	}
	return w.finish()
}

// The name at pos in wire form, pointers followed; the position after it
func mdnsName(m []byte, pos int, out []byte) ([]byte, int, error) {
	p, limit, after, jumped := pos, pos, 0, false
	for {
		if p >= len(m) {
			return nil, 0, errDNS
		}
		l := int(m[p])
		switch {
		case l == 0:
			if !jumped {
				after = p + 1
			}
			return append(out, 0), after, nil
		case l&0xC0 == 0xC0:
			if p+1 >= len(m) {
				return nil, 0, errDNS
			}
			t := (l&0x3F)<<8 | int(m[p+1])
			if t >= limit {
				return nil, 0, errDNS
			}
			if !jumped {
				after, jumped = p+2, true
			}
			limit, p = t, t
		case l&0xC0 != 0:
			return nil, 0, errDNS
		default:
			if p+1+l > len(m) || len(out)+l+2 > 255 {
				return nil, 0, errDNS
			}
			out = append(out, m[p:p+1+l]...)
			p += l + 1
		}
	}
}

type mdnsMessage struct {
	id, flags uint16
	questions []mdnsQuestion
	records   [3][]mdnsRecord
}

// A whole message read, its slices reused
func mdnsRead(m []byte, out *mdnsMessage) error {
	if len(m) < 12 || len(m) > 9000 {
		return errDNS
	}
	out.id, out.flags = u16(m, 0), u16(m, 2)
	if (out.flags>>11)&0xF != 0 {
		return errDNS
	}
	qd := int(u16(m, 4))
	counts := [3]int{int(u16(m, 6)), int(u16(m, 8)), int(u16(m, 10))}
	out.questions = out.questions[:0]
	p := 12
	var err error
	for i := 0; i < qd; i++ {
		var q mdnsQuestion
		if q.name, p, err = mdnsName(m, p, nil); err != nil || p+4 > len(m) {
			return errDNS
		}
		q.typ = u16(m, p)
		q.unicast = u16(m, p+2)&0x8000 != 0
		p += 4
		out.questions = append(out.questions, q)
	}
	for s := 0; s < 3; s++ {
		out.records[s] = out.records[s][:0]
		for i := 0; i < counts[s]; i++ {
			var r mdnsRecord
			if r.name, p, err = mdnsName(m, p, nil); err != nil || p+10 > len(m) {
				return errDNS
			}
			r.typ, r.class = u16(m, p), u16(m, p+2)
			r.unique = r.class&0x8000 != 0
			r.class &= 0x7FFF
			r.ttl = uint32(u16(m, p+4))<<16 | uint32(u16(m, p+6))
			rdlen := int(u16(m, p+8))
			rd := p + 10
			if rd+rdlen > len(m) {
				return errDNS
			}
			switch r.typ {
			case 12, 5, 2:
				var end int
				if r.rdata, end, err = mdnsName(m, rd, nil); err != nil || end != rd+rdlen {
					return errDNS
				}
			case 33:
				if rdlen < 7 {
					return errDNS
				}
				var end int
				r.rdata = append([]byte(nil), m[rd:rd+6]...)
				if r.rdata, end, err = mdnsName(m, rd+6, r.rdata); err != nil || end != rd+rdlen {
					return errDNS
				}
			default:
				r.rdata = append([]byte(nil), m[rd:rd+rdlen]...)
			}
			p = rd + rdlen
			out.records[s] = append(out.records[s], r)
		}
	}
	return nil
}

func benchMdnsCodec(what string, n int64) bool {
	if n == 0 {
		n = 2000000
	}
	records := mdnsBenchRecords()
	msg := append([]byte(nil), mdnsBuild(records, make([]byte, 1500))...)
	check := int64(0)
	t0 := time.Now()
	if what == "mdns_parse" {
		var m mdnsMessage
		for i := int64(0); i < n; i++ {
			if mdnsRead(msg, &m) == nil {
				check += int64(len(m.records[0]) + len(m.records[2]))
			}
		}
	} else {
		buf := make([]byte, 1500)
		for i := int64(0); i < n; i++ {
			if len(mdnsBuild(records, buf)) == len(msg) {
				check += 5
			}
		}
	}
	report(what, time.Since(t0).Seconds(), float64(n), "")
	return check == n*5
}

func benchMdnsRoundtrip(n int64) bool {
	if n == 0 {
		n = 20000
	}
	var lo *net.Interface
	ifs, _ := net.Interfaces()
	for i := range ifs {
		if ifs[i].Flags&net.FlagLoopback != 0 && ifs[i].Flags&net.FlagMulticast != 0 {
			lo = &ifs[i]
			break
		}
	}
	group := &net.UDPAddr{IP: net.IPv4(224, 0, 0, 251), Port: 5353}
	in, err := net.ListenMulticastUDP("udp4", lo, group)
	if err != nil {
		panic(err)
	}
	records := mdnsBenchRecords()
	ptr := records[0].name
	// the responder: a PTR question of the service's type answered as a
	// legacy unicast answer (the id and question echoed, TTLs of 10 s)
	go func() {
		buf := make([]byte, 9000)
		out := make([]byte, 1500)
		var m mdnsMessage
		for {
			k, from, err := in.ReadFromUDP(buf)
			if err != nil {
				return
			}
			if mdnsRead(buf[:k], &m) != nil || m.flags&0x8000 != 0 || len(m.questions) != 1 {
				continue
			}
			q := m.questions[0]
			if q.typ != 12 || !strings.EqualFold(string(q.name), string(ptr)) {
				continue
			}
			legacy := from.Port != 5353
			w := newMdnsWriter(out, m.id, 0x8400)
			ttl := func(t uint32) uint32 {
				if legacy && t > 10 {
					return 10
				}
				return t
			}
			if legacy {
				w.question(mdnsQuestion{name: q.name, typ: q.typ})
			}
			w.record(0, records[0], ttl(records[0].ttl), false)
			for _, r := range records[1:] {
				w.record(2, r, ttl(r.ttl), r.unique && !legacy)
			}
			in.WriteToUDP(w.finish(), from)
		}
	}()
	q, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		panic(err)
	}
	raw, _ := q.SyscallConn()
	raw.Control(func(fd uintptr) {
		syscall.SetsockoptInet4Addr(int(fd), syscall.IPPROTO_IP, syscall.IP_MULTICAST_IF, [4]byte{127, 0, 0, 1})
	})
	q.SetReadDeadline(time.Now().Add(60 * time.Second))
	query := make([]byte, 0, 64)
	room := make([]byte, 2048)
	rounds := func(count int64) int64 {
		got := int64(0)
		for i := int64(0); i < count; i++ {
			id := uint16(i + 1)
			query = binary.BigEndian.AppendUint16(query[:0], id)
			query = append(query, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0)
			query = append(query, ptr...)
			query = append(query, 0, 12, 0, 1)
			if _, err := q.WriteToUDP(query, group); err != nil {
				break
			}
			for {
				k, _, err := q.ReadFromUDP(room)
				if err != nil {
					return got
				}
				if k >= 12 && binary.BigEndian.Uint16(room) == id {
					if k > 100 {
						got++
					}
					break
				}
			}
		}
		return got
	}
	rounds(200)
	t0 := time.Now()
	got := rounds(n)
	report("mdns_roundtrip", time.Since(t0).Seconds(), float64(n), "")
	q.Close()
	in.Close()
	return got == n
}
