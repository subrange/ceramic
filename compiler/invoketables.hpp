#pragma once

#include "clay.hpp"
#include "matchinvoke.hpp"

namespace clay {
    struct InvokeSet;
    struct InvokeEntry;

    static auto invokeEntryAllocator
            = new llvm::SpecificBumpPtrAllocator<InvokeEntry>();
    static auto invokeSetAllocator
            = new llvm::SpecificBumpPtrAllocator<InvokeSet>();

    struct InvokeEntry {
        virtual ~InvokeEntry() = default;

        InvokeSet *parent;
        ObjectPtr callable;
        vector<TypePtr> argsKey;
        vector<uint8_t> forwardedRValueFlags;

        CodePtr origCode;
        CodePtr code;
        EnvPtr env;
        EnvPtr interfaceEnv;

        vector<TypePtr> fixedArgTypes;
        vector<IdentifierPtr> fixedArgNames;
        IdentifierPtr varArgName;
        vector<TypePtr> varArgTypes;
        unsigned varArgPosition;

        InlineAttribute isInline;

        ObjectPtr analysis;
        vector<uint8_t> returnIsRef;
        vector<TypePtr> returnTypes;

        llvm::Function *llvmFunc;
        llvm::Function *llvmCWrappers[CC_Count]{};

        llvm::TrackingMDNodeRef debugInfo;

        bool analyzed: 1;
        bool analyzing: 1;
        bool callByName: 1; // if callByName the rest of InvokeEntry is not set
        bool runtimeNop: 1;

        InvokeEntry(InvokeSet *parent,
                    const ObjectPtr &callable,
                    llvm::ArrayRef<TypePtr> argsKey)
            : parent(parent), callable(callable),
              argsKey(argsKey),
              varArgPosition(0),
              isInline(IGNORE),
              llvmFunc(nullptr),
              debugInfo(nullptr),
              analyzed(false),
              analyzing(false),
              callByName(false),
              runtimeNop(false) {
            for (auto & llvmCWrapper : llvmCWrappers)
                llvmCWrapper = nullptr;
        }

        void *operator new(size_t num_bytes) {
            return invokeEntryAllocator->Allocate();
        }

        virtual void dealloc() { ANodeAllocator->Deallocate(this); }
        llvm::DISubprogram *getDebugInfo() const {
            return llvm::dyn_cast_or_null<llvm::DISubprogram>(debugInfo.get());
        }
    };

    extern vector<OverloadPtr> patternOverloads;

    struct InvokeSet {
        virtual ~InvokeSet() = default;

        ObjectPtr callable;
        vector<TypePtr> argsKey;
        OverloadPtr interface;
        vector<OverloadPtr> overloads;

        vector<MatchSuccessPtr> matches;
        map<vector<bool>, InvokeEntry *> tempnessMap;
        map<vector<ValueTempness>, InvokeEntry *> tempnessMap2;

        unsigned nextOverloadIndex; //:31;

        bool shouldLog: 1;
        bool evaluatingPredicate: 1;

        InvokeSet(const ObjectPtr &callable,
                  llvm::ArrayRef<TypePtr> argsKey,
                  const OverloadPtr &symbolInterface,
                  llvm::ArrayRef<OverloadPtr> symbolOverloads)
            : callable(callable), argsKey(argsKey),
              interface(symbolInterface),
              overloads(symbolOverloads), nextOverloadIndex(0),
              shouldLog(false),
              evaluatingPredicate(false) {
            overloads.insert(overloads.end(), patternOverloads.begin(), patternOverloads.end());
        }

        void *operator new(size_t num_bytes) {
            return invokeSetAllocator->Allocate();
        }

        virtual void dealloc() { ANodeAllocator->Deallocate(this); }
    };

    typedef vector<pair<OverloadPtr, MatchResultPtr> > MatchFailureVector;

    struct MatchFailureError {
        MatchFailureVector failures;
        bool failedInterface: 1;
        bool ambiguousMatch: 1;

        MatchFailureError() : failedInterface(false), ambiguousMatch(false) {
        }
    };

    InvokeSet *lookupInvokeSet(ObjectPtr callable,
                               llvm::ArrayRef<TypePtr> argsKey);

    vector<InvokeSet *> lookupInvokeSets(ObjectPtr callable);

    InvokeEntry *lookupInvokeEntry(ObjectPtr callable,
                                   llvm::ArrayRef<PVData> args,
                                   MatchFailureError &failures);

    void setFinalOverloadsEnabled(bool enabled);
}
