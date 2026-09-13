// The binary-trees benchmark, the shape of benchmarks/binary_trees.cpp:
// a stretch tree, a long-lived tree, then the depths 4, 6, ..., max
// split over the threads. Prints wall and process CPU time.
//   java BinaryTrees [max_depth=21] [threads=1]
import java.util.concurrent.atomic.AtomicInteger;

public final class BinaryTrees {
    static final class Tree { Tree left, right; }

    static Tree make(int depth) {
        Tree n = new Tree();
        if (depth > 0) { n.left = make(depth - 1); n.right = make(depth - 1); }
        return n;
    }

    static long check(Tree n) { return n.left != null ? 1 + check(n.left) + check(n.right) : 1; }

    public static void main(String[] args) throws Exception {
        int maxDepth = args.length > 0 ? Integer.parseInt(args[0]) : 21;
        int threads = args.length > 1 ? Integer.parseInt(args[1]) : 1;
        final int minDepth = 4;
        maxDepth = Math.max(minDepth + 2, maxDepth);
        long t0 = System.nanoTime();
        {
            Tree stretch = make(maxDepth + 1);
            System.out.printf("stretch tree of depth %d\t check: %d%n", maxDepth + 1, check(stretch));
        }
        Tree longLived = make(maxDepth);
        int lineCount = (maxDepth - minDepth) / 2 + 1;
        String[] lines = new String[lineCount];
        AtomicInteger next = new AtomicInteger();
        final int md = maxDepth;
        Thread[] ws = new Thread[threads];
        for (int t = 0; t < threads; ++t) {
            ws[t] = new Thread(() -> {
                for (;;) {
                    int i = next.getAndIncrement();
                    if (i >= lineCount) break;
                    int depth = minDepth + 2 * i;
                    long iterations = 1L << (md - depth + minDepth);
                    long chk = 0;
                    for (long j = 0; j < iterations; ++j) chk += check(make(depth));
                    lines[i] = iterations + "\t trees of depth " + depth + "\t check: " + chk;
                }
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        for (String l : lines) System.out.println(l);
        System.out.printf("long lived tree of depth %d\t check: %d%n", maxDepth, check(longLived));
        System.out.printf("wall=%.2fs cpu=%.2fs%n", (System.nanoTime() - t0) / 1e9, Common.cpuSeconds());
    }
}
