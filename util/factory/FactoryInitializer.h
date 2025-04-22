// FactoryInitializer.h
#include "GenericFactory.h"
#include "MathProduct.h"
#include "PhysicsProduct.h"

inline void InitializeAllFactories(GenericFactory& factory) {
    // 一键初始化所有工厂
    factory.registerFactory<MathProduct>(FactoryType::MATH_FACTORY);
    MathProductExtensions::registerAll(factory);
    factory.registerFactory<PhysicsProduct>(FactoryType::PHYSICS_FACTORY);
    PhysicsProductExtensions::registerAll(factory);
    // ... 其他工厂注册 ...
}