#pragma once

#include "ceramic.hpp"

namespace ceramic {
bool isOverloadablePrimOp(ObjectPtr x);
vector<OverloadPtr> &primOpOverloads(PrimOpPtr x);
vector<OverloadPtr> &getPatternOverloads();
void initBuiltinConstructor(RecordDeclPtr x);
} // namespace ceramic
