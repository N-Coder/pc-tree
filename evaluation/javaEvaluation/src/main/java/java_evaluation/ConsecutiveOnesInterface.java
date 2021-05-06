package java_evaluation;


public interface ConsecutiveOnesInterface {

	public boolean applyRestriction(int[] restriction) throws Exception;
	
	public void cleanUp() throws Exception;
	
	public long getTime();
	
	public String getFingerprint();

	public default String uniqueID() {
		return uniqueID(true, true);
	}

	public default String uniqueID(boolean normalizeLeafIDs, boolean includeInnerNodeOrder) {
		return "";
	}
}
