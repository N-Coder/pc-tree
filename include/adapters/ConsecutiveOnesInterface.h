#pragma once

#include <json.hpp>
#include <bigInt/bigint.h>

#include <chrono>

using std::chrono::time_point;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using nlohmann::json;
using namespace Dodecahedron;

enum class TreeType {
    Invalid,
    UFPC,
    HsuPC,
    BiVoc,
    Reisle,
    Gregable,
    OGDF,
    GraphSet,
    CppZanetti,
};

NLOHMANN_JSON_SERIALIZE_ENUM(TreeType, {
    { TreeType::Invalid, "Invalid" },
    { TreeType::UFPC, "UFPC" },
    { TreeType::HsuPC, "HsuPC" },
    { TreeType::BiVoc, "BiVoc" },
    { TreeType::Reisle, "Reisle" },
    { TreeType::Gregable, "Gregable" },
    { TreeType::OGDF, "OGDF" },
    { TreeType::GraphSet, "GraphSet" },
    { TreeType::CppZanetti, "CppZanetti" },
})

class ConsecutiveOnesInterface {
protected:
    long time = 0;

public:

    virtual ~ConsecutiveOnesInterface() = default;

    virtual void initTree(int leaf_count) = 0;

    virtual bool applyRestriction(const std::vector<int> &restriction) = 0;

    virtual TreeType type() = 0;

    virtual void cleanUp() { time = 0; };

    virtual int terminalPathLength() { return 0; };

    virtual Bigint possibleOrders() { return Bigint(0); };

    virtual bool isValidOrder(const std::vector<int> &order) { return true; };

    virtual std::ostream &uniqueID(std::ostream &os) { return os; };

    virtual std::string toString() { return ""; };

    virtual json stats() { return json(); };

    long getTime() { return time; }
};
