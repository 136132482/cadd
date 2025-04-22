//
// Created by Administrator on 2025/1/22 0022.
//
#include <iostream>
#include <string>
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

class ReflectionGenerator : public MatchFinder::MatchCallback {
public:
    virtual void run(const MatchFinder::MatchResult &Result) {
        if (const CXXRecordDecl *Record = Result.Nodes.getNodeAs<CXXRecordDecl>("class")) {
            std::string className = Record->getNameAsString();
            std::cout << "Registering class: " << className << std::endl;
            // 在这里可以生成反射信息并注册类

            // 获取类名，并输出到控制台
            std::cout << "Class name: " << className << std::endl;

            // 遍历类的字段，并输出到控制台
            for (const FieldDecl *Field : Record->fields()) {
                std::string fieldName = Field->getNameAsString();
                std::cout << "Field name: " << fieldName << std::endl;
            }
        }
    }
};

int main(int argc, const char **argv) {
     CommonOptionsParser OptionsParser(argc, argv);
     ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

    ReflectionGenerator Generator;
    MatchFinder Finder;
    Finder.addMatcher(cxxRecordDecl().bind("class"), &Generator);

    return Tool.run(newFrontendActionFactory(&Finder).get());
}