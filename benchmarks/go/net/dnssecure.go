// The DNS over TLS and DNS over HTTPS cases of the net benchmark
// (benchmarks/net/net.cpp has the SGCL side, the same cases and bytes).
//
//	dot_lookup [n]  a lookup of five MX records over DNS over TLS (RFC 7858) against a server on
//	                the loopback answering from memory (dns_lookup's answer, framed by its length),
//	                one kept connection, one lookup after another: per lookup
//	doh_lookup [n]  the same over DNS over HTTPS (RFC 8484): POST of application/dns-message,
//	                net/http's client and server over TLS, HTTP/2 by ALPN: per lookup
//
// The standard library has neither DoT nor DoH, nor a public DNS codec: the
// client is written here for the same work the SGCL side does. A query is
// the message with a random id (DoH: id 0), recursion desired, an OPT
// record with the Padding option to a multiple of 128 bytes (RFC 8467); the
// DoT client keeps one connection, writes each query under a lock and hands
// the answers out by id from a reader goroutine (a client that may have
// several queries in flight, as the SGCL transport may), each lookup
// waiting for its answer or a deadline; the answer is read by dns.go's
// parseAnswer, the records of the type decoded.
package main

import (
	"bytes"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"io"
	"net"
	"net/http"
	"os"
	"sync"
	"time"
)

// A query of example.test. MX, padded to 128 bytes
func dnsQuery(id uint16) []byte {
	q := make([]byte, 0, 128)
	q = binary.BigEndian.AppendUint16(q, id)
	q = append(q, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 1)
	q = append(q, 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 4, 't', 'e', 's', 't', 0, 0, 15, 0, 1)
	pad := (128 - (len(q)+11+4)%128) % 128
	q = append(q, 0, 0, 41, 0x04, 0xd0, 0, 0, 0, 0)
	q = binary.BigEndian.AppendUint16(q, uint16(4+pad))
	q = append(q, 0, 12)
	q = binary.BigEndian.AppendUint16(q, uint16(pad))
	return append(q, make([]byte, pad)...)
}

// dns_lookup's answer to a query: its header and question echoed, five MX after them
func dnsAnswerOf(in []byte, out []byte) int {
	record := []byte{0xc0, 0x0c, 0, 15, 0, 1, 0, 0, 0x0e, 0x10, 0, 8, 0, 10, 3, 'm', 'x', '0', 0xc0, 0x0c}
	end := 12
	for end < len(in) && in[end] != 0 {
		end += int(in[end]) + 1
	}
	end += 5
	if len(in) < end {
		return 0
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
	return k
}

func dotServe(c net.Conn) {
	defer c.Close()
	in := make([]byte, 4096)
	out := make([]byte, 4098)
	for {
		if _, err := io.ReadFull(c, in[:2]); err != nil {
			return
		}
		n := int(binary.BigEndian.Uint16(in))
		if _, err := io.ReadFull(c, in[:n]); err != nil {
			return
		}
		k := dnsAnswerOf(in[:n], out[2:])
		binary.BigEndian.PutUint16(out, uint16(k))
		if _, err := c.Write(out[:k+2]); err != nil {
			return
		}
	}
}

// The DoT client: one connection, the answers handed out by id
type dotClient struct {
	c       *tls.Conn
	writing sync.Mutex
	m       sync.Mutex
	waiting map[uint16]chan []byte
}

func (d *dotClient) read() {
	head := make([]byte, 2)
	for {
		if _, err := io.ReadFull(d.c, head); err != nil {
			break
		}
		n := int(binary.BigEndian.Uint16(head))
		if n < 12 {
			break
		}
		answer := make([]byte, n)
		if _, err := io.ReadFull(d.c, answer); err != nil {
			break
		}
		id := binary.BigEndian.Uint16(answer)
		d.m.Lock()
		ch := d.waiting[id]
		delete(d.waiting, id)
		d.m.Unlock()
		if ch != nil {
			ch <- answer
		}
	}
	d.m.Lock()
	for id, ch := range d.waiting {
		close(ch)
		delete(d.waiting, id)
	}
	d.m.Unlock()
}

var errTimeout = errors.New("timeout")

func (d *dotClient) lookupMX(timeout time.Duration) ([]dnsRecord, error) {
	var b [2]byte
	ch := make(chan []byte, 1)
	var id uint16
	d.m.Lock()
	for {
		rand.Read(b[:])
		id = binary.BigEndian.Uint16(b[:])
		if _, taken := d.waiting[id]; !taken {
			break
		}
	}
	d.waiting[id] = ch
	d.m.Unlock()
	q := dnsQuery(id)
	frame := make([]byte, 2, 2+len(q))
	binary.BigEndian.PutUint16(frame, uint16(len(q)))
	frame = append(frame, q...)
	d.writing.Lock()
	_, err := d.c.Write(frame)
	d.writing.Unlock()
	if err != nil {
		return nil, err
	}
	t := time.NewTimer(timeout)
	defer t.Stop()
	select {
	case a, ok := <-ch:
		if !ok {
			return nil, errDNS
		}
		return parseAnswer(a, id, "example.test.", 15)
	case <-t.C:
		d.m.Lock()
		delete(d.waiting, id)
		d.m.Unlock()
		return nil, errTimeout
	}
}

func testTLS() (tls.Certificate, *x509.CertPool) {
	dir := "tests/net/tls_testdata/"
	pair, err := tls.LoadX509KeyPair(dir+"ecdsa.pem", dir+"ecdsa.key")
	if err != nil {
		panic(err)
	}
	pem, err := os.ReadFile(dir + "ca.pem")
	if err != nil {
		panic(err)
	}
	roots := x509.NewCertPool()
	roots.AppendCertsFromPEM(pem)
	return pair, roots
}

func benchDoT(n int64) bool {
	if n == 0 {
		n = 20000
	}
	pair, roots := testTLS()
	l, err := tls.Listen("tcp", "127.0.0.1:0", &tls.Config{Certificates: []tls.Certificate{pair}, NextProtos: []string{"dot"}})
	if err != nil {
		panic(err)
	}
	go func() {
		for {
			c, err := l.Accept()
			if err != nil {
				return
			}
			go dotServe(c)
		}
	}()
	c, err := tls.Dial("tcp", l.Addr().String(), &tls.Config{RootCAs: roots, ServerName: "127.0.0.1", NextProtos: []string{"dot"}})
	if err != nil {
		panic(err)
	}
	d := &dotClient{c: c, waiting: map[uint16]chan []byte{}}
	go d.read()
	check := int64(0)
	run := func(count int64) {
		for i := int64(0); i < count; i++ {
			if mx, err := d.lookupMX(2 * time.Second); err == nil {
				check += int64(len(mx))
			}
		}
	}
	run(100) // warm, as the SGCL side
	check = 0
	t0 := time.Now()
	run(n)
	report("dot_lookup", time.Since(t0).Seconds(), float64(n), "")
	c.Close()
	l.Close()
	return check == n*5
}

func benchDoH(n int64) bool {
	if n == 0 {
		n = 20000
	}
	pair, roots := testTLS()
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		panic(err)
	}
	mux := http.NewServeMux()
	mux.HandleFunc("POST /dns-query", func(w http.ResponseWriter, r *http.Request) {
		q, err := io.ReadAll(io.LimitReader(r.Body, 65536))
		if err != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		out := make([]byte, 4096)
		k := dnsAnswerOf(q, out)
		w.Header().Set("Content-Type", "application/dns-message")
		w.Write(out[:k])
	})
	srv := &http.Server{Handler: mux, TLSConfig: &tls.Config{Certificates: []tls.Certificate{pair}}}
	go srv.ServeTLS(l, "", "")
	client := &http.Client{Transport: &http.Transport{TLSClientConfig: &tls.Config{RootCAs: roots}, ForceAttemptHTTP2: true}}
	url := "https://127.0.0.1:" + itoa(l.Addr().(*net.TCPAddr).Port) + "/dns-query"
	query := dnsQuery(0)
	check := int64(0)
	h2 := true
	run := func(count int64) {
		for i := int64(0); i < count; i++ {
			req, _ := http.NewRequest("POST", url, bytes.NewReader(query))
			req.Header.Set("Content-Type", "application/dns-message")
			req.Header.Set("Accept", "application/dns-message")
			res, err := client.Do(req)
			if err != nil {
				continue
			}
			body, err := io.ReadAll(io.LimitReader(res.Body, 65536))
			res.Body.Close()
			h2 = h2 && res.ProtoMajor == 2
			if err != nil || res.StatusCode != 200 || res.Header.Get("Content-Type") != "application/dns-message" {
				continue
			}
			if mx, err := parseAnswer(body, 0, "example.test.", 15); err == nil {
				check += int64(len(mx))
			}
		}
	}
	run(100)
	check = 0
	t0 := time.Now()
	run(n)
	report("doh_lookup", time.Since(t0).Seconds(), float64(n), "")
	srv.Close()
	return check == n*5 && h2
}

func itoa(v int) string {
	var b [20]byte
	i := len(b)
	for v >= 10 {
		i--
		b[i] = byte('0' + v%10)
		v /= 10
	}
	i--
	b[i] = byte('0' + v)
	return string(b[i:])
}
