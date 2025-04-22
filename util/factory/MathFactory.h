#ifndef MATH_FACTORY_H
#define MATH_FACTORY_H

#include "GenericFactory.h"
#include "MathProduct.h"

class MathFactory : public IFactory {
public:
    std::unique_ptr<IProduct> create() override {
        return std::make_unique<MathProduct>();
    }

    std::unique_ptr<IProduct> create(const std::vector<std::any>& args) override {
        if (args.empty()) {
            return std::make_unique<MathProduct>();
        }

        if (args.size() == 1 && args[0].type() == typeid(double)) {
            try {
                double initialValue = std::any_cast<double>(args[0]);
                return std::make_unique<MathProduct>(initialValue);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid argument type for MathProduct creation");
            }
        }

        throw std::runtime_error("Unsupported arguments for MathProduct creation");
    }

    std::string getFactoryType() const override {
        return "MathFactory";
    }
};

#endif // MATH_FACTORY_H