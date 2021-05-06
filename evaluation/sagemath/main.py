import argparse
import json
import sys
import time
from copy import deepcopy
from json import JSONDecodeError
from math import factorial

import pq_trees


def fingerprint(tree, root=True):
    # orders = tree.cardinality()
    # if isinstance(tree, pq_trees.P):
    #     orders //= tree.number_of_children()
    # return orders

    orders = 1
    if isinstance(tree, frozenset):
        return 1
    if tree.number_of_children() > 1:
        if isinstance(tree, pq_trees.P):
            childCount = tree.number_of_children()
            if root:
                childCount -= 1
            orders *= factorial(childCount)
        else:
            assert isinstance(tree, pq_trees.Q)
            orders *= 2
    for child in tree:
        orders *= fingerprint(child, False)

    return orders


def processMatrix(matrix):
    mapping = [[i] for i in range(matrix["cols"])]
    index = 0
    for restriction in matrix["restrictions"]:
        for col in restriction:
            mapping[col].append(-index - 1)
        index += 1
    del matrix["restrictions"]

    mapping = [frozenset(s) for s in mapping]
    start = time.perf_counter()
    tree = pq_trees.P(mapping)
    matrix["tree_type"] = "SageMath2"
    matrix["name"] = algname
    matrix["id"] = "{}/{}".format(algname, matrix["id"])
    matrix["total_restrict_time"] = 0
    matrix["init_time"] = matrix["total_time"] = (time.perf_counter() - start) * 1000_000_000
    errors = []
    # matrix["sage_tree"] = str(tree)
    last_idx = 0
    for idx in range(0, matrix["rows"]):
        start = time.perf_counter()
        last = idx == matrix["rows"] - 1
        last_idx = idx
        try:
            tree.set_contiguous(-idx - 1)
            # restriction["sage_tree"] = str(tree)
            possible = True
        except ValueError as e:
            if e.args[0] != pq_trees.impossible_msg:
                raise
            else:
                possible = False

        rtime = (time.perf_counter() - start) * 1000_000_000
        if not last and not possible:
            matrix["valid"] = False
            errors.append("possible")
            break

        if last:
            last_res = matrix["last_restriction"]
            last_res["time"] = rtime
            last_res["possible"] = possible
            fp = str(fingerprint(tree))
            last_res["fingerprint"] = fp[:5] + str(len(fp))

            if possible and last_res["fingerprint"] != last_res["exp_fingerprint"]:
                errors.append("fingerprint")
            if possible is not last_res["exp_possible"]:
                errors.append("possible")

        matrix["total_time"] += rtime
        matrix["total_restrict_time"] += rtime

    matrix["valid"] = not errors
    matrix["errors"] = errors
    matrix["complete"] = last_idx == matrix["rows"] - 1


parser = argparse.ArgumentParser()
parser.add_argument("-n", type=str, default="SageMath2")
parser.add_argument("-r", type=int, default=1)
parser.add_argument("f", type=str)
args = parser.parse_args()
repetitions = args.r
filename = args.f
algname = args.n

for i in range(repetitions):
    with open(filename, "r") as f:
            matrix = json.load(f)
            processMatrix(matrix)
            if i is repetitions - 1:
                print(json.dumps(matrix))
