// PhysicsProduct.h
#include "GenericFactory.h"

class PhysicsProduct : public IProduct {
public:
    std::string getName() const override { return "PhysicsProduct"; }

    // 物理相关操作
    double calculateForce(double mass, double acceleration) {
        return mass * acceleration;
    }

    static std::string getPhysicsFormula() {
        return "F = m*a";
    }
};

// PhysicsProductExtensions.h
namespace PhysicsProductExtensions {
    inline void registerAll(GenericFactory& factory) {
        // 注册物理工厂的操作
        factory.registerOperation<PhysicsProduct, double, double, double>(
                FactoryType::PHYSICS_FACTORY,
                "calculateForce",
                &PhysicsProduct::calculateForce);

        factory.registerOperation<std::string>(
                FactoryType::PHYSICS_FACTORY,
                "getFormula",
                &PhysicsProduct::getPhysicsFormula);
    }
}