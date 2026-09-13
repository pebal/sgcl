// Same shape as benchmarks/large_tree.cpp: a large tree held for the whole
// run, threads making and dropping small trees. Prints wall and process
// CPU time and small trees per second.
//   java LargeTree [big_depth=22] [small_depth=8] [iterations=100000] [threads=1]
import java.util.concurrent.atomic.AtomicLong;

public final class LargeTree {
    static final class Tree { Tree left, right; }

    static Tree make(int depth) {
        Tree n = new Tree();
        if (depth > 0) { n.left = make(depth - 1); n.right = make(depth - 1); }
        return n;
    }

    static long check(Tree n) { return n.left != null ? 1 + check(n.left) + check(n.right) : 1; }

    public static void main(String[] args) throws Exception {
        int big = args.length > 0 ? Integer.parseInt(args[0]) : 22;
        int small = args.length > 1 ? Integer.parseInt(args[1]) : 8;
        long iterations = args.length > 2 ? Long.parseLong(args[2]) : 100000;
        int threads = args.length > 3 ? Integer.parseInt(args[3]) : 1;
        long t0 = System.nanoTime();
        Tree large = make(big);
        double built = (System.nanoTime() - t0) / 1e9;
        AtomicLong sum = new AtomicLong();
        long t1 = System.nanoTime();
        Thread[] ws = new Thread[threads];
        for (int t = 0; t < threads; ++t) {
            ws[t] = new Thread(() -> {
                long s = 0;
                for (long i = 0; i < iterations; ++i) {
                    Tree tree = make(small);
                    s += check(tree);
                }
                sum.addAndGet(s);
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        double loop = (System.nanoTime() - t1) / 1e9;
        System.out.printf("large tree of depth %d (%d nodes) built in %.2fs, check %d%n", big, (1L << (big + 1)) - 1, built, check(large));
        System.out.printf("threads=%d small=%d iterations=%d sum=%d wall=%.2fs cpu=%.2fs trees/s=%.0f%n",
            threads, small, iterations, sum.get(), loop, Common.cpuSeconds(), iterations * threads / loop);
    }
}
