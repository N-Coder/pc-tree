#pragma once

#include "ConsecutiveOnesInterface.h"
#include <json.hpp>
#include <sstream>

using namespace ogdf;
using namespace pc_tree;
using nlohmann::json;

std::string fingerprint(const Bigint &orders) {
    return to_string(orders).substr(0, 5) + std::to_string(to_string(orders).length());
}

void getRestrictions(PCTree &T, std::vector<std::vector<int>> &out, PCNode *startLeaf = nullptr, std::vector<PCNode *> *append = nullptr) {
    PCTreeNodeArray<int> leafMapping(T);
    int i = 0;
    std::vector<PCNode *> sortedLeaves = T.getLeaves();
    std::sort(sortedLeaves.begin(), sortedLeaves.end(), [] (PCNode *n1, PCNode *n2) {
        return n1->index() < n2->index();
    });
    for (PCNode *leaf : sortedLeaves)
        leafMapping[leaf] = i++;
    std::vector<std::vector<PCNode *>> restrictions;
    T.getRestrictions(restrictions, startLeaf);
    if (append != nullptr)
        restrictions.emplace_back(*append);
    for (std::vector<PCNode *> &restriction : restrictions) {
        std::vector<int> &back = out.emplace_back();
        for (PCNode *leaf : restriction)
            back.push_back(leafMapping[leaf]);
    }
}

json dumpMatrix(int cols, const std::vector<std::vector<int>> &restrictions, const std::string &id, json &out,
                ConsecutiveOnesInterface *T, std::ostream &outstream = std::cout) {
    out["id"] = id;
    out["cols"] = cols;
    int rows = out["rows"] = restrictions.size();

    T->initTree(cols);

    int num_ones = 0;
    int max_size = 0;
    bool possible = true;
    std::vector<json> rowsJson;
    for (const std::vector<int> &restriction : restrictions) {
        OGDF_ASSERT(possible);
        json rowJson;
        int row_idx = rowJson["idx"] = rowsJson.size();
        int size = rowJson["size"] = restriction.size();
        bool last = rowJson["last"] = restriction == restrictions.back();
        OGDF_ASSERT(!isTrivialRestriction(size, cols));
        rowJson["restriction"] = restriction;
        rowJson["fraction"] = ((double) size) / cols;
        rowJson["parent_id"] = id;
        {
            std::stringstream sb;
            sb << id << "_" << row_idx;
            rowJson["id"] = sb.str();
            rowsJson.emplace_back(sb.str());
        }

        rowJson["possible"] = possible = T->applyRestriction(restriction);
        rowJson["tp_length"] = T->terminalPathLength();
        rowJson["tree"] = T->stats();
        rowJson["tree"]["type"] = T->type();
        rowJson["tree"]["fingerprint"] = fingerprint(T->possibleOrders());
        {
            std::stringstream sb;
            T->uniqueID(sb);
            rowJson["tree"]["uid"] = sb.str();
        }
        if (last)
            out["last_tree"] = rowJson["tree"];
        outstream << "RESTRICTION:" << rowJson << std::endl;

        num_ones += size;
        max_size = max(max_size, size);
    }
    out["matrix"] = std::move(rowsJson);
    out["num_ones"] = num_ones;
    out["fraction"] = num_ones / (((double) cols) * rows);
    out["max_size"] = max_size;
    out["max_fraction"] = ((double) max_size) / cols;
    out["possible"] = possible;

    return possible;
}
