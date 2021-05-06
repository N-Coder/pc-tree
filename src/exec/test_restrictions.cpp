#include "likwid_utils.h"

#include "utils.h"

#include <iostream>
#include <getopt.h>
#include <csignal>
#include <limits>
#include <csetjmp>


jmp_buf env;
volatile sig_atomic_t previousSig = 0;

void signalHandler(int sig) {
    previousSig = sig;
    siglongjmp(env, 1);
}


int main(int argc, char **argv) {
    pc_tree::PCTree T;
    std::string name;
    std::unique_ptr<ConsecutiveOnesInterface> testTree;
    bool accumulate = false;
    int max_idx = std::numeric_limits<int>::max();

    int c;
    while ((c = getopt(argc, argv, "m:t:n:a")) != -1) {
        switch (c) {
            case 'a':
                accumulate = true;
                break;
            case 'm':
                max_idx = std::stoi(optarg);
                break;
            case 't':
                testTree = getAdapter(optarg);
                break;
            case 'n':
                name = optarg;
                break;
            default:
                std::cerr << "Invalid arguments" << std::endl;
                return 1;
        }
    }

    if (optind >= argc) {
        cerr << "Expected infile!" << std::endl;
        return 1;
    }
    std::ifstream inputFile;
    inputFile.open(argv[optind++]);
    if (!inputFile.good()) {
        cerr << "Could not open input file!" << std::endl;
        return 1;
    }
    if (optind < argc) {
        cerr << "Too many positional arguments!" << std::endl;
        return 1;
    }

    if (!testTree) {
        std::cerr << "Missing tree type selection" << std::endl;
        return 1;
    }

    json matrix;
    inputFile >> matrix;
    matrix["tree_type"] = testTree->type();
    if (name.empty()) {
        name = matrix["tree_type"].get<std::string>();
    }
    matrix["name"] = name;
    std::string id;
    {
        std::stringstream sb;
        sb << name << "/" << matrix["id"].get<std::string>();
        matrix["id"] = id = sb.str();
    }

    signal(SIGSEGV, signalHandler);
    signal(SIGUSR1, signalHandler);

    likwid_prepare(matrix, accumulate);

    long totalRestrictTime = 0;
    long totalCleanupTime = 0;
    long initTime = 0;
    long rowIdx = 0;
    std::vector<json> rowsResults;
    std::vector<std::string> errors;
    if (sigsetjmp(env, 1) == 0) {
        testTree->initTree(matrix["cols"].get<int>());
        initTime = testTree->getTime();
        json &restrictions = matrix["restrictions"];
        for (auto it = restrictions.begin(); it != restrictions.end(); ++it) {
            bool last = std::next(it) == restrictions.end();
            auto restriction = *it;
            likwid_before_it();

            json &lastRestriction = matrix["last_restriction"];
            bool possible;
            try {
                possible = testTree->applyRestriction(restriction);
            } catch (const std::exception &e) {
                possible = false;
                errors.emplace_back(std::string("Exception: ") + e.what());
            }
            long restrictionTime = testTree->getTime();
            testTree->cleanUp();
            long cleanUpTime = testTree->getTime();
            totalRestrictTime += restrictionTime;
            totalCleanupTime += cleanUpTime;

            if (!last && !possible) {
                errors.emplace_back("possible");
                break;
            }

            if (last) {
                lastRestriction["time"] = restrictionTime;
                lastRestriction["cleanup_time"] = cleanUpTime;
                lastRestriction["fingerprint"] = fingerprint(testTree->possibleOrders());
                lastRestriction["possible"] = possible;
                {
                    std::stringstream sb;
                    testTree->uniqueID(sb);
                    lastRestriction["uid"] = sb.str();
                }
                lastRestriction["tree"] = testTree->toString();
                if (lastRestriction["tree"].get_ref<std::string&>().empty()) {
                    lastRestriction.erase("tree");
                }
                if (possible && lastRestriction["fingerprint"] != lastRestriction["exp_fingerprint"]) {
                    errors.emplace_back("fingerprint");
                }
                if (possible && !lastRestriction["uid"].get<std::string>().empty() && !lastRestriction["uid"].get<std::string>().empty() &&
                    lastRestriction["uid"] != lastRestriction["exp_uid"]) {
                    errors.emplace_back("uid");
                }
                if (possible != lastRestriction["exp_possible"]) {
                    errors.emplace_back("possible");
                }
            }
            rowIdx++;
            likwid_after_it(matrix);
        }
    } else {
        matrix["signal"] = previousSig;
        errors.emplace_back("signal");
        testTree.release(); // Avoid another signal when calling the destructor
    }
    matrix["valid"] = errors.empty();
    matrix["errors"] = std::move(errors);
    matrix["init_time"] = initTime;
    matrix["total_restrict_time"] = totalRestrictTime;
    matrix["total_cleanup_time"] = totalCleanupTime;
    matrix["total_time"] = initTime + totalRestrictTime + totalCleanupTime;
    matrix["complete"] = (rowIdx == matrix["rows"].get<int>());
    matrix.erase("restrictions");

    likwid_finalize(matrix);

    std::cout << matrix << std::endl;

    return 0;
}