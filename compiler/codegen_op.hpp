#pragma once

#include "ceramic.hpp"

namespace ceramic {
struct CodegenContext;
void codegenPrimOp(PrimOpPtr x, MultiCValuePtr args, CodegenContext *ctx,
                   MultiCValuePtr out);
} // namespace ceramic
