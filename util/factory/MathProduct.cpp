//#include "MathProduct.h"
//#include <stdexcept>
//
//MathProduct::MathProduct(double value) : value_(value) {}
//
//void MathProduct::process(const std::string& op, double a, double b) {
//    if (op == "add") {
//        value_ = a + b;
//    } else if (op == "multiply") {
//        value_ = a * b;
//    } else {
//        throw std::invalid_argument("Invalid operation: " + op);
//    }
//}
//
//double MathProduct::getValue() const { return value_; }
//
//std::unique_ptr<IProduct> MathFactory::create() {
//    return std::make_unique<MathProduct>(0.0);
//}
//
//void MathFactory::destroy(IProduct* product) { delete product; }