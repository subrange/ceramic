#include <vector>

#include "ut.hpp"
#include "parachute.hpp"

using namespace std;

namespace clay {
    struct Test {
        const char *name;
        TestFunc func;
    };

    static vector<Test> *tests;

    void register_test(const char *name, const TestFunc func) {
        if (tests == nullptr) {
            tests = new vector<Test>;
        }
        tests->push_back(Test());
        tests->back().name = name;
        tests->back().func = func;
    }

    int real_main(int argc, char **argv, char const *const *envp) {
        for (const auto test : *tests) {
            printf("%s...\n", test.name);
            try {
                test.func();

                printf("%s OK\n", test.name);
            } catch (const AssertionError &) {
                printf("%s FAILED\n", test.name);
            }
        }
        return 0;
    }
}

int main(const int argc, char **argv, char const *const *envp) {
    return clay::parachute(&clay::real_main, argc, argv, envp);
}
