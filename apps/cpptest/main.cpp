#include <cstdio>
#include <typeinfo>

#include <list>

using std::printf;

namespace {
struct testconstruction {
    testconstruction() {
        printf("Testing construction of %p\n", this);
    }

    void throwit(int x) {
        throw x;
    }

    ~testconstruction() {
        printf("Testing destruction of %p\n", this);
    }
};

testconstruction test;
}

int main()
{
    /* exceptions dont work right now, I dont know why */
#if 0
    try {
        printf("throwing\n");
        throw 1;
    } catch (int z) {
        printf("threw %d (int)\n", z);
    } catch (...) {
        printf("general purpose catch\n");
    }
#endif

    int *foo = new int;

    printf("allocated memory with new, address %p\n", foo);

    delete foo;
}
