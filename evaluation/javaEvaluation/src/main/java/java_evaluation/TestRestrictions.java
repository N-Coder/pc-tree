package java_evaluation;

import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;

import com.fasterxml.jackson.databind.node.JsonNodeFactory;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.DefaultParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;

enum TreeType {
	Zanetti, JGraphEd, GraphTea;
}

public class TestRestrictions {

	public static final int REPETITIONS = 10;

	public static void main(String[] args) throws IOException, ParseException {
		Options options = new Options();

		Option treeTypeOption = new Option("t", "tree-type", true, "tree type to use");
		treeTypeOption.setRequired(true);
		options.addOption(treeTypeOption);
		
		Option name = new Option("n", "name", true, "name");
		name.setRequired(true);
		options.addOption(name);

		Option allRestrictionsOption = new Option("A", "all-restrictions", false,
				"collect data for all restrictions of the matrix");
		allRestrictionsOption.setRequired(false);
		options.addOption(allRestrictionsOption);

		CommandLineParser parser = new DefaultParser();
		CommandLine cmd = parser.parse(options, args);

		TreeType treeType = TreeType.valueOf(cmd.getOptionValue("t"));
		String tname = cmd.getOptionValue("n");
		boolean allRestrictions = cmd.hasOption("A");
		String[] filePaths = cmd.getArgs();

		ObjectMapper mapper = new ObjectMapper();
		for (int i = 0; i < REPETITIONS; i++) {
			for (String filePath : filePaths) {
				ObjectNode matrix = (ObjectNode) mapper.readTree(new FileReader(filePath));
				try {
					matrix.put("id", tname + "/" + matrix.get("id").asText());
					matrix.putArray("errors");
					processMatrix(matrix, treeType, tname, allRestrictions);
				} catch (Exception | OutOfMemoryError | AssertionError e) {
					ArrayNode errors = (ArrayNode) matrix.get("errors");
					StringWriter sw = new StringWriter();
					PrintWriter pw = new PrintWriter(sw);
					e.printStackTrace(pw);
					errors.add(sw.toString());
					matrix.put("valid", false);
					matrix.put("tree_type", treeType.toString());
					matrix.put("name", tname);
					if (!allRestrictions) {
						matrix.remove("restrictions");
					}
				}
				
				if (i == REPETITIONS - 1) {
					System.out.println(matrix);
				}
			}
		}
	}

	private static void processMatrix(ObjectNode matrix, TreeType treeType, String tname, boolean allRestrictions) throws Exception {
		int numLeaves = matrix.get("cols").asInt();

		ConsecutiveOnesInterface tree;
		long initStart = System.nanoTime();
		switch (treeType) {
		case JGraphEd:
			tree = new JGraphEdAdapter(numLeaves);
			break;
		case Zanetti:
			tree = new JppzanettiAdapter(numLeaves);
			break;
		case GraphTea:
			tree = new GraphTeaAdapter(numLeaves);
			break;
		default:
			throw new IllegalStateException("Invalid tree type");
		}
		long initTime = System.nanoTime() - initStart;
		matrix.put("init_time", initTime);
		
		long totalRestrictionTime = 0;
		long totalCleanupTime = 0;
		ArrayNode restrictions = (ArrayNode) matrix.get("restrictions");
		Set<String> matrixErrors = new HashSet<>();
		boolean complete = false;
		for (int i = 0; i < restrictions.size(); ++i) {
			ArrayList<String> restrictionErrors = new ArrayList<>();
			JsonNode consecutive = allRestrictions ? restrictions.get(i).get("consecutive") : restrictions.get(i);

			boolean possible = tree.applyRestriction(parseRestriction(consecutive));
			long restrictionTime = tree.getTime();

			tree.cleanUp();
			long cleanUpTime = tree.getTime();

			totalRestrictionTime += restrictionTime;
			totalCleanupTime += cleanUpTime;
			
			
			boolean last = i == restrictions.size() - 1;
			if (!last && !possible) {
				restrictionErrors.add("possible");
				break;
			}

			if (last || allRestrictions) {
				ObjectNode restrictionResults = (ObjectNode) (allRestrictions ? restrictions.get(i) : matrix.get("last_restriction"));
				restrictionResults.put("time", restrictionTime);
				restrictionResults.put("cleanup_time", cleanUpTime);
				restrictionResults.put("possible", possible);
				String fingerprint = tree.getFingerprint();
				restrictionResults.put("fingerprint", fingerprint);
				String uniqueID = tree.uniqueID();
				restrictionResults.put("uid", uniqueID);
				if (tree instanceof JppzanettiAdapter)
					restrictionResults.put("tree", tree.toString());
				
				if (possible && !fingerprint.equals(restrictionResults.get("exp_fingerprint").asText())) {
					restrictionErrors.add("fingerprint");
				}
				if (possible && !restrictionResults.get("exp_uid").asText().isEmpty() && !uniqueID.isEmpty() && !restrictionResults.get("exp_uid").asText().equals(uniqueID)) {
					restrictionErrors.add("uid");
				}
				if (possible != restrictionResults.get("exp_possible").asBoolean()) {
					restrictionErrors.add("possible");
				}
				matrixErrors.addAll(restrictionErrors);
				restrictionResults.put("errors", String.valueOf(restrictionErrors));
				restrictionResults.put("valid", restrictionErrors.isEmpty());
				if (allRestrictions) {
					restrictionResults.remove("consecutive");
				}
				complete = true;
			}
		}
		ArrayNode errors = (ArrayNode) matrix.get("errors");
		for (String error: matrixErrors) {
			errors.add(error);
		}
		matrix.put("tree_type", treeType.toString());
		matrix.put("name", tname);
		matrix.put("total_time", initTime + totalCleanupTime + totalRestrictionTime);
		matrix.put("total_restriction_time", totalRestrictionTime);
		matrix.put("total_cleanup_time", totalCleanupTime);
		matrix.put("complete", complete);
		matrix.put("valid", matrixErrors.isEmpty());
		if (!allRestrictions) {
			matrix.remove("restrictions");
		}
	}

	private static int[] parseRestriction(JsonNode consecutive) {
		int[] restriction = new int[consecutive.size()];
		for (int i = 0; i < consecutive.size(); i++) {
			restriction[i] = consecutive.get(i).asInt();
		}
		return restriction;
	}

}
