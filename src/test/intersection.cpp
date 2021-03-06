#include <bandit/bandit.h>
#include "PCTree.h"

using namespace pc_tree;
using namespace snowhouse;
using namespace bandit;

bool applyRestrictions(PCTree &t, std::initializer_list<std::initializer_list<int>> restrictions) {
    for (auto restriction : restrictions) {
        std::vector<PCNode *> restrictionLeaves;
        for (int i : restriction) {
            restrictionLeaves.push_back(t.getLeaves().at(i));
        }
        if (!t.makeConsecutive(restrictionLeaves)) {
            return false;
        }
    }
    return true;
}

void testIntersection(int numLeaves, std::initializer_list<std::initializer_list<int>> restrictions1,
                      std::initializer_list<std::initializer_list<int>> restrictions2) {
    PCTree t1(numLeaves);
    PCTree t2(numLeaves);
    PCTreeNodeArray<PCNode *> mapLeaves(t2);
    for (int i = 0; i < numLeaves; i++) {
        mapLeaves[t2.getLeaves().at(i)] = t1.getLeaves().at(i);
    }
    AssertThat(applyRestrictions(t1, restrictions1), IsTrue());
    AssertThat(applyRestrictions(t2, restrictions2), IsTrue());

    PCTree check(numLeaves);
    AssertThat(applyRestrictions(check, restrictions1), IsTrue());
    bool possibleCheck = applyRestrictions(check, restrictions2);

    bool possibleIntersection = t1.intersect(t2, mapLeaves);
    AssertThat(possibleIntersection, Equals(possibleCheck));
    if (possibleCheck) {
        AssertThat(t1.uniqueID(uid_utils::leafToID), Equals(check.uniqueID(uid_utils::leafToID)));
    }
}

go_bandit([]() {
    describe("PCTree intersection", []() {
        it("correctly handles the trivial case", []() {
            testIntersection(10,
                             {{0, 1, 2}},
                             {});
        });
        it("correctly handles another trivial case", []() {
            testIntersection(10,
                             {},
                             {{0, 1, 2}});
        });
        it("correctly handles a tree with only P-Nodes", []() {
            testIntersection(10,
                             {{3, 4, 5}},
                             {{0, 1, 2},
                              {6, 7, 8}});
        });
        it("correctly handles a simple intersection with a C-Node", []() {
            testIntersection(10,
                             {{2, 3, 4}},
                             {{0, 1, 2},
                              {5, 6, 7},
                              {7, 8, 9}});
        });
        it("correctly handles a single C-Node", []() {
            testIntersection(5,
                             {{1, 2, 3}},
                             {{0, 1},
                              {1, 2},
                              {2, 3},
                              {3, 4},
                              {4, 0}});
        });
        it("correctly handles a more complicated intersection", []() {
            testIntersection(20,
                             {{11, 12, 13, 14},
                              {0,  8},
                              {14, 9}},
                             {{0,  1},
                              {1,  2},
                              {2,  3},
                              {6,  7, 8, 9, 10},
                              {11, 12},
                              {12, 13},
                              {13, 14},
                              {15, 16},
                              {16, 17},
                              {17, 18}});
        });
        it("correctly handles an impossible intersection", []() {
            testIntersection(10,
                             {{0, 1},
                              {1, 2},
                              {2, 3}},
                             {{0, 2}});
        });
    });

});

int main(int argc, char *argv[]) {
    return bandit::run(argc, argv);
}