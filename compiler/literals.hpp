#pragma once

#include "ceramic.hpp"

namespace ceramic {
ValueHolderPtr parseIntLiteral(ModulePtr module, IntLiteral *x);
ValueHolderPtr parseFloatLiteral(ModulePtr module, FloatLiteral *x);
} // namespace ceramic
