//===- LoopRewriter.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Example clang plugin which simply prints the names of all the top-level decls
// in the input file.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

namespace {

class ForStmtVisitor : public RecursiveASTVisitor<ForStmtVisitor> {
private:
  ASTContext &Context;

public:
  ForStmtVisitor(ASTContext &Context) : Context(Context) {}

  bool VisitForStmt(ForStmt *FS) {
    // add statement into CompoundStmt
    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(FS->getBody())) {
      llvm::errs() << "inside for loop\n";
      // create stash array
      Stmt **stashedBody = new Stmt *[CS->size() + 1];
      int stashIndex = 0;

      // iterate body and pack them into stash array
      for (CompoundStmt::body_iterator i = CS->body_begin(), e = CS->body_end();
           i != e; ++i) {
        Stmt *S = *i;
        stashedBody[stashIndex++] = S;
      }

      // and add BreakStmt
      stashedBody[stashIndex++] = new (Context) BreakStmt(CS->getEndLoc());

      // then, create new CompoundStmt()
      CompoundStmt *CSNew = CompoundStmt::Create(
          Context, ArrayRef<Stmt *>(stashedBody, stashIndex), CS->getLBracLoc(),
          CS->getRBracLoc());
      // and set for content
      FS->setBody(CSNew);

      // cleanup
      delete stashedBody;
    }
    return true;
  }
};

class RewriteLoopConsumer : public ASTConsumer {
  CompilerInstance &Instance;
  ForStmtVisitor *visitor;

public:
  RewriteLoopConsumer(CompilerInstance &Instance)
      : Instance(Instance),
        visitor(new ForStmtVisitor(Instance.getASTContext())) {}

  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i) {
      Decl *D = *i;
      if (NamedDecl *ND = dyn_cast<NamedDecl>(D)) {
        if (ND->getNameAsString() == std::string("main")) {
          llvm::errs() << "inside main()\n";
          visitor->TraverseDecl(ND);
        }
      }
    }

    return true;
  }

  void HandleTranslationUnit(ASTContext &context) override {
    // debug dump
    // context.getTranslationUnitDecl()->dump();
  }
};

class LoopRewriterAction : public PluginASTAction {

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<RewriteLoopConsumer>(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    // nop
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static FrontendPluginRegistry::Add<LoopRewriterAction> X("loop-rewriter",
                                                         "rewrite loop");
