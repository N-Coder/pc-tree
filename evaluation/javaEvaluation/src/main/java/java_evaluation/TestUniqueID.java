package java_evaluation;

import java_evaluation.jppzanetti.PQRTree;

public class TestUniqueID {
	static void check(String actual, String expected) {
		if (!actual.equals(expected)) {
			System.err.println("Expected: "+expected);
			System.err.println("Actual:   "+actual);
		} else {
			System.out.println(expected);
		}
	}

	public static void main(String[] args) throws Exception {
		PQRTree tree = new PQRTree(7);
		check(tree.uniqueID(), "7:(6, 5, 4, 3, 2, 1, 0)");
		tree.reduce(new int[]{4, 5});
		check(tree.uniqueID(), "8:(7:[5, 4], 6, 3, 2, 1, 0)");
		tree.reduce(new int[]{3, 4, 5});
		check(tree.uniqueID(), "9:(8:[7:[5, 4], 3], 6, 2, 1, 0)");
		tree.reduce(new int[]{0, 1});
		check(tree.uniqueID(), "10:(9:[8:[5, 4], 3], 7:[1, 0], 6, 2)");
		tree.reduce(new int[]{1, 2});
		check(tree.uniqueID(), "10:[9:[8:[5, 4], 3], 7:[2, 1, 0], 6]");
		tree.reduce(new int[]{2, 3, 4, 5});
		check(tree.uniqueID(), "9:[6, 8:[7:[5, 4], 3], 2, 1, 0]"); // Actual: 10:[9:[8:[7:[5, 4], 3], 2, 1, 0], 6]
		tree.reduce(new int[]{3, 4, 5, 6});
		check(tree.uniqueID(), "9:[6, 8:[7:[5, 4], 3], 2, 1, 0]"); // Inconsistent Tree
		tree.reduce(new int[]{3, 4});
		check(tree.uniqueID(), "8:[6, 7:[5, 4, 3], 2, 1, 0]");
		tree.reduce(new int[]{2, 3, 4, 5});
		check(tree.uniqueID(), "8:[6, 7:[5, 4, 3], 2, 1, 0]");
	}
}
