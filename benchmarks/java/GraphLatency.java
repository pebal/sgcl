// Latency of single operations on a shared graph, the shape of
// benchmarks/graph_latency.cpp: each thread keeps `roots` roots; an
// insert allocates a node linked to four random roots and replaces a
// random root, a walk follows 32 random links; percentiles per operation,
// then every root dropped. One insert and one walk in every eight
// operations are timed: on macOS System.nanoTime() serializes the threads
// when every operation calls it twice (16 threads ran at a third of their
// throughput), where the C++ clock does not.
//   java GraphLatency [threads=cores] [seconds=5] [roots=4096]
public final class GraphLatency {
    static final int Links = 4, WalkSteps = 32;
    static final class Node { final Node[] link = new Node[Links]; long value, pad0, pad1, pad2; }

    static final class Result { int[] insert = new int[1 << 22], walk = new int[1 << 22]; int ni, nw; long ops, checksum; double dropNs; }

    static Result worker(int id, double seconds, int rootCount) {
        Result r = new Result();
        long x = 1234 + id;   // xorshift
        Node[] roots = new Node[rootCount];
        for (int i = 0; i < rootCount; ++i) { roots[i] = new Node(); roots[i].value = i; }
        long t0 = System.nanoTime(), limit = (long) (seconds * 1e9);
        long op = 0;
        for (;;) {
            if ((op & 1023) == 0 && System.nanoTime() - t0 >= limit) break;
            ++op;
            boolean timed = (op & 7) < 2;   // op % 8 == 0 is an insert, == 1 a walk
            long start = timed ? System.nanoTime() : 0;
            if (op % 4 == 0) {
                Node n = new Node(); n.value = op;
                for (int l = 0; l < Links; ++l) { x ^= x << 13; x ^= x >>> 7; x ^= x << 17; n.link[l] = roots[(int) Long.remainderUnsigned(x, rootCount)]; }
                x ^= x << 13; x ^= x >>> 7; x ^= x << 17;
                roots[(int) Long.remainderUnsigned(x, rootCount)] = n;
                if (timed && r.ni < r.insert.length) r.insert[r.ni++] = (int) (System.nanoTime() - start);
            } else {
                x ^= x << 13; x ^= x >>> 7; x ^= x << 17;
                Node cur = roots[(int) Long.remainderUnsigned(x, rootCount)];
                long sum = 0;
                for (int s = 0; s < WalkSteps; ++s) {
                    sum += cur.value;
                    x ^= x << 13; x ^= x >>> 7; x ^= x << 17;
                    Node next = cur.link[(int) Long.remainderUnsigned(x, Links)];
                    if (next == null) break;
                    cur = next;
                }
                r.checksum += sum;
                if (timed && r.nw < r.walk.length) r.walk[r.nw++] = (int) (System.nanoTime() - start);
            }
        }
        r.ops = op;
        long drop = System.nanoTime();
        java.util.Arrays.fill(roots, null);
        r.dropNs = System.nanoTime() - drop;
        return r;
    }

    public static void main(String[] args) throws Exception {
        int threads = args.length > 0 ? Integer.parseInt(args[0]) : Runtime.getRuntime().availableProcessors();
        double seconds = args.length > 1 ? Double.parseDouble(args[1]) : 5;
        int roots = args.length > 2 ? Integer.parseInt(args[2]) : 4096;
        Result[] results = new Result[threads];
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) { final int id = t; ws[t] = new Thread(() -> results[id] = worker(id, seconds, roots)); ws[t].start(); }
        for (Thread w : ws) w.join();
        double wall = (System.nanoTime() - t0) / 1e9;
        int ti = 0, tw = 0; long ops = 0, checksum = 0; double dropMax = 0;
        for (Result r : results) { ti += r.ni; tw += r.nw; ops += r.ops; dropMax = Math.max(dropMax, r.dropNs); checksum += r.checksum; }
        int[] insert = new int[ti], walk = new int[tw]; int pi = 0, pw = 0;
        for (Result r : results) { System.arraycopy(r.insert, 0, insert, pi, r.ni); pi += r.ni; System.arraycopy(r.walk, 0, walk, pw, r.nw); pw += r.nw; }
        double[] a = Common.percentiles(insert, ti), b = Common.percentiles(walk, tw);
        System.out.printf("insert p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns%n", a[0], a[1], a[2], a[3], a[4], a[5]);
        System.out.printf("walk   p50=%.0f p90=%.0f p99=%.0f p99.9=%.0f p99.99=%.0f max=%.0f ns%n", b[0], b[1], b[2], b[3], b[4], b[5]);
        System.out.printf("drop-all max=%.3f ms  ops/s=%.0f  wall=%.2fs cpu=%.2fs  (checksum %d)%n", dropMax * 1e-6, ops / wall, wall, Common.cpuSeconds(), checksum);
    }
}
