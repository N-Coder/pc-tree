package java_evaluation;

import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

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

		CommandLineParser parser = new DefaultParser();
		CommandLine cmd = parser.parse(options, args);

		TreeType treeType = TreeType.valueOf(cmd.getOptionValue("t"));
		String tname = cmd.getOptionValue("n");
		String[] filePaths = cmd.getArgs();

		ObjectMapper mapper = new ObjectMapper();
		for (int i = 0; i < REPETITIONS; i++) {
			for (String filePath : filePaths) {
				ObjectNode matrix = (ObjectNode) mapper.readTree(new FileReader(filePath));
				try {
					matrix.put("id", tname + "/" + matrix.get("id").asText());
					matrix.putArray("errors");
					processMatrix(matrix, treeType, tname);
				} catch (Exception | OutOfMemoryError e) {
					ArrayNode errors = (ArrayNode) matrix.get("errors");
					StringWriter sw = new StringWriter();
					PrintWriter pw = new PrintWriter(sw);
					e.printStackTrace(pw);
					errors.add(sw.toString());
					matrix.put("valid", false);
					matrix.put("tree_type", treeType.toString());
					matrix.put("name", tname);
					matrix.remove("restrictions");
				}
				
				if (i == REPETITIONS - 1) {
					System.out.println(matrix);
				}
			}
		}
	}

	private static void processMatrix(ObjectNode matrix, TreeType treeType, String tname) throws Exception {
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
		ArrayNode errors = (ArrayNode) matrix.get("errors");
		boolean complete = false;
		for (int i = 0; i < restrictions.size(); ++i) {
			JsonNode consecutive = restrictions.get(i);

			boolean possible = tree.applyRestriction(parseRestriction(consecutive));
			long restrictionTime = tree.getTime();

			tree.cleanUp();
			long cleanUpTime = tree.getTime();

			totalRestrictionTime += restrictionTime;
			totalCleanupTime += cleanUpTime;
			
			
			boolean last = i == restrictions.size() - 1;
			if (!last && !possible) {
				errors.add("possible");
				break;
			}
			
			if (last) {
				ObjectNode lastRestriction = (ObjectNode) matrix.get("last_restriction");
				lastRestriction.put("time", restrictionTime);
				lastRestriction.put("cleanup_time", cleanUpTime);
				lastRestriction.put("possible", possible);
				String fingerprint = tree.getFingerprint();
				lastRestriction.put("fingerprint", fingerprint);
				String uniqueID = tree.uniqueID();
				lastRestriction.put("uid", uniqueID);
				if (tree instanceof JppzanettiAdapter)
					lastRestriction.put("tree", tree.toString());
				
				if (possible && !fingerprint.equals(lastRestriction.get("exp_fingerprint").asText())) {
					errors.add("fingerprint");
				}
				if (possible && !lastRestriction.get("exp_uid").asText().isEmpty() && !uniqueID.isEmpty() && !lastRestriction.get("exp_uid").asText().equals(uniqueID)) {
					errors.add("uid");
				}
				if (possible != lastRestriction.get("exp_possible").asBoolean()) {
					errors.add("possible");
				}
				
				complete = true;
			}
		}

		matrix.put("tree_type", treeType.toString());
		matrix.put("name", tname);
		matrix.put("total_time", initTime + totalCleanupTime + totalRestrictionTime);
		matrix.put("total_restriction_time", totalRestrictionTime);
		matrix.put("total_cleanup_time", totalCleanupTime);
		matrix.put("complete", complete);
		matrix.put("valid", errors.isEmpty());
		matrix.remove("restrictions");
	}

	private static int[] parseRestriction(JsonNode consecutive) {
		int[] restriction = new int[consecutive.size()];
		for (int i = 0; i < consecutive.size(); i++) {
			restriction[i] = consecutive.get(i).asInt();
		}
		return restriction;
	}

}
