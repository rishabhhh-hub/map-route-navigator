// tests.cpp - simple tests. Run it: it prints PASS or FAIL for each check.

#include <cmath>
#include <iostream>

#include "pathfinding.h"

int failures = 0;

void check(bool condition, const std::string& what) {
    std::cout << (condition ? "PASS  " : "FAIL  ") << what << "\n";
    if (!condition) failures++;
}

int main() {
    // A tiny graph where we know the answer:
    //   A --4-- B --1-- D        shortest A to D is A, C, B, D = 1 + 2 + 1 = 4
    //   |       |
    //   1       2
    //   |       |
    //   C ------+      (C to D directly costs 5)
    Graph small;
    small.addNode("A", 0, 0);
    small.addNode("B", 2, 0);
    small.addNode("C", 0, 1);
    small.addNode("D", 3, 0);
    small.addNode("Lonely", 50, 50);  // no roads: unreachable
    small.addRoad(0, 1, 4);
    small.addRoad(0, 2, 1);
    small.addRoad(2, 1, 2);
    small.addRoad(1, 3, 1);
    small.addRoad(2, 3, 5);

    for (bool aStar : {false, true}) {
        std::string name = aStar ? "A*" : "Dijkstra";

        Result r = findRoute(small, 0, 3, aStar);
        check(r.reachable && r.distance == 4, name + ": shortest distance A to D is 4");
        check((r.path == std::vector<int>{0, 2, 1, 3}), name + ": path is A, C, B, D");

        Result same = findRoute(small, 2, 2, aStar);
        check(same.reachable && same.distance == 0 && same.path.size() == 1,
              name + ": start equal to destination gives distance 0");

        Result none = findRoute(small, 0, 4, aStar);
        check(!none.reachable && none.path.empty(), name + ": unreachable node is reported");
    }

    // Dijkstra and A* must always agree on the distance (checked for every pair).
    Graph city = makeSampleCity();
    bool allAgree = true;
    for (int s = 0; s < (int)city.nodes.size(); s++) {
        for (int t = 0; t < (int)city.nodes.size(); t++) {
            Result d = findRoute(city, s, t, false);
            Result a = findRoute(city, s, t, true);
            if (d.reachable != a.reachable || std::fabs(d.distance - a.distance) > 1e-9)
                allAgree = false;
        }
    }
    check(allAgree, "Dijkstra and A* agree on every pair in the sample city");

    check(!findRoute(city, 0, 13, true).reachable, "island node 13 is unreachable from node 0");
    check(city.validNode(14) && !city.validNode(15) && !city.validNode(-1), "invalid node ids are detected");

    std::cout << (failures == 0 ? "\nAll tests passed.\n" : "\nSome tests FAILED.\n");
    return failures == 0 ? 0 : 1;
}
