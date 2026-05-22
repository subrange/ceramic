#pragma once

#include "ceramic.hpp"

namespace ceramic {
void codegenPrimOp(PrimOpPtr x, MultiCValuePtr args, CodegenContext *ctx,
                   MultiCValuePtr out);
}
