package java_evaluation;

import java_evaluation.jppzanetti.PQRTree;

public class JppzanettiAdapter extends PQRTree implements ConsecutiveOnesInterface {
	long time = 0;
	int[] previousRestriction;

	public JppzanettiAdapter(int numLeaves) {
		super(numLeaves);
	}

	@Override
	public boolean applyRestriction(int[] restriction) {
		previousRestriction = restriction;
		long restrictionStart = System.nanoTime();
		boolean result = super.reduce(restriction);
		time = System.nanoTime() - restrictionStart;
		
		return result;
	}

	@Override
	public long getTime() {
		return time;
	}

	@Override
	public void cleanUp() {
		long cleanUpStart = System.nanoTime();
		//uncolor(previousRestriction);
		time = System.nanoTime() - cleanUpStart;
	}

	@Override
	public String getFingerprint() {
		String orders = getPossibleOrders().toString();
		return orders.substring(0, Math.min(5, orders.length())) + orders.length();
	}

	public static String getFingerprint(PQRTree tree) {
		String orders = tree.getPossibleOrders().toString();
		return orders.substring(0, Math.min(5, orders.length())) + orders.length();
	}
}
