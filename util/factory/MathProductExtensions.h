#ifndef MATH_PRODUCT_EXTENSIONS_H
#define MATH_PRODUCT_EXTENSIONS_H

#include "GenericFactory.h"
#include "MathProduct.h"

namespace MathProductExtensions {

    inline void registerAddOperation(GenericFactory& factory) {
        factory.registerOperation<MathProduct>(FactoryType::MATH_FACTORY, "add", &MathProduct::add);
    }

    inline void registerMultiplyOperation(GenericFactory& factory) {
        factory.registerOperation<MathProduct,double>(FactoryType::MATH_FACTORY, "multiply", &MathProduct::multiply);
    }

    inline void registerGetValueOperation(GenericFactory& factory) {
        // 明确指定返回类型为 double
        factory.registerOperation<MathProduct,double>(FactoryType::MATH_FACTORY, "getValue", &MathProduct::getValue);
    }

    inline void registerAll(GenericFactory& factory) {
        registerAddOperation(factory);
        registerMultiplyOperation(factory);
        registerGetValueOperation(factory);
    }

} // namespace MathProductExtensions

#endif // MATH_PRODUCT_EXTENSIONS_H