#!/bin/bash

set -e
set -x

mkdir -p ../libraries/zanettiPQR/
cd ../libraries/zanettiPQR/

rm -f *.h
cp ../../evaluation/javaEvaluation/src/main/java/java_evaluation/jppzanetti/*.java .

for f in *.java; do
    mv -- "$f" "${f%.java}.h"
done

sed -i -- 's#;##g' Color.h PQRType.h
sed -i -- 's#implements .* {#{#g' PQRNode.h

sed -i -- 's#\t#    #g' *.h
sed -i -- 's#\bpackage\b#// package#g' *.h
sed -i -- 's#\bimport\b#// import#g' *.h
sed -i -- 's#\bassert\b#// assert#g' *.h
sed -i -- 's#@Override\b#// @Override#g' *.h
sed -i -- 's#\bclass\b#struct#g' *.h
sed -i -- 's#\benum\b#enum class#g' *.h
sed -i -- 's#\bextends\b#:#g' *.h
sed -i -- 's#\bpublic\b ##g' *.h
sed -i -- 's#\bprivate\b ##g' *.h
sed -i -- 's#\bprotected\b ##g' *.h
sed -i -- 's#\bfinal\b#const#g' *.h
sed -i -- 's#\babstract\b##g' *.h
sed -i -- 's#\bnull\b#nullptr#g' *.h
sed -i -- 's#\bboolean\b#bool#g' *.h
sed -i -- 's#\bColor\.#Color::#g' *.h
sed -i -- 's#\bPQRType\.#PQRType::#g' *.h
sed -i -- 's#\bStringBuilder\b#std::stringstream#g' *.h
sed -i -- 's#\bString\b#std::string\&#g' *.h
sed -i -- 's#\bLinkedList\b#std::deque#g' *.h
sed -i -- 's#\bArrayList\b#std::vector#g' *.h
sed -i -- 's#\bBigInteger\b#Bigint#g' *.h
sed -i -- 's#\bNode \([a-zA-Z0-9.]\+\)\b#Node * \1#g' *.h
sed -i -- 's#\bPQRNode \([a-zA-Z0-9.]\+\)\b#PQRNode * \1#g' *.h
sed -i -- 's#\bLeaf \([a-zA-Z0-9.]\+\)\b#Leaf * \1#g' *.h
sed -i -- 's#.length\b#.size()#g' *.h
sed -i -- 's#.add(#.push_back(#g' *.h
sed -i -- 's#.removeLast(#.pop_back(#g' *.h
sed -i -- 's#.isEmpty(#.empty(#g' *.h
sed -i -- 's#.ordinal()##g' *.h
sed -i -- 's#s +=#s <<#g' *.h
sed -i -- 's#\.append(# << (#g' *.h
sed -i -- 's#new IllegalStateException#std::runtime_error#g' *.h
sed -i -- 's#\([a-zA-Z.]\+\).poll()#poll(\1)#g' *.h
sed -i -- 's#case \([PQR]\)#case PQRType::\1#g' *.h
sed -i -- 's# \+\*#         *#g' *.h

sed -i -- 's#PQRNode#Node#g' Node.h
sed -i -- 's#Node *\* sibling\[\];#std::array<Node *, 2> sibling;#g' Node.h
sed -i -- 's#int\[\] c#std::vector<Leaf *> \&c#g' PQRTree.h

sed -i "0,/^/s//namespace cpp_zanetti{\n/" *.h
sed -i "$ a;}" *.h

sed -i '0,/^/s//#include "Color.h"\n\n/' Node.h
sed -i '0,/^/s//#include "Node.h"\n\n/' Leaf.h PQRNode.h
sed -i '0,/^/s//#include "PQRType.h"\n\n/' PQRNode.h
sed -i '0,/^/s//#include "PQRNode.h"\n\n/' PQRTree.h
sed -i '0,/^/s//#include "Leaf.h"\n\n/' PQRTree.h

sed -i "0,/^/s//#pragma once\n\n/" *.h

clang-format -style=file -i *.h

clang -x c++ -Xclang -fixit -Xclang -fix-what-you-can -ferror-limit=0 *.h &>cppzanetti-clang-warnings.log || true

clang-format -style=file -i *.h
