package java_evaluation;

import java.util.Vector;

import java_evaluation.jgraphed.PQNode;
import java_evaluation.jgraphed.PQTree;

public class JGraphEdAdapter extends PQTree implements ConsecutiveOnesInterface {
	PQNode[] mapping;
	long time = 0;
	
	public JGraphEdAdapter(int numLeaves) throws Exception {
		super(numLeaves);
		mapping = new PQNode[numLeaves];
		
		for (int i = 0; i < numLeaves; i++) {
			mapping[i] = getLeafAt(i);
		}
	}

	@Override
	public boolean applyRestriction(int[] restriction) throws Exception {
		Vector<PQNode> v = new Vector<>();
		for (int i : restriction) {
			v.add(mapping[i]);
		}
		
		long restrictionStart = System.nanoTime();
		reduction(v);
		time = System.nanoTime() - restrictionStart;
		
		return !isNullTree();
	}

	@Override
	public void cleanUp() throws Exception {
		long restrictionStart = System.nanoTime();
		clear();
		time = System.nanoTime() - restrictionStart;
	}

	@Override
	public long getTime() {
		return time;
	}

	@Override
	public String getFingerprint() {
		String orders = getPossibleOrders().toString();
		return orders.substring(0, Math.min(5, orders.length())) + orders.length();
	}

}
