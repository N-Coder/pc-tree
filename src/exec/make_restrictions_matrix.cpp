#include "utils.h"

#include <iostream>
#include <getopt.h>
#include <filesystem>

int main(int argc, char **argv) {
    int minSize = 0;
    int maxSize = 0;
    int seed = 0;

    int c;
    while ((c = getopt(argc, argv, "l:h:s:")) != -1) {
        switch (c) {
            case 'l':
                minSize = std::stoi(optarg);
                break;
            case 'h':
                maxSize = std::stoi(optarg);
                break;
            case 's':
                seed = std::stoi(optarg);
                break;
            default:
                std::cerr << "Invalid arguments" << std::endl;
                return 1;
        }
    }

    if (optind >= argc) {
        std::cerr << "Expected output directory for matrices" << std::endl;
        return 1;
    }

    if (minSize <= 0) {
        std::cerr << "Missing min size for cols and rows." << std::endl;
        return 1;
    }
    if (maxSize <= 0) {
        std::cerr << "Missing max size for cols and rows" << std::endl;
        return 1;
    }
    if (minSize > maxSize) {
        std::cerr << "Invalid size bounds";
        return 1;
    }

    json matrixJson;
    std::filesystem::path matrixDir = argv[optind++];

    if (!std::filesystem::exists(matrixDir)) {
        std::cerr << "Output directory does not exist." << std::endl;
        return 1;
    }

    std::mt19937 generator(seed);
    std::uniform_int_distribution<> uniform(minSize, maxSize);
    int rows = uniform(generator);
    int cols = uniform(generator);
    matrixJson["rows"] = rows;
    matrixJson["cols"] = cols;
    matrixJson["seed"] = seed;

    std::string matrixId;
    {
        std::stringstream sb;
        sb << "M-m" << cols << "-n" << rows << "-s" << seed;
        matrixId = matrixJson["id"] = sb.str();
    }
    std::vector<std::vector<bool>> restrictions(cols, std::vector<bool>(rows, false));
    std::uniform_int_distribution<> uniformStart(0, cols - 3);
    for (int i = 0; i < rows; ++i) {
        int start = uniformStart(generator);
        std::uniform_int_distribution<> uniformEnd(start + 1, cols - 2);
        int end = uniformEnd(generator);
        for (int j = start; j <= end; ++j) {
            restrictions.at(j).at(i) = true;
        }
    }
    std::shuffle(restrictions.begin(), restrictions.end(), generator);
    std::vector<json> outputRestrictions;
    outputRestrictions.reserve(rows);
    std::unique_ptr<ConsecutiveOnesInterface> T = std::make_unique<UnionFindPCTreeAdapter>();
    T->initTree(cols);
    for (int i = 0; i < rows; ++i) {
        auto &back = outputRestrictions.emplace_back();
        std::vector<int> consecutive;
        for (int j = 0; j < cols; ++j) {
            if (restrictions.at(j).at(i)) {
                consecutive.emplace_back(j);
            }
        }
        back["id"] = i;
        back["parent_id"] = matrixId;
        back["consecutive"] = consecutive;
        back["valid"] = false;
        bool possible = back["exp_possible"] = T->applyRestriction(consecutive);
        back["size"] = consecutive.size();
        json stats = T->stats();
        back["c_nodes"] = stats["c_nodes"];
        back["p_nodes"] = stats["p_nodes"];
        {
            std::stringstream sb;
            T->uniqueID(sb);
            back["exp_uid"] = sb.str();
        }
        back["exp_fingerprint"] = fingerprint(T->possibleOrders());
        back["tpLength"] = T->terminalPathLength();
        if (!possible) {
            break;
        }
    }
    matrixJson["restrictions"] = std::move(outputRestrictions);

    std::filesystem::path matrixFile(matrixDir);
    matrixFile /= matrixId;

    std::ofstream outFile(matrixFile, std::ofstream::out | std::ofstream::trunc);
    outFile << matrixJson;

    json outputJson;
    outputJson["path"] = matrixFile.string();
    outputJson["id"] = matrixId;
    std::cout << "MATRIX:" << outputJson << std::endl;
    return 0;
}