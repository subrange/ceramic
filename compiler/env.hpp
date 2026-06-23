#pragma once

#include "ceramic.hpp"

namespace ceramic {
void addGlobal(const ModulePtr &module, const IdentifierPtr &name,
               Visibility visibility, ObjectPtr value);

ObjectPtr lookupPrivate(const ModulePtr &module, const IdentifierPtr &name);
ObjectPtr lookupPublic(const ModulePtr &module, const IdentifierPtr &name);
ObjectPtr safeLookupPublic(const ModulePtr &module, const IdentifierPtr &name);

void addLocal(const EnvPtr &env, const IdentifierPtr &name, ObjectPtr value);
ObjectPtr lookupEnv(const EnvPtr &env, const IdentifierPtr &name);
ObjectPtr safeLookupEnv(const EnvPtr &env, const IdentifierPtr &name);
ModulePtr safeLookupModule(const EnvPtr &env);
llvm::DINamespace *lookupModuleDebugInfo(const EnvPtr &env);

ObjectPtr lookupEnvEx(const EnvPtr &env, const IdentifierPtr &name,
                      EnvPtr nonLocalEnv, bool &isNonLocal, bool &isGlobal);

ExprPtr foreignExpr(const EnvPtr &env, ExprPtr expr);

ExprPtr lookupCallByNameExprHead(const EnvPtr &env);
Location safeLookupCallByNameLocation(const EnvPtr &env, const char *macro);

bool lookupExceptionAvailable(const Env *env);
} // namespace ceramic
