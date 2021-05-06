package java_evaluation;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;

import java_evaluation.graphtea.LeafNode;
import java_evaluation.graphtea.PNode;
import java_evaluation.graphtea.PQHelpers;
import java_evaluation.graphtea.PQNode;
import java_evaluation.graphtea.PQTree;
import java_evaluation.graphtea.QNode;

public class GraphTeaAdapter extends PQTree implements ConsecutiveOnesInterface{
	private PQNode[] leaves;
	private PNode root;
	private long time;

	public GraphTeaAdapter(int numLeaves) {
		leaves = new PQNode[numLeaves];
		this.numLeaves = numLeaves;
		root = new PNode(PQNode.EMPTY);

		for (int i = 0; i < numLeaves; i++) {
			PQNode leafNode = new LeafNode();
			leafNode.setParent(root);
			root.addChild(leafNode);
			leafNode.setLabel(PQNode.EMPTY);
			leaves[i] = leafNode;
		}
	}

	@Override
	public boolean applyRestriction(int[] restriction) throws Exception {
		List<PQNode> consecutiveLeaves = new ArrayList<>(restriction.length);
		for (int i : restriction) {
			consecutiveLeaves.add(leaves[i]);
		}
		
		long restrictionStart = System.nanoTime();
		for (PQNode leaf : consecutiveLeaves) {
			leaf.setLabel(PQNode.FULL);
		}
		PNode tempRoot = bubble(root, consecutiveLeaves);
		if (tempRoot != null) {
			tempRoot = reduce(tempRoot, consecutiveLeaves);
		}
		
		time = System.nanoTime() - restrictionStart;

		if (tempRoot != null) {
			root = tempRoot;
		}
		return tempRoot != null;
	}
	
	public void cleanUp() {
		long cleanUpStart = System.nanoTime();
		PQHelpers.reset(root, true, true);
		time = System.nanoTime() - cleanUpStart;
	}

	@Override
	public String getFingerprint() {
		BigInteger orders = BigInteger.ONE;
		Queue<PQNode> queue = new LinkedList<>();
		
		queue.add(root);
		
		while (!queue.isEmpty()) {
			PQNode nextNode = queue.poll();
			
			if (nextNode instanceof PNode) {
				PNode pnode = (PNode) nextNode;
				int childCount = nextNode == root ? nextNode.getChildren().size() - 1 : nextNode.getChildren().size(); 
				
				for (int i = 2; i <= childCount; i++) {
					orders = orders.multiply(BigInteger.valueOf(i));
				}
				
				for (PQNode child : pnode.getChildren()) {
					if (!(child instanceof LeafNode)) {
						queue.add(child);
					}
				}
			} else if (nextNode instanceof QNode) {
				QNode qnode = (QNode) nextNode;
				orders = orders.multiply(BigInteger.valueOf(2));
			
				for (PQNode child : qnode.getChildren()) {
					if (!(child instanceof LeafNode)) {
						queue.add(child);
					}
				}
			}
		}
		
		return orders.toString().substring(0, Math.min(5, orders.toString().length())) + orders.toString().length();
	}

	@Override
	public long getTime() {
		return time;
	}
}
