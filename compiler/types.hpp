#pragma once

#include "clay.hpp"

namespace clay {
    //
    // types module
    //

    extern TypePtr boolType;
    extern TypePtr int8Type;
    extern TypePtr int16Type;
    extern TypePtr int32Type;
    extern TypePtr int64Type;
    extern TypePtr int128Type;
    extern TypePtr uint8Type;
    extern TypePtr uint16Type;
    extern TypePtr uint32Type;
    extern TypePtr uint64Type;
    extern TypePtr uint128Type;
    extern TypePtr float32Type;
    extern TypePtr float64Type;
    extern TypePtr float80Type;
    extern TypePtr imag32Type;
    extern TypePtr imag64Type;
    extern TypePtr imag80Type;
    extern TypePtr complex32Type;
    extern TypePtr complex64Type;
    extern TypePtr complex80Type;

    // aliases
    extern TypePtr cIntType;
    extern TypePtr cSizeTType;
    extern TypePtr cPtrDiffTType;

    void initTypes();

    TypePtr integerType(unsigned bits, bool isSigned);
    TypePtr intType(unsigned bits);
    TypePtr uintType(unsigned bits);
    TypePtr floatType(unsigned bits);
    TypePtr imagType(unsigned bits);
    TypePtr complexType(unsigned bits);

    TypePtr pointerType(const TypePtr &pointeeType);

    TypePtr codePointerType(llvm::ArrayRef<TypePtr> argTypes,
                            llvm::ArrayRef<uint8_t> returnIsRef,
                            llvm::ArrayRef<TypePtr> returnTypes);

    TypePtr cCodePointerType(CallingConv callingConv,
                             llvm::ArrayRef<TypePtr> argTypes,
                             bool hasVarArgs,
                             const TypePtr &returnType);

    TypePtr arrayType(const TypePtr &elementType, unsigned size);

    TypePtr vecType(const TypePtr &elementType, unsigned size);

    TypePtr tupleType(llvm::ArrayRef<TypePtr> elementTypes);
    TypePtr unionType(llvm::ArrayRef<TypePtr> memberTypes);

    TypePtr recordType(const RecordDeclPtr &record, llvm::ArrayRef<ObjectPtr> params);

    TypePtr variantType(const VariantDeclPtr &variant, llvm::ArrayRef<ObjectPtr> params);

    TypePtr staticType(const ObjectPtr &obj);

    TypePtr enumType(const EnumDeclPtr &enumeration);

    TypePtr newType(const NewTypeDeclPtr &decl);

    bool isPrimitiveType(TypePtr t);
    bool isPrimitiveAggregateType(TypePtr t);
    bool isPrimitiveAggregateTooLarge(TypePtr t);
    bool isPointerOrCodePointerType(TypePtr t);
    bool isStaticOrTupleOfStatics(TypePtr t);

    void initializeRecordFields(const RecordTypePtr &t);

    llvm::ArrayRef<IdentifierPtr> recordFieldNames(const RecordTypePtr &t);

    llvm::ArrayRef<TypePtr> recordFieldTypes(const RecordTypePtr &t);

    const llvm::StringMap<size_t> &recordFieldIndexMap(const RecordTypePtr &t);

    llvm::ArrayRef<TypePtr> variantMemberTypes(const VariantTypePtr &t);

    TypePtr variantReprType(const VariantTypePtr &t);

    unsigned dispatchTagCount(TypePtr t);

    TypePtr newtypeReprType(const NewTypePtr &t);

    void initializeEnumType(const EnumTypePtr &t);

    void initializeNewType(const NewTypePtr &t);

    const llvm::StructLayout *tupleTypeLayout(TupleType *t);
    const llvm::StructLayout *complexTypeLayout(ComplexType *t);
    const llvm::StructLayout *recordTypeLayout(RecordType *t);

    llvm::Type *llvmIntType(unsigned bits);
    llvm::Type *llvmFloatType(unsigned bits);
    llvm::PointerType *llvmPointerType(llvm::Type *llType);

    llvm::PointerType *llvmPointerType(const TypePtr &t);

    llvm::Type *llvmArrayType(llvm::Type *llType, unsigned size);

    llvm::Type *llvmArrayType(const TypePtr &type, unsigned size);

    llvm::Type *llvmVoidType();

    llvm::Type *llvmType(const TypePtr &t);

    llvm::DIType *llvmTypeDebugInfo(const TypePtr &t);

    llvm::DIType llvmVoidTypeDebugInfo();

    size_t typeSize(const TypePtr &t);

    size_t typeAlignment(const TypePtr &t);

    void typePrint(llvm::raw_ostream &out, TypePtr t);

    llvm::StringRef typeName(const TypePtr &t);

    inline size_t alignedUpTo(const size_t offset, const size_t align) {
        return (offset + align - 1) / align * align;
    }

    inline size_t alignedUpTo(const size_t offset, const TypePtr &type) {
        return alignedUpTo(offset, typeAlignment(type));
    }
}
