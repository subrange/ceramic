#include <iostream>
#include <vector>

#include "ut.hpp"
#include "parachute.hpp"

namespace clay {
    struct Test {
        const char *name;
        TestFunc func;
    };

    static std::vector<Test> *tests;

    void register_test(const char *name, const TestFunc func) {
        if (tests == nullptr) {
            tests = new std::vector<Test>;
        }
        tests->emplace_back();
        tests->back().name = name;
        tests->back().func = func;
    }

    int real_main(int argc, char **argv, char const *const *envp) {
        for (const auto test : *tests) {
            std::cout << test.name << "...\n";
            try {
                test.func();

                std::cout << test.name << " OK\n";
            } catch (const AssertionError &) {
                std::cout << test.name << " FAILED\n";
            }
        }
        return 0;
    }
}

int main(const int argc, char **argv, char const *const *envp) {
    return clay::parachute(&clay::real_main, argc, argv, envp);
}
