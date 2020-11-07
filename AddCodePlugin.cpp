#include "clang/Driver/Options.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Sema/Sema.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/DeclObjC.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace std;
using namespace clang::ast_matchers;

static llvm::cl::OptionCategory AddOptionCategory("ClangAutoStats");

//要插入的代码块 尽量保持一行 避免编译的loc错乱
static std::string CodeSnippet = "NSLog(@\"%__FUNCNAME__%\");";

//新命名
namespace AddCodePlugin {

class ClangAutoStatsVisitor : public RecursiveASTVisitor<ClangAutoStatsVisitor> {
    
private:
    Rewriter &rewriter;
public:
    explicit ClangAutoStatsVisitor(Rewriter &R) : rewriter{R} {} // 创建方法
    
    //如果是实例方法
    bool VisitObjCImplementationDecl(ObjCImplementationDecl *ID) {
        for (auto D : ID->decls()) {
            if (ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(D)) {
                handleObjcMethDecl(MD);
            }
        }
        return true;
    }
    
    //方法处理逻辑
    bool handleObjcMethDecl(ObjCMethodDecl *MD) {
        if (!MD->hasBody()) return true;

        cout << "handleObjcMethDecl" << endl;
        //找到方法的括号后面 同时避免宏定义没展开
        CompoundStmt *CS = MD->getCompoundBody();
        SourceLocation loc = CS->getBeginLoc().getLocWithOffset(1);
        if (loc.isMacroID()) {
            loc = rewriter.getSourceMgr().getImmediateExpansionRange(loc).getBegin();
        }
        
        static std::string varName("%__FUNCNAME__%");
        std::string funcName = MD->getDeclName().getAsString();
        std::string codes(CodeSnippet);
        size_t pos = 0;
         //替换特殊字符%__FUNCNAME__%为方法名
        while ((pos = codes.find(varName, pos)) != std::string::npos) {
            codes.replace(pos, varName.length(), funcName);
            pos += funcName.length();
        }
        //写入
        rewriter.InsertTextBefore(loc, codes);
        
        return true;
    }
};

class ClangAutoStatsASTConsumer : public ASTConsumer {
private:
   ClangAutoStatsVisitor Visitor;
public:
   explicit ClangAutoStatsASTConsumer(Rewriter &R): Visitor(R) {}//创建方法
    
   void HandleTranslationUnit(ASTContext &context) {
       TranslationUnitDecl *decl = context.getTranslationUnitDecl();
       Visitor.TraverseTranslationUnitDecl(decl);
   }

};

class ClangAutoStatsAction : public ASTFrontendAction {
private:
    Rewriter fileRewriter;

public:
    unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) {
       fileRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
           CI.getPreprocessor();
           return make_unique<ClangAutoStatsASTConsumer>(fileRewriter);
    }

    bool ParseArgs(const CompilerInstance &ci, const std::vector<std::string> &args) {
        return true;
    }
     
    void EndSourceFileAction() {
        cout << "EndSourceFileAction" << endl;
        
        SourceManager &SM = fileRewriter.getSourceMgr();
        std::string filename = SM.getFileEntryForID(SM.getMainFileID())->getName().str();
        std::error_code error_code;
        llvm::raw_fd_ostream outFile(filename, error_code, llvm::sys::fs::F_None);
        // 将Rewriter结果输出到文件中
        fileRewriter.getEditBuffer(SM.getMainFileID()).write(outFile);
        // 将Rewriter结果输出在控制台上
        fileRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
    }
    
};

};

//执行入口
int main(int argc, const char **argv) {
    CommonOptionsParser op(argc, argv, AddOptionCategory);
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());
    int result = Tool.run(newFrontendActionFactory<AddCodePlugin::ClangAutoStatsAction>().get());
    return result;
}
