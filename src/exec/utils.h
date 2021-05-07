#pragma once

#include "PCTree.h"
#include <ogdf/basic/PQTree.h>

#include "adapters/C1IUtils.h"

#include "adapters/UnionFindPCTreeAdapter.h"
#include "adapters/HsuPCTreeAdapter.h"
#include "adapters/OgdfPQTreeAdapter.h"
#ifdef OTHER_LIBS
#include "adapters/ReislePQTreeAdapter.h"
#include "adapters/GregablePQTreeAdapter.h"
#include "adapters/BiVocPQTreeAdapter.h"
#include "adapters/GraphSetPQTreeAdapter.h"
#include "adapters/CppZanettiPQRTreeAdapter.h"
#endif

json dumpMatrix(int cols, const std::vector<std::vector<int>> &restrictions, const std::string &id, json &out,
                std::ostream &outstream = std::cout) {
    UnionFindPCTreeAdapter T;
    return dumpMatrix(cols, restrictions, id, out, &T, outstream);
}

std::unique_ptr<ConsecutiveOnesInterface> getAdapter(TreeType treeType) {
    std::unique_ptr<ConsecutiveOnesInterface> testTree;
    switch (treeType) {
        case TreeType::UFPC:
            return std::make_unique<UnionFindPCTreeAdapter>();
        case TreeType::HsuPC:
            return std::make_unique<HsuPCTreeAdapter>();
        case TreeType::OGDF:
            return std::make_unique<OgdfPQTreeAdapter>();
#ifdef OTHER_LIBS
        case TreeType::BiVoc:
            return std::make_unique<BiVocPQTreeAdapter>();
        case TreeType::Reisle:
            return std::make_unique<ReislePQTreeAdapter>();
        case TreeType::Gregable:
            return std::make_unique<GregablePQTreeAdapter>();
        case TreeType::GraphSet:
            return std::make_unique<GraphSetPQTreeAdapter>();
        case TreeType::CppZanetti:
            return std::make_unique<CppZanettiPQRTreeAdapter>();
#endif
        case TreeType::Invalid:
        default:
            std::cerr << "Invalid TreeType parameter" << std::endl;
            return nullptr;
    }
}

std::unique_ptr<ConsecutiveOnesInterface> getAdapter(const std::string &type) {
    return getAdapter(json::parse(string("\"") + type + "\"").get<TreeType>());
}

std::unique_ptr<ConsecutiveOnesInterface> getAdapter(char *type) {
    return getAdapter(std::string(type));
}
