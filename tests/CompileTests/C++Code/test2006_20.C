#if 0
This bug was found while compiling
/src/midend/astUtil/astInterface/AstInterface.C. I was able to make it
disappear by changing the code as specified below, but still it is a bug
since
__uninitialized_copy_aux is a valid template in STL and is found by GCC.

Compiling the following code using ROSE:
//Support header files for stl_uninitialized.h
#include <bits/stl_algobase.h>
#include <bits/stl_construct.h>
//The __uninitialized_copy_aux template is declared and defined within
stl_uninitialized.h
#include <bits/stl_uninitialized.h>

using namespace std;
class BoolAttribute
{};

//To make the bug disappear just replace __uninitialized_copy_aux with
std::__uninitialized_copy_aux
template BoolAttribute * __uninitialized_copy_aux<BoolAttribute const *,
BoolAttribute *>(BoolAttribute const *, BoolAttribute const *,
BoolAttribute *, __false_type);


gives the following error:

"../../../../../ROSE/src/midend/astUtil/astInterface/AstInterface.C", line
13: error:
          "__uninitialized_copy_aux" is not a class or function template name
          in the current scope
  template BoolAttribute * __uninitialized_copy_aux<BoolAttribute const *,
BoolAttribute *>(BoolAttribute const *, BoolAttribute const *,
BoolAttribute *, __false_type);


Andreas
#endif

// Support header files for stl_uninitialized.h
#include <bits/stl_algobase.h>
#include <bits/stl_construct.h>
// The __uninitialized_copy_aux template is declared and defined within stl_uninitialized.h
#include <bits/stl_uninitialized.h>

using namespace std;
class BoolAttribute
{};

// To make the bug disappear just replace __uninitialized_copy_aux with std::__uninitialized_copy_aux
template BoolAttribute * __uninitialized_copy_aux<BoolAttribute const *,BoolAttribute *>(BoolAttribute const *, BoolAttribute const *, BoolAttribute *, __false_type);

