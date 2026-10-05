// DNS over TLS (RFC 7858) and DNS over HTTPS (RFC 8484) servers written
// with Go's standard library alone, the oracle of tests/net/dns_tls.cpp and
// tests/net/http/dns_https.cpp: crypto/tls serving the length-framed
// messages of RFC 1035 §4.2.2, net/http serving application/dns-message by
// POST and by GET (?dns=, base64url without padding) over TLS, where Go
// offers HTTP/2 by ALPN. The DNS messages are read and written here by hand
// from RFC 1035 (the standard library has no public DNS codec), answering
// from a fixed zone:
//
//	host.example.test.  A     192.0.2.1
//	host.example.test.  AAAA  2001:db8::1
//	example.test.       MX    10 mx1.example.test., 20 mx2.example.test.
//	example.test.       TXT   "v=spf1 -all"
//
// any other name NXDOMAIN, a name of the zone without the type NODATA.
// Usage: dns_secure_oracle -cert leaf.pem -key leaf.key; it prints
// "DOT <port>" and "DOH <port>", then a line for each query: "DOT
// padded|unpadded <name> <type>" and "DOH <proto> <method> padded|unpadded
// <name> <type>", and serves until it is killed, its parent (the test)
// ends, or ten minutes pass: a test that dies leaves no server behind.
package main

import (
	"bufio"
	"crypto/tls"
	"encoding/base64"
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

type rr struct {
	name  string
	typ   uint16
	rdata []byte
}

func wireName(n string) []byte {
	var b []byte
	for _, l := range strings.Split(strings.TrimSuffix(n, "."), ".") {
		b = append(b, byte(len(l)))
		b = append(b, l...)
	}
	return append(b, 0)
}

func mx(pref uint16, host string) []byte {
	b := binary.BigEndian.AppendUint16(nil, pref)
	return append(b, wireName(host)...)
}

var zone = []rr{
	{"host.example.test.", 1, []byte{192, 0, 2, 1}},
	{"host.example.test.", 28, []byte{0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
	{"example.test.", 15, mx(10, "mx1.example.test.")},
	{"example.test.", 15, mx(20, "mx2.example.test.")},
	{"example.test.", 16, append([]byte{11}, "v=spf1 -all"...)},
}

var out = struct {
	sync.Mutex
	w *bufio.Writer
}{w: bufio.NewWriter(os.Stdout)}

func say(format string, a ...any) {
	out.Lock()
	fmt.Fprintf(out.w, format+"\n", a...)
	out.w.Flush()
	out.Unlock()
}

// The question of a query: its name (lower case, absolute), type, and
// where it ends; whether it carries the Padding option of RFC 7830
func question(q []byte) (name string, typ uint16, end int, padded bool, ok bool) {
	if len(q) < 12 || binary.BigEndian.Uint16(q[4:]) != 1 {
		return
	}
	p := 12
	var labels []string
	for {
		if p >= len(q) {
			return
		}
		l := int(q[p])
		if l == 0 {
			p++
			break
		}
		if l > 63 || p+1+l > len(q) {
			return
		}
		labels = append(labels, strings.ToLower(string(q[p+1:p+1+l])))
		p += 1 + l
	}
	if p+4 > len(q) {
		return
	}
	typ = binary.BigEndian.Uint16(q[p:])
	end = p + 4
	name = strings.Join(labels, ".") + "."
	// the additional section: the OPT record and its options
	r := end
	ar := int(binary.BigEndian.Uint16(q[10:]))
	for i := 0; i < ar && r+11 <= len(q); i++ {
		if q[r] != 0 {
			break
		}
		t := binary.BigEndian.Uint16(q[r+1:])
		rdlen := int(binary.BigEndian.Uint16(q[r+9:]))
		rd := r + 11
		if rd+rdlen > len(q) {
			break
		}
		if t == 41 {
			for o := rd; o+4 <= rd+rdlen; {
				code := binary.BigEndian.Uint16(q[o:])
				olen := int(binary.BigEndian.Uint16(q[o+2:]))
				if code == 12 {
					padded = len(q)%128 == 0
				}
				o += 4 + olen
			}
		}
		r = rd + rdlen
	}
	ok = true
	return
}

// The answer to a query from the zone
func answer(q []byte) []byte {
	name, typ, end, _, ok := question(q)
	if !ok {
		return nil
	}
	var answers [][]byte
	exists := false
	for _, r := range zone {
		if r.name != name {
			continue
		}
		exists = true
		if r.typ != typ {
			continue
		}
		a := wireName(r.name)
		a = binary.BigEndian.AppendUint16(a, r.typ)
		a = binary.BigEndian.AppendUint16(a, 1)
		a = binary.BigEndian.AppendUint32(a, 300)
		a = binary.BigEndian.AppendUint16(a, uint16(len(r.rdata)))
		answers = append(answers, append(a, r.rdata...))
	}
	flags := uint16(0x8000 | 0x0080 | (binary.BigEndian.Uint16(q[2:]) & 0x0100))
	if !exists {
		flags |= 3
	} else {
		flags |= 0x0400
	}
	b := make([]byte, 12, 512)
	copy(b, q[:2])
	binary.BigEndian.PutUint16(b[2:], flags)
	binary.BigEndian.PutUint16(b[4:], 1)
	binary.BigEndian.PutUint16(b[6:], uint16(len(answers)))
	b = append(b, q[12:end]...)
	for _, a := range answers {
		b = append(b, a...)
	}
	return b
}

func padWord(q []byte) string {
	if _, _, _, padded, _ := question(q); padded {
		return "padded"
	}
	return "unpadded"
}

func describe(q []byte) string {
	name, typ, _, _, _ := question(q)
	return fmt.Sprintf("%s %d", name, typ)
}

func serveDot(c net.Conn) {
	defer c.Close()
	var mu sync.Mutex
	for {
		var n [2]byte
		if _, err := io.ReadFull(c, n[:]); err != nil {
			return
		}
		q := make([]byte, binary.BigEndian.Uint16(n[:]))
		if _, err := io.ReadFull(c, q); err != nil {
			return
		}
		say("DOT %s %s", padWord(q), describe(q))
		a := answer(q)
		if a == nil {
			return
		}
		mu.Lock()
		c.Write(binary.BigEndian.AppendUint16(nil, uint16(len(a))))
		c.Write(a)
		mu.Unlock()
	}
}

func dohHandler(w http.ResponseWriter, r *http.Request) {
	var q []byte
	var err error
	switch r.Method {
	case http.MethodPost:
		if r.Header.Get("Content-Type") != "application/dns-message" {
			http.Error(w, "unsupported media type", http.StatusUnsupportedMediaType)
			return
		}
		q, err = io.ReadAll(io.LimitReader(r.Body, 65536))
	case http.MethodGet:
		q, err = base64.RawURLEncoding.DecodeString(r.URL.Query().Get("dns"))
	default:
		http.Error(w, "method", http.StatusMethodNotAllowed)
		return
	}
	if err != nil {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	say("DOH %s %s %s %s", r.Proto, r.Method, padWord(q), describe(q))
	a := answer(q)
	if a == nil {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}
	w.Header().Set("Content-Type", "application/dns-message")
	w.Header().Set("Cache-Control", "max-age=300")
	w.Write(a)
}

func main() {
	certFile := flag.String("cert", "", "the leaf's PEM")
	keyFile := flag.String("key", "", "its key")
	flag.Parse()
	cert, err := tls.LoadX509KeyPair(*certFile, *keyFile)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	dot, err := tls.Listen("tcp", "127.0.0.1:0", &tls.Config{Certificates: []tls.Certificate{cert}, NextProtos: []string{"dot"}})
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	doh, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	go func() {
		for {
			c, err := dot.Accept()
			if err != nil {
				return
			}
			go serveDot(c)
		}
	}()
	mux := http.NewServeMux()
	mux.HandleFunc("/dns-query", dohHandler)
	srv := &http.Server{Handler: mux, TLSConfig: &tls.Config{Certificates: []tls.Certificate{cert}}}
	go srv.ServeTLS(doh, "", "")
	say("DOT %d", dot.Addr().(*net.TCPAddr).Port)
	say("DOH %d", doh.Addr().(*net.TCPAddr).Port)
	parent := os.Getppid()
	deadline := time.Now().Add(10 * time.Minute)
	for os.Getppid() == parent && time.Now().Before(deadline) {
		time.Sleep(200 * time.Millisecond)
	}
}
