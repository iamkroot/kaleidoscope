#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/IndirectionUtils.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/Orc/TPCIndirectionUtils.h>
#include <llvm/ExecutionEngine/Orc/TargetProcessControl.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace llvm;
using namespace llvm::orc;

class KaleidoscopeJIT {
public:
  std::unique_ptr<TargetProcessControl> tpc;
  std::unique_ptr<ExecutionSession> es;
  std::unique_ptr<TPCIndirectionUtils> tpciu;
  std::unique_ptr<IndirectStubsManager> stubsMgr;

  DataLayout dl;
  MangleAndInterner mangle;
  RTDyldObjectLinkingLayer objectLayer;
  IRCompileLayer compileLayer;
  JITDylib &mainJD;
  std::string curFunc{"foo"};

  KaleidoscopeJIT(std::unique_ptr<TargetProcessControl> tpc,
                  std::unique_ptr<ExecutionSession> es,
                  std::unique_ptr<TPCIndirectionUtils> tpciu,
                  JITTargetMachineBuilder jtmb, DataLayout dl)
      : tpc(std::move(tpc)), es(std::move(es)), tpciu(std::move(tpciu)),
        stubsMgr(this->tpciu->createIndirectStubsManager()), dl(std::move(dl)),
        mangle(*this->es, this->dl),
        objectLayer(*this->es,
                    []() { return std::make_unique<SectionMemoryManager>(); }),
        compileLayer(*this->es, objectLayer,
                     std::make_unique<ConcurrentIRCompiler>(std::move(jtmb))),
        mainJD(this->es->createBareJITDylib("<main>")) {
    mainJD.addGenerator(
        cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
            dl.getGlobalPrefix())));
  }

  ~KaleidoscopeJIT() {
    if (auto err = es->endSession())
      es->reportError(std::move(err));
    if (auto err = tpciu->cleanup())
      es->reportError(std::move(err));
  }

  static Expected<std::unique_ptr<KaleidoscopeJIT>> Create() {
    auto ssp = std::make_shared<SymbolStringPool>();
    auto tpc = SelfTargetProcessControl::Create(ssp);
    if (!tpc)
      return tpc.takeError();
    auto es = std::make_unique<ExecutionSession>(std::move(ssp));
    auto tpciu = cantFail(TPCIndirectionUtils::Create(**tpc));
    JITTargetMachineBuilder jtmb((*tpc)->getTargetTriple());
    auto dl = cantFail(jtmb.getDefaultDataLayoutForTarget());
    return std::make_unique<KaleidoscopeJIT>(std::move(*tpc), std::move(es),
                                             std::move(tpciu), std::move(jtmb),
                                             std::move(dl));
  }

  Error addModule(ThreadSafeModule tsm, ResourceTrackerSP rt = nullptr) {
    if (!rt)
      rt = mainJD.getDefaultResourceTracker();
    return compileLayer.add(rt, std::move(tsm));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef name) {
    if (name == "foobar") {
      // really ugly hardcoding
      if (auto stub = stubsMgr->findStub("foobar", false)) {
        return stub;
      }
      // set foobar to call foo (via curFunc)
      auto sym = es->lookup({&mainJD}, mangle(curFunc));
      if (!sym)
        return sym.takeError();
      cantFail(stubsMgr->createStub("foobar", sym->getAddress(),
                                    JITSymbolFlags::Callable |
                                        JITSymbolFlags::Exported));
      return stubsMgr->findStub("foobar", false);
    } else {
      // normal lookup
      return es->lookup({&mainJD}, mangle(name.str()));
    }
  }
  void setCurFunc(const std::string &val) { curFunc = val; }
  const DataLayout &getDataLayout() const { return dl; }

  JITDylib &getMainJITDylib() const { return mainJD; }
};

// TODO: why doesn't this work?
extern "C" double foo() {
//  std::cout << "foo\n";
  return 2.;
}
// unused
extern "C" double bar() {
//  std::cout << "bar\n";
  return 3.;
}

std::unique_ptr<LLVMContext> ctx;
std::unique_ptr<Module> mod;
std::unique_ptr<IRBuilder<>> builder;

static void InitializeModule(const DataLayout &dl) {
  // Open a new context and module.
  ctx = std::make_unique<LLVMContext>();
  mod = std::make_unique<Module>("my cool jit", *ctx);
  mod->setDataLayout(dl);

  // Create a new builder for the module.
  builder = std::make_unique<IRBuilder<>>(*ctx);
}

void createFuncs() {
  FunctionType *ft = FunctionType::get(Type::getDoubleTy(*ctx), {}, false);
  Function *f_foo =
      Function::Create(ft, Function::ExternalLinkage, "foo", *mod);
  BasicBlock *bb = BasicBlock::Create(*ctx, "entry", f_foo);
  builder->SetInsertPoint(bb);
  Value *v = ConstantFP::get(Type::getDoubleTy(*ctx), 2.0);
  builder->CreateRet(v);
  verifyFunction(*f_foo, &errs());

  Function *f_bar =
      Function::Create(ft, Function::ExternalLinkage, "bar", *mod);
  bb = BasicBlock::Create(*ctx, "entry", f_bar);
  builder->SetInsertPoint(bb);
  v = ConstantFP::get(Type::getDoubleTy(*ctx), 3.0);
  builder->CreateRet(v);
  verifyFunction(*f_bar, &errs());
}

typedef double (*func)();

void mainloop(func fb) {
  for (int i = 0; i < 10; ++i) {
    std::cout << (*fb)() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

int main() {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  std::unique_ptr<KaleidoscopeJIT> jit = cantFail(KaleidoscopeJIT::Create());
  InitializeModule(jit->dl);

  createFuncs(); // creates 2 funcs, foo and bar, which return different
                 // constant values
  ThreadSafeContext tsctx(std::make_unique<LLVMContext>());
  auto tsm = ThreadSafeModule(std::move(mod), std::move(tsctx));
  cantFail(jit->addModule(std::move(tsm)));

  auto fb = cantFail(jit->lookup("foobar"));
  auto *f_ptr = (double (*)())fb.getAddress();
  std::thread t(mainloop, f_ptr);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  auto b = cantFail(jit->lookup("bar"));
  cantFail(jit->stubsMgr->updatePointer("foobar", b.getAddress()));

  t.join();
}
