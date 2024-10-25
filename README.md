# PC-Trees

This is a C++ implementation of the PC-tree datastructure for representing cyclic permutations subject to some *consecutivity* constraints, that is, requiring certain sets of elements to appear uninterrupted by elements from their complement sets.
The PC-Tree was first introduced by Hsu, Shih, and McConnell \[1, 2, 3\] and this union-find based implementation was found to be the by-far fastest correct one \[4\].
While the [same code](https://github.com/ogdf/ogdf/tree/master/src/ogdf/basic/pctree) is also [available](https://ogdf.netlify.app/classogdf_1_1pc__tree_1_1_p_c_tree.html) through the [OGDF library](https://github.com/ogdf/ogdf), 
this stand-alone version has no dependencies and only uses [bandit](https://github.com/banditcpp/bandit) and [Dodecahedron::Bigint](https://github.com/kasparsklavins/bigint) for testing.

If you are looking for the older evaluation code from \[4\], you can find it in the [UFPCPaper](https://github.com/N-Coder/pc-tree/tree/UFPCPaper) branch.
Similarly, the older non-union-find-based HsuPC implementation can be found in the [HsuPCSubmodule](https://github.com/N-Coder/pc-tree/tree/HsuPCSubmodule) branch.
If you want more backgrounds on the PC-tree datastructure, see this [Wikipedia article](https://en.wikipedia.org/wiki/PQ_tree) and the thesis \[5\].

## References

1. >Wei-Kuan Shih, Wen-Lian Hsu:
    A New Planarity Test.
    Theor. Comput. Sci. 223(1-2): 179-191 (1999)
    https://doi.org/10.1016/S0304-3975(98)00120-0

2. >Wen-Lian Hsu:
    PC-Trees vs. PQ-Trees.
    COCOON 2001: 207-217
    https://doi.org/10.1007/3-540-44679-6_23

3. >Wen-Lian Hsu, Ross M. McConnell:
    PC trees and circular-ones arrangements.
    Theor. Comput. Sci. 296(1): 99-116 (2003)
    https://doi.org/10.1016/S0304-3975(02)00435-8

4. >Simon D. Fink, Matthias Pfretzschner, Ignaz Rutter:
    Experimental Comparison of PC-Trees and PQ-Trees.
    ACM J. Exp. Algorithmics 28: 1.10:1-1.10:24 (2023)
    https://doi.org/10.1145/3611653

5. >Simon Dominik Fink:
    Constrained Planarity Algorithms in Theory and Practice.
    University of Passau, Germany, 2024
    https://doi.org/10.15475/cpatp.2024
