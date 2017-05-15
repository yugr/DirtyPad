// Copyright 2017 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/InstVisitor.h"

#include <stdlib.h>
#include <stdint.h>

#include <tuple>

using namespace llvm;

// LLVM's -debug machinery requires assertions
// which are of course not available in distro Clang...
#define MYDEBUG(x) //(x)

namespace {
  class InstrumentAllocas : public FunctionPass {
    static char ID;
    bool instrumentAlloca(AllocaInst &I, const DataLayout &DL);
  public:
    InstrumentAllocas(): FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    const char *getPassName() const override {
      return "DirtyPad (instrument alloca)";
    }
  };

  char InstrumentAllocas::ID;

  class InstrumentMallocs : public FunctionPass {
    static char ID;
    bool instrumentBitCast(BitCastInst &I, const DataLayout &DL,
                           TargetLibraryInfo &TLI);
  public:
    InstrumentMallocs(): FunctionPass(ID) {}
    bool runOnFunction(Function &F) override;
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<TargetLibraryInfoWrapperPass>();
    }
    const char *getPassName() const override {
      return "DirtyPad (instrument malloc)";
    }
  };

  char InstrumentMallocs::ID;

  class InstrumentGlobals : public ModulePass {
    static char ID;
  public:
    InstrumentGlobals(): ModulePass(ID) {}
    bool runOnModule(Module &M) override;
    const char *getPassName() const override {
      return "DirtyPad (instrument globals)";
    }
  };

  char InstrumentGlobals::ID;

  // TODO: also need a pass that will overwrite pads
  // whenever struct might have been modified. This
  // requires dataflow analysis.
}

static unsigned char Magic = 0xcd;

typedef SmallVectorImpl<std::pair<unsigned, unsigned> > PadVectorImpl;
typedef SmallVector<std::pair<unsigned, unsigned>, 10> PadVector;

// TODO: cache results
static void GetPads_1(StructType *Ty, PadVectorImpl &Pads,
                      unsigned Base, unsigned StructSize,
                      const DataLayout &DL) {
  const StructLayout *SL = DL.getStructLayout(Ty);

  for (unsigned I = 0, N = Ty->getNumElements(); I < N; ++I) {
    Type *ETy = Ty->getElementType(I);

    unsigned NextOffset = I < N - 1
                          ? Base + SL->getElementOffset(I + 1)
                          : StructSize;

    if (auto *STy = dyn_cast<StructType>(ETy)) {
      // Handle nested structs
      GetPads_1(STy, Pads, Base + SL->getElementOffset(I), NextOffset, DL);
    } else {
      // Handle other fields
      unsigned Size = DL.getTypeStoreSize(ETy);  // FIXME: getTypeAllocSize ?
      unsigned PadOffset = Base + SL->getElementOffset(I) + Size;
      if (unsigned PadSize = NextOffset - PadOffset) {
        MYDEBUG(errs() << "  " << PadOffset << ": " << PadSize << " bytes\n");
        Pads.push_back(std::make_pair(PadOffset, PadSize));
      }
    }
  }
}

static bool GetPads(StructType *Ty,
                    PadVectorImpl &Pads, const DataLayout &DL) {
  Pads.clear();
  MYDEBUG(errs() << "Paddings for type '"
               << (Ty->hasName() ? Ty->getName() : "<unknown>")
               << "'\n");
  GetPads_1(Ty, Pads, 0, DL.getTypeStoreSize(Ty), DL);
  return !Pads.empty();
}

static void WritePads(Value *S, PadVectorImpl &Pads,
                      IRBuilder<> &IRB, LLVMContext &C) {
  Value *BC = IRB.CreateBitCast(S, Type::getInt8PtrTy(C));
  for (unsigned J = 0, N = Pads.size(); J < N; ++J) {
    unsigned Offset = Pads[J].first,
      Size = Pads[J].second;
    Value *Pad = IRB.CreateAdd(BC, IRB.getInt32(Offset));
    IRB.CreateMemSet(Pad, IRB.getInt8(Magic), Size, /*isVolatile*/1);
  }
}

bool InstrumentAllocas::runOnFunction(Function &F) {
  struct AllocaFinder : public InstVisitor<AllocaFinder> {
    SmallVector<AllocaInst *, 10> Insts;
    void visitAllocaInst(AllocaInst &AI) { Insts.push_back(&AI); }
  } AF;
  AF.visit(F);

  bool Changed = false;
  const DataLayout &DL = F.getParent()->getDataLayout();
  for (auto I : AF.Insts)
    Changed |= instrumentAlloca(*I, DL);

  return Changed;
}

bool InstrumentAllocas::instrumentAlloca(AllocaInst &I, const DataLayout &DL) {
  auto *CI = dyn_cast_or_null<ConstantInt>(I.getArraySize());
  if (!CI || CI->isZero())
    return false;

  auto *STy = dyn_cast_or_null<StructType>(I.getAllocatedType());
  if (!STy)
    return false;

  PadVector Pads;
  if (!GetPads(STy, Pads, DL))
    return false;

  LLVMContext &C = I.getModule()->getContext();

  IRBuilder<> IRB(I.getNextNode());
  WritePads(&I, Pads, IRB, C);

  return true;
}

bool InstrumentMallocs::runOnFunction(Function &F) {
  struct BitCastFinder : public InstVisitor<BitCastFinder> {
    SmallVector<BitCastInst *, 10> Insts;
    void visitBitCastInst(BitCastInst &BCI) { Insts.push_back(&BCI); }
  } BCF;
  BCF.visit(F);

  bool Changed = false;
  const DataLayout &DL = F.getParent()->getDataLayout();
  auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  for (auto I : BCF.Insts)
    Changed |= instrumentBitCast(*I, DL, TLI);

  return Changed;
}

static bool IsMallocated(Value *V, TargetLibraryInfo&TLI) {
  auto *CI = dyn_cast<CallInst>(V);
  if (!CI)
    return false;

  auto *F = CI->getCalledFunction();
  if (!F)
    return false;

  LibFunc::Func LF;
  if (!(TLI.getLibFunc(F->getName(), LF) && TLI.has(LF)))
    return false;

  // TODO: any function with "alloc" suffix?
  switch (LF) {
  case LibFunc::Znwj:  // new(unsigned int)
  case LibFunc::Znwm:  // new(unsigned long)
  case LibFunc::Znaj:  // new[](unsigned int)
  case LibFunc::Znam:  // new[](unsigned long)
  case LibFunc::malloc:
  case LibFunc::calloc:  // XXX
  case LibFunc::realloc:
    return true;
  default:
    return false;
  }
}

bool InstrumentMallocs::instrumentBitCast(BitCastInst &I, const DataLayout &DL,
                                          TargetLibraryInfo &TLI) {
  auto *PtrTy = dyn_cast_or_null<PointerType>(I.getDestTy());
  if (!PtrTy)
    return false;

  auto *STy = dyn_cast_or_null<StructType>(PtrTy->getElementType());
  if (!STy)
    return false;

  Value *Ptr = I.getOperand(0);
  if (!IsMallocated(Ptr, TLI))
    return false;

  PadVector Pads;
  if (!GetPads(STy, Pads, DL))
    return false;

  Instruction *Next = I.getNextNode();
  IRBuilder<> IRB(Next);

  Value *Cmp = IRB.CreateIsNotNull(Ptr);

  TerminatorInst *JmpBack =
    SplitBlockAndInsertIfThen(Cmp, Next, /*Unreachable*/false);
  IRB.SetInsertPoint(JmpBack);

  WritePads(&I, Pads, IRB, I.getModule()->getContext());

  return true;
}

bool InstrumentGlobals::runOnModule(Module &M) {
  LLVMContext &C = M.getContext();
  const DataLayout &DL = M.getDataLayout();

  SmallVector<GlobalVariable *, 10> ToWipe;

  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
       I != E; ++I) {
    GlobalVariable *GV = &*I;

    auto *STy = dyn_cast_or_null<StructType>(GV->getValueType());
    if (!STy)
      return false;

    // Will replace local constants with new variable
    // TODO: why local?
    if (GV->isConstant() && !GV->hasLocalLinkage())
      continue;

    ToWipe.push_back(GV);
  }

  if (ToWipe.empty())
    return false;

  Function *Ctor = Function::Create(
    FunctionType::get(Type::getVoidTy(C), false),
    GlobalValue::InternalLinkage, "DirtyPad", &M);
  BasicBlock *CtorBB = BasicBlock::Create(C, "", Ctor);

  appendToGlobalCtors(M, Ctor, INT32_MIN);

  Instruction *Ret = ReturnInst::Create(C, CtorBB);
  IRBuilder<> IRB(Ret);

  for (unsigned I = 0, E = ToWipe.size(); I < E; ++I) {
    GlobalVariable *GV = ToWipe[I];

    auto *Ty = dyn_cast<StructType>(GV->getValueType());

    PadVector Pads;
    if (!GetPads(Ty, Pads, DL))
      continue;

    if (GV->isConstant()) {
       // TODO: probly need to copy other fields but enough for now
       assert(GV->hasLocalLinkage());
       GlobalVariable *NewGV = new GlobalVariable(M, GV->getValueType(),
                                                  /*isConstant*/false,
                                                  GV->getLinkage(),
                                                  GV->getInitializer());
       GV->replaceAllUsesWith(NewGV);
       GV->eraseFromParent();
       GV = NewGV;
    }

    WritePads(GV, Pads, IRB, C);
  }

  return true;
}

static void registerInstrumentAllocas(const PassManagerBuilder &,
                                      legacy::PassManagerBase &PM) {
  PM.add(new InstrumentAllocas());
}

// Allocas vanish after mem2reg so instrument early
static RegisterStandardPasses RegisterAllocaPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerInstrumentAllocas),
  RegisterAllocaPass_O0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerInstrumentAllocas);

static void registerInstrumentMallocs(const PassManagerBuilder &,
                                      legacy::PassManagerBase &PM) {
  PM.add(new InstrumentMallocs());
}

// Need to run after mem2reg to have usedef info
static RegisterStandardPasses RegisterMallocPass(
  PassManagerBuilder::EP_EarlyAsPossible, registerInstrumentMallocs);

static void registerInstrumentGlobals(const PassManagerBuilder &,
                                      legacy::PassManagerBase &PM) {
  PM.add(new InstrumentGlobals());
}

static RegisterStandardPasses RegisterGlobalsPass(
    PassManagerBuilder::EP_OptimizerLast, registerInstrumentGlobals),
  RegisterGlobalsPass_O0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, registerInstrumentGlobals);
