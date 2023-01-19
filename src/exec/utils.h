#pragma once

#include "PCTree.h"
#include <ogdf/basic/PQTree.h>

#include "adapters/C1IUtils.h"

#include "adapters/UnionFindPCTreeAdapter.h"
#include "adapters/HsuPCTreeAdapter.h"
#include "adapters/OgdfPQTreeAdapter.h"
#ifdef OTHER_LIBS
#ifdef ENABLE_REISLE
#include "adapters/ReislePQTreeAdapter.h"
#endif
#ifdef ENABLE_GREGABLE
#include "adapters/GregablePQTreeAdapter.h"
#endif
#ifdef ENABLE_BIVOC
#include "adapters/BiVocPQTreeAdapter.h"
#endif
#ifdef ENABLE_GRAPHSET
#include "adapters/GraphSetPQTreeAdapter.h"
#endif
#ifdef ENABLE_CPPZANETTI
#include "adapters/CppZanettiPQRTreeAdapter.h"
#endif
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
#ifdef ENABLE_BIVOC
        case TreeType::BiVoc:
            return std::make_unique<BiVocPQTreeAdapter>();
#endif
#ifdef ENABLE_REISLE
        case TreeType::Reisle:
            return std::make_unique<ReislePQTreeAdapter>();
#endif
#ifdef ENABLE_GREGABLE
        case TreeType::Gregable:
            return std::make_unique<GregablePQTreeAdapter>();
#endif
#ifdef ENABLE_GRAPHSET
        case TreeType::GraphSet:
            return std::make_unique<GraphSetPQTreeAdapter>();
#endif
#ifdef ENABLE_CPPZANETTI
        case TreeType::CppZanetti:
            return std::make_unique<CppZanettiPQRTreeAdapter>();
#endif
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
