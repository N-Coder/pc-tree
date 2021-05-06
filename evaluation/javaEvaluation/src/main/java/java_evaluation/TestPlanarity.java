package java_evaluation;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ArrayNode;
import com.fasterxml.jackson.databind.node.ObjectNode;
import java_evaluation.jppzanetti.PQRTree;
import org.apache.commons.cli.*;

import java.io.BufferedReader;
import java.io.FileReader;

public class TestPlanarity {
	public static void main(String[] args) throws Exception {
		Options options = new Options();

		int minRestrictionSize = 25;
		Option minRestrictionSizeOption = new Option("s", "size", true, "minRestrictionSize");
		minRestrictionSizeOption.setRequired(false);
		minRestrictionSizeOption.setType(Number.class);
		options.addOption(minRestrictionSizeOption);

		int checkpointInterval = 1000;
		Option checkpointIntervalOption = new Option("c", "checkpoint", true, "checkpointInterval");
		checkpointIntervalOption.setRequired(false);
		checkpointIntervalOption.setType(Number.class);
		options.addOption(checkpointIntervalOption);

		int repetitions = 3;
		Option repetitionsOption = new Option("r", "repetitions", true, "repetitions");
		repetitionsOption.setRequired(false);
		repetitionsOption.setType(Number.class);
		options.addOption(repetitionsOption);

		CommandLineParser parser = new DefaultParser();
		CommandLine cmd = parser.parse(options, args);

		if (cmd.hasOption("s"))
			minRestrictionSize = ((Number) cmd.getParsedOptionValue("s")).intValue();
		if (cmd.hasOption("c"))
			checkpointInterval = ((Number) cmd.getParsedOptionValue("c")).intValue();
		if (cmd.hasOption("r"))
			repetitions = ((Number) cmd.getParsedOptionValue("r")).intValue();

		assert cmd.getArgList().size() == 1;
		String filePath = cmd.getArgList().get(0);

		ObjectMapper mapper = new ObjectMapper();
		for (int i = 0; i < repetitions; i++) {
			long totalTime = 0;
			try (BufferedReader br = new BufferedReader(new FileReader(filePath))) {
				String line = br.readLine();
				ArrayNode init = (ArrayNode) mapper.readTree(line).get("replace");
				PQRTree tree = new PQRTree(init.size());

				while ((line = br.readLine()) != null) {
					JsonNode root = mapper.readTree(line);
					String filename = root.get("file").asText();
					int index = root.get("index").asInt();
					ArrayNode consecutiveNode = (ArrayNode) root.get("consecutive");
					ArrayNode replaceNode = (ArrayNode) root.get("replace");
					int[] consecutive = parseRestriction(consecutiveNode);
					int[] replace = parseRestriction(replaceNode);

					long restrictionStart = System.nanoTime();
					boolean possible;
					String exception = "";
					try {
						possible = tree.reduce(consecutive);
					} catch (Exception e) {
					    possible = false;
					    exception = e.toString();
					}
					long restrictionTime = System.nanoTime() - restrictionStart;
					totalTime += restrictionTime;

					if (i == repetitions - 1 && (consecutive.length >= minRestrictionSize || index % checkpointInterval == 0 || !possible)) {
						ObjectNode restrictionNode = mapper.createObjectNode();
						restrictionNode.put("file", filename);
						restrictionNode.put("index", index);
						restrictionNode.put("type", "Zanetti");
						restrictionNode.put("time", restrictionTime);
						restrictionNode.put("size", consecutive.length);
						restrictionNode.put("result", possible);

						if (!exception.isEmpty()) {
						    restrictionNode.put("exception", exception);
						}

						if (index % checkpointInterval == 0) {
							restrictionNode.put("fingerprint", JppzanettiAdapter.getFingerprint(tree));
							restrictionNode.put("tree", tree.toString());
							restrictionNode.put("uid", tree.uniqueID(false, false));
						}
						System.out.println(restrictionNode);
					}
					if (!possible) break;

                    try {
					    tree.mergeAndReplaceLeaves(consecutive, replace);
                    } catch (Exception e) {
                        ObjectNode restrictionNode = mapper.createObjectNode();
                        restrictionNode.put("file", filename);
                        restrictionNode.put("index", index);
                        restrictionNode.put("type", "Zanetti");
                        restrictionNode.put("exception", e.toString());
                        System.out.println(restrictionNode);
                        break;
                    }
				}

				if (i == repetitions - 1) {
					System.err.println(totalTime);
				}
			}
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
