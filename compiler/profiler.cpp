#include "clay.hpp"
#include "profiler.hpp"
#include "printer.hpp"

namespace clay {
    static llvm::StringMap<int> countsMap;

    void incrementCount(const ObjectPtr &obj) {
        string buf;
        llvm::raw_string_ostream sout(buf);
        sout << obj;
        string s = sout.str();
        llvm::StringMap<int>::iterator i = countsMap.find(s);
        if (i == countsMap.end()) {
            countsMap[s] = 1;
        } else {
            i->second += 1;
        }
    }

    void displayCounts() {
        vector<pair<int, string> > counts;
        llvm::StringMap<int>::iterator cmi = countsMap.begin();
        while (cmi != countsMap.end()) {
            counts.emplace_back(cmi->getValue(), cmi->getKey());
            ++cmi;
        }
        sort(counts.begin(), counts.end());
        for (auto & count : counts) {
            llvm::outs() << count.second << " - " << count.first << '\n';
        }
    }
}
