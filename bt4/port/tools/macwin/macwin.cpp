// The game's memory model on macOS / arm64, as an LLVM pass (opt -load-pass-plugin macwin.dylib -passes=bt3-window).
//
// The game's pointers are 4 bytes wide (port/tools/ptr32.py: `__ptr32 __uptr`, address space 271). On Linux and
// Windows they are plain addresses below 4 GB. macOS keeps the low 4 GB of every arm64 process unmapped, so here a
// game pointer is an OFFSET into a 4 GB window instead:
//
//     W = &__mh_execute_header - 0x20000000
//
// In code, W is LOADED from the variable Port_Window (port/src/plat_mem.c) at the start of each function, never
// written as the constant above: an address computed from a known symbol lets LLVM's alias analysis decide which
// object it reaches (that one symbol, one byte long), and it then treats game memory reached through W as
// unrelated to everything else: stores were dropped and loads reused (seen in Ot_Init). And W + offset is made
// with inttoptr, not as a GEP on W (see host()). Constant data keeps the constant form (it is only arithmetic there).
//
// The program is linked so that it starts 0x20000000 into the window, as the Linux and Windows programs start at
// 0x20000000; the PS2's memory (heap, stack, registers, scratchpad) is mapped at W + its PS2 address
// (port/src/plat_mem.c). An offset therefore has the same value a Linux address has, for everything the game
// computes with: heap, stack, the port's blocks.
//
// The pass:
//   - a 4-byte pointer used for memory (load, store, atomics, memcpy/memmove/memset, a call through it) is turned
//     into W + offset first;
//   - a conversion between a 4-byte and an ordinary pointer (addrspacecast, in code or in constant data) becomes
//     the arithmetic: to the game  p - W,  to the host  W + offset;  null stays null both ways;
//   - in the game's code, an ordinary pointer converted to a 32-bit (or smaller) integer is converted as a game
//     address (p - W), whether clang writes that as one ptrtoint or as a ptrtoint to 64 bits and a trunc (it does
//     for `(u32)param`: ptr32.py keeps parameters and results ordinary 64-bit pointers);
//   - pointers cross every call as host pointers: a function's 4-byte pointer parameters and result become
//     ordinary pointers (converted at its entry and at its returns), and every call converts its 4-byte pointer
//     arguments (and converts the result back). So the game can call the C library, the port's 64-bit code and
//     its own functions alike, and they can call it: exactly what happens on Linux, where the two are equal;
//   - every function is compiled with no floating-point unit (the game's arithmetic is software, as with
//     -msoft-float on x86: see undefined.py).
// It fails, naming the function, when a memory operation on a 4-byte pointer is left: the code generator would
// treat it as an absolute low address (and LLVM 22 also writes 32 bits for a narrow store through such a pointer,
// llvm/llvm-project#228782).
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>

using namespace llvm;

namespace {

// What GNU ld's --wrap and --defsym do in the Linux and Windows links (port/tools/link.py), done here because ld64
// has neither: a call to a declared NAME goes to __wrap_NAME and a call to __real_NAME to NAME (only declarations:
// calls inside the module that defines NAME stay, as with --wrap); a declared global NAME=ADDR is the game address
// ADDR, i.e. W + ADDR.
cl::list<std::string> Wrap("bt3-wrap", cl::desc("functions to wrap, as GNU ld --wrap"), cl::CommaSeparated);
cl::list<std::string> Abs("bt3-abs", cl::desc("NAME=ADDR: a symbol at a game address"), cl::CommaSeparated);
// The port's own files: an ordinary pointer cast to a 32-bit integer there is not taken for a game address (the port
// converts with Port_Addr, port/src/port_addr.h, and the optimizer may turn that into such a cast).
cl::opt<bool> Report("bt3-report", cl::desc("list host pointers made into integers outside the window arithmetic"), cl::init(false));
cl::opt<bool> PortModule("bt3-port", cl::desc("a port source, not the game's"), cl::init(false));
// Declarations that disagree with their definition. The game's sources often declare a function another file defines
// with an integer where the definition has a pointer, or the reverse (`s32 EftShot_GetCharPack()` in one file,
// `void *` in another), or as variadic when it is not. On Linux and Windows an address is the same 4 bytes either
// way (and x86-64 passes variadic arguments in registers too); here a pointer crosses a call as W + offset and an
// integer as the offset, and arm64 passes variadic arguments on the stack. So the build first lists every
// definition of the program (-bt3-defs: one line per function, "NAME KINDS", written by each module), then each call
// to a function this module only declares is made in the definition's terms (-bt3-sigs: that list): a pointer
// argument for an integer parameter becomes its game address, an integer for a pointer becomes W + it, the result
// likewise, and the call is a fixed-argument one when the definition is.
cl::opt<std::string> Defs("bt3-defs", cl::desc("write this module's function definitions to FILE, change nothing"), cl::init(""));
cl::opt<std::string> Sigs("bt3-sigs", cl::desc("the program's function definitions (FILE from -bt3-defs)"), cl::init(""));

constexpr unsigned GAME = 271; // __ptr32 __uptr: the game's 4-byte pointers
constexpr unsigned HOST64 = 272; // __ptr64: ordinary 64-bit pointers (ptr32.py uses them for variadic arguments)
constexpr int64_t PROGRAM_OFFSET = 0x20000000; // where the program starts in the window

struct Window {
    Module &M;
    LLVMContext &Ctx;
    Type *I64, *I32, *I8;
    PointerType *Host, *Game;
    Constant *W = nullptr;
    std::map<Function *, Value *> WIn; // per function: the base loaded from Port_Window
    std::map<Constant *, Constant *> Done;
    std::map<Constant *, Constant *> HostOf; // a 4-byte pointer constant made from a host address -> that address
    bool Failed = false;

    explicit Window(Module &M)
        : M(M), Ctx(M.getContext()), I64(Type::getInt64Ty(Ctx)), I32(Type::getInt32Ty(Ctx)), I8(Type::getInt8Ty(Ctx)),
          Host(PointerType::get(Ctx, 0)), Game(PointerType::get(Ctx, GAME)) {}

    Constant *window() {
        if (!W) {
            GlobalVariable *mh = M.getGlobalVariable("_mh_execute_header");
            if (!mh) {
                mh = new GlobalVariable(M, I8, true, GlobalValue::ExternalLinkage, nullptr, "_mh_execute_header");
                mh->setVisibility(GlobalValue::HiddenVisibility);
                mh->setDSOLocal(true);
            }
            W = ConstantExpr::getGetElementPtr(I8, mh, ConstantInt::get(I64, -PROGRAM_OFFSET));
        }
        return W;
    }

    bool isGame(Type *T) { return T->isPointerTy() && T->getPointerAddressSpace() == GAME; }
    bool isHostLike(Type *T) {
        return T->isPointerTy() && (T->getPointerAddressSpace() == 0 || T->getPointerAddressSpace() == HOST64);
    }

    // ---- constants (data and constant operands)
    Constant *hostC(Constant *off, PointerType *To) { // a 4-byte pointer constant as a host address
        if (off->isNullValue()) {
            return ConstantPointerNull::get(To);
        }
        auto known = HostOf.find(off);
        if (known != HostOf.end()) { // W + (p - W): p itself
            return To->getAddressSpace() == 0 ? known->second : ConstantExpr::getAddrSpaceCast(known->second, To);
        }
        Constant *p = ConstantExpr::getGetElementPtr(I8, window(), ConstantExpr::getPtrToInt(off, I64));
        return To->getAddressSpace() == 0 ? p : ConstantExpr::getAddrSpaceCast(p, To);
    }
    Constant *gameC(Constant *p) { // a host address constant as a 4-byte pointer
        if (p->isNullValue()) {
            return ConstantPointerNull::get(Game);
        }
        if (p->getType()->getPointerAddressSpace() != 0) {
            p = ConstantExpr::getAddrSpaceCast(p, Host);
        }
        Constant *d = ConstantExpr::getSub(ConstantExpr::getPtrToInt(p, I64), ConstantExpr::getPtrToInt(window(), I64));
        Constant *off = ConstantExpr::getIntToPtr(d, Game);
        HostOf[off] = p;
        return off;
    }
    Constant *fixC(Constant *C) {
        if (isa<GlobalValue>(C) || isa<ConstantData>(C)) {
            return C;
        }
        auto it = Done.find(C);
        if (it != Done.end()) {
            return it->second;
        }
        std::vector<Constant *> ops;
        bool changed = false;
        for (Use &U : C->operands()) {
            Constant *o = fixC(cast<Constant>(U.get()));
            changed |= o != U.get();
            ops.push_back(o);
        }
        Constant *R = C;
        if (auto *CE = dyn_cast<ConstantExpr>(C)) {
            Type *from = CE->getOperand(0)->getType(), *to = CE->getType();
            if (CE->getOpcode() == Instruction::AddrSpaceCast && isGame(from) && isHostLike(to)) {
                R = hostC(ops[0], cast<PointerType>(to));
            } else if (CE->getOpcode() == Instruction::AddrSpaceCast && isHostLike(from) && isGame(to)) {
                R = gameC(ops[0]);
            } else if (!PortModule && CE->getOpcode() == Instruction::PtrToInt && from->getPointerAddressSpace() == 0 &&
                       to->getIntegerBitWidth() <= 32) {
                R = ConstantExpr::getPtrToInt(gameC(ops[0]), to);
            } else if (!PortModule && CE->getOpcode() == Instruction::Trunc && to->getIntegerBitWidth() <= 32 &&
                       isa<ConstantExpr>(ops[0]) && cast<ConstantExpr>(ops[0])->getOpcode() == Instruction::PtrToInt &&
                       cast<ConstantExpr>(ops[0])->getOperand(0)->getType()->getPointerAddressSpace() == 0) {
                R = ConstantExpr::getPtrToInt(gameC(cast<ConstantExpr>(ops[0])->getOperand(0)), to);
            } else if (changed) {
                R = CE->getWithOperands(ops);
            }
        } else if (changed) {
            if (auto *S = dyn_cast<ConstantStruct>(C)) {
                R = ConstantStruct::get(S->getType(), ops);
            } else if (auto *A = dyn_cast<ConstantArray>(C)) {
                R = ConstantArray::get(A->getType(), ops);
            } else if (auto *V = dyn_cast<ConstantVector>(C)) {
                R = ConstantVector::get(ops);
            }
        }
        Done[C] = R;
        return R;
    }

    // ---- values in code
    static bool knownNonNull(Value *V) {
        V = V->stripInBoundsOffsets();
        return isa<AllocaInst>(V) || isa<GlobalValue>(V);
    }
    Value *windowIn(IRBuilder<> &B) {
        Function *F = B.GetInsertBlock()->getParent();
        auto it = WIn.find(F);
        if (it != WIn.end()) {
            return it->second;
        }
        GlobalVariable *var = M.getGlobalVariable("Port_Window");
        if (!var) {
            var = new GlobalVariable(M, Host, false, GlobalValue::ExternalLinkage, nullptr, "Port_Window");
        }
        IRBuilder<> EB(&*F->getEntryBlock().getFirstInsertionPt());
        LoadInst *w = EB.CreateLoad(Host, var, "w");
        w->setMetadata(LLVMContext::MD_invariant_load, MDNode::get(Ctx, {}));
        WIn[F] = w;
        return w;
    }
    Value *host(IRBuilder<> &B, Value *off, bool mayBeNull, PointerType *To) {
        if (auto *C = dyn_cast<Constant>(off)) {
            if (C->isNullValue() || HostOf.count(C)) {
                return hostC(C, To);
            }
            mayBeNull = false;
        }
        // inttoptr, not a GEP on W: a pointer made from W has W's provenance, and LLVM concludes from it that it cannot
        // reach a local or a global whose address was taken later (game(): ptrtoint), then drops stores to it
        // (StgRigid_Create's inlined `link = &head`). An integer made into a pointer may reach any object whose
        // address was exposed, which is what a game address is.
        Value *p = B.CreateIntToPtr(B.CreateAdd(B.CreatePtrToInt(windowIn(B), I64), B.CreatePtrToInt(off, I64)), Host, "w.host");
        if (mayBeNull) {
            p = B.CreateSelect(B.CreateIsNull(off), ConstantPointerNull::get(Host), p);
        }
        return To->getAddressSpace() == 0 ? p : B.CreateAddrSpaceCast(p, To);
    }
    Value *game(IRBuilder<> &B, Value *p, bool mayBeNull = true) {
        if (auto *C = dyn_cast<Constant>(p)) {
            return gameC(C);
        }
        if (p->getType()->getPointerAddressSpace() != 0) {
            p = B.CreateAddrSpaceCast(p, Host);
        }
        Value *off = B.CreateIntToPtr(B.CreateSub(B.CreatePtrToInt(p, I64), B.CreatePtrToInt(window(), I64)), Game, "w.game");
        return !mayBeNull || knownNonNull(p) ? off : B.CreateSelect(B.CreateIsNull(p), ConstantPointerNull::get(Game), off);
    }
    Value *mem(Instruction *I, Value *ptr) { // a pointer used to reach memory: no null case to keep
        if (!isGame(ptr->getType())) {
            return ptr;
        }
        IRBuilder<> B(I);
        return host(B, ptr, false, Host);
    }

    void fixFunction(Function &F) {
        std::vector<Instruction *> work;
        for (Instruction &I : instructions(F)) {
            work.push_back(&I);
        }
        for (Instruction *I : work) {
            for (Use &U : I->operands()) {
                if (auto *C = dyn_cast<Constant>(U.get())) {
                    if (!isa<BasicBlock>(U.get())) {
                        Constant *f = fixC(C);
                        if (f != C) {
                            U.set(f);
                        }
                    }
                }
            }
            IRBuilder<> B(I);
            if (auto *AC = dyn_cast<AddrSpaceCastInst>(I)) {
                Type *from = AC->getSrcTy(), *to = AC->getDestTy();
                Value *R = nullptr;
                if (isGame(from) && isHostLike(to)) {
                    R = host(B, AC->getOperand(0), true, cast<PointerType>(to));
                } else if (isHostLike(from) && isGame(to)) {
                    R = game(B, AC->getOperand(0));
                }
                if (R) {
                    AC->replaceAllUsesWith(R);
                    AC->eraseFromParent();
                }
            } else if (auto *IP = dyn_cast<IntToPtrInst>(I)) {
                // the game's code: a 32-bit value made into an ordinary pointer is a game address
                Value *v = IP->getOperand(0);
                if (isa<ZExtInst>(v) || isa<SExtInst>(v)) { // (void *)u32 / (void *)s32: the same 4 bytes
                    v = cast<CastInst>(v)->getOperand(0);
                }
                if (!PortModule && IP->getAddressSpace() == 0 && v->getType()->getIntegerBitWidth() <= 32) {
                    Value *off = B.CreateIntToPtr(B.CreateZExt(v, Type::getInt32Ty(Ctx)), Game);
                    Value *R = host(B, off, true, cast<PointerType>(IP->getType()));
                    IP->replaceAllUsesWith(R);
                    IP->eraseFromParent();
                }
            } else if (auto *TI = dyn_cast<TruncInst>(I)) {
                auto *PI = dyn_cast<PtrToIntInst>(TI->getOperand(0));
                if (!PortModule && PI && PI->getPointerAddressSpace() == 0 && TI->getType()->getIntegerBitWidth() <= 32) {
                    Value *R = B.CreatePtrToInt(game(B, PI->getPointerOperand()), TI->getType());
                    TI->replaceAllUsesWith(R);
                    TI->eraseFromParent();
                }
            } else if (auto *PI = dyn_cast<PtrToIntInst>(I)) {
                if (!PortModule && PI->getPointerAddressSpace() == 0 && PI->getType()->getIntegerBitWidth() <= 32) {
                    PI->setOperand(0, game(B, PI->getOperand(0)));
                }
            } else if (auto *L = dyn_cast<LoadInst>(I)) {
                L->setOperand(L->getPointerOperandIndex(), mem(I, L->getPointerOperand()));
            } else if (auto *S = dyn_cast<StoreInst>(I)) {
                S->setOperand(S->getPointerOperandIndex(), mem(I, S->getPointerOperand()));
            } else if (auto *A = dyn_cast<AtomicRMWInst>(I)) {
                A->setOperand(A->getPointerOperandIndex(), mem(I, A->getPointerOperand()));
            } else if (auto *X = dyn_cast<AtomicCmpXchgInst>(I)) {
                X->setOperand(X->getPointerOperandIndex(), mem(I, X->getPointerOperand()));
            } else if (auto *MI = dyn_cast<MemIntrinsic>(I)) {
                fixMemIntrinsic(MI);
            } else if (auto *CB = dyn_cast<CallBase>(I)) {
                if (isGame(CB->getCalledOperand()->getType())) {
                    CB->setCalledOperand(mem(I, CB->getCalledOperand()));
                }
            }
        }
    }

    void fixMemIntrinsic(MemIntrinsic *MI) {
        bool game = isGame(MI->getRawDest()->getType());
        if (auto *MT = dyn_cast<MemTransferInst>(MI)) {
            game |= isGame(MT->getRawSource()->getType());
        }
        if (!game) {
            return;
        }
        IRBuilder<> B(MI);
        Value *dst = mem(MI, MI->getRawDest());
        CallInst *N;
        if (auto *MS = dyn_cast<MemSetInst>(MI)) {
            N = B.CreateMemSet(dst, MS->getValue(), MS->getLength(), MS->getDestAlign(), MS->isVolatile());
        } else {
            auto *MT = cast<MemTransferInst>(MI);
            Value *src = mem(MI, MT->getRawSource());
            N = isa<MemMoveInst>(MT) ? B.CreateMemMove(dst, MT->getDestAlign(), src, MT->getSourceAlign(), MT->getLength(), MT->isVolatile())
                                     : B.CreateMemCpy(dst, MT->getDestAlign(), src, MT->getSourceAlign(), MT->getLength(), MT->isVolatile());
        }
        N->copyMetadata(*MI);
        MI->replaceAllUsesWith(N);
        MI->eraseFromParent();
    }

    // ---- calls: pointers cross every call as host pointers
    bool gameSig(FunctionType *FT) {
        if (isGame(FT->getReturnType())) {
            return true;
        }
        for (Type *T : FT->params()) {
            if (isGame(T)) {
                return true;
            }
        }
        return false;
    }
    FunctionType *hostSig(FunctionType *FT) {
        auto h = [&](Type *T) -> Type * { return isGame(T) ? static_cast<Type *>(Host) : T; };
        std::vector<Type *> ps;
        for (Type *T : FT->params()) {
            ps.push_back(h(T));
        }
        return FunctionType::get(h(FT->getReturnType()), ps, FT->isVarArg());
    }
    void fixSignature(Function *F) {
        FunctionType *FT = F->getFunctionType();
        if (F->isIntrinsic() || !gameSig(FT)) {
            return;
        }
        Function *NF = Function::Create(hostSig(FT), F->getLinkage(), F->getAddressSpace(), "", &M);
        NF->copyAttributesFrom(F);
        NF->setComdat(F->getComdat());
        NF->copyMetadata(F, 0);
        NF->takeName(F);
        if (!F->isDeclaration()) {
            NF->splice(NF->begin(), F);
            IRBuilder<> B(&*NF->getEntryBlock().getFirstInsertionPt());
            for (auto it : zip(F->args(), NF->args())) {
                Argument &old = std::get<0>(it), &nw = std::get<1>(it);
                nw.takeName(&old);
                old.replaceAllUsesWith(isGame(old.getType()) ? game(B, &nw, !nw.hasNonNullAttr()) : static_cast<Value *>(&nw));
            }
            if (isGame(FT->getReturnType())) {
                for (BasicBlock &BB : *NF) {
                    if (auto *R = dyn_cast<ReturnInst>(BB.getTerminator())) {
                        IRBuilder<> RB(R);
                        R->setOperand(0, host(RB, R->getReturnValue(), true, Host));
                    }
                }
            }
        }
        F->replaceAllUsesWith(NF); // (opaque pointers: a function is a `ptr` whatever its type)
        F->eraseFromParent();
    }
    void fixCall(CallInst *CI) {
        if (CI->getIntrinsicID() != Intrinsic::not_intrinsic || CI->isInlineAsm()) {
            return;
        }
        FunctionType *FT = CI->getFunctionType();
        bool gameArg = false;
        for (Value *a : CI->args()) {
            gameArg |= isGame(a->getType());
        }
        if (!gameSig(FT) && !gameArg) {
            return;
        }
        IRBuilder<> B(CI);
        std::vector<Value *> args;
        for (unsigned i = 0; i < CI->arg_size(); i++) {
            Value *a = CI->getArgOperand(i);
            args.push_back(isGame(a->getType()) ? host(B, a, !CI->paramHasAttr(i, Attribute::NonNull), Host) : a);
        }
        CallInst *N = B.CreateCall(hostSig(FT), CI->getCalledOperand(), args);
        N->setCallingConv(CI->getCallingConv());
        N->setTailCallKind(CI->getTailCallKind() == CallInst::TCK_MustTail ? CallInst::TCK_None : CI->getTailCallKind());
        N->setAttributes(CI->getAttributes());
        N->copyMetadata(*CI);
        N->takeName(CI);
        Value *R = N;
        if (isGame(FT->getReturnType())) {
            B.SetInsertPoint(CI->getNextNode());
            R = game(B, N);
        }
        CI->replaceAllUsesWith(R);
        CI->eraseFromParent();
    }

    // A function the game calls through a pointer may be called with a game address where it takes a pointer: the
    // caller's pointer type says s32 (`McFlowCb` is `void (*)(s32)`, Title_OnCardDone takes `s32 *`, and
    // McFlow_SetDoneCb keeps the `s32 *` as an s32). On Linux and Windows those are the same 4 bytes; here the
    // callee would get the bare offset. So such a function's pointer parameters are taken as a game address when their
    // high 32 bits are 0 (nothing in the window has them, W being at 1 TB; null stays null), and so are the pointer
    // results of indirect calls, for the reverse case. The other way round (a pointer for an integer) needs nothing:
    // W is on a 4 GB boundary, so a pointer's low 32 bits are its game address.
    void offsetToHost(IRBuilder<> &B, Value *p) { // replaces p's uses
        Value *w = B.CreatePtrToInt(windowIn(B), I64);
        auto *v = cast<Instruction>(B.CreatePtrToInt(p, I64));
        Value *bare = B.CreateAnd(B.CreateICmpEQ(B.CreateLShr(v, 32), ConstantInt::get(I64, 0)), B.CreateICmpNE(v, ConstantInt::get(I64, 0)));
        Value *R = B.CreateIntToPtr(B.CreateSelect(bare, B.CreateAdd(w, v), v), p->getType(), "w.bare");
        p->replaceUsesWithIf(R, [&](Use &U) { return U.getUser() != v; });
    }
    void fixCalledThroughPointer(Function &F) {
        if (PortModule || !F.hasAddressTaken()) {
            return;
        }
        IRBuilder<> B(&*F.getEntryBlock().getFirstInsertionPt());
        B.SetInsertPoint(cast<Instruction>(windowIn(B))->getNextNode()); // (after W's load, which may be there already)
        for (Argument &A : F.args()) {
            if (A.getType()->isPointerTy() && A.getType()->getPointerAddressSpace() == 0 && !A.use_empty()) {
                offsetToHost(B, &A);
            }
        }
    }
    void fixIndirectResult(CallInst *CI) {
        if (PortModule || CI->getCalledFunction() || CI->isInlineAsm() || !CI->getType()->isPointerTy() ||
            CI->getType()->getPointerAddressSpace() != 0 || CI->use_empty()) {
            return;
        }
        IRBuilder<> B(CI->getNextNode());
        offsetToHost(B, CI);
    }

    static char kindOf(Type *T) { // KINDS: v(oid), p(ointer), i(nteger of 32 bits or less), x (anything else); then '.' if variadic
        if (T->isVoidTy()) {
            return 'v';
        }
        if (T->isPointerTy()) {
            return 'p';
        }
        if (T->isIntegerTy() && T->getIntegerBitWidth() <= 32) {
            return 'i';
        }
        return 'x';
    }
    static std::string kinds(FunctionType *FT) {
        std::string k(1, kindOf(FT->getReturnType()));
        for (Type *P : FT->params()) {
            k += kindOf(P);
        }
        return FT->isVarArg() ? k + "." : k;
    }
    void writeDefs() {
        std::error_code ec;
        raw_fd_ostream out(Defs, ec);
        for (Function &F : M) {
            if (!F.isDeclaration() && !F.hasLocalLinkage()) {
                out << F.getName() << " " << kinds(F.getFunctionType()) << "\n";
            }
        }
    }
    void fixAcross(CallInst *CI, const std::map<std::string, std::string> &db) {
        Function *F = CI->getCalledFunction();
        if (!F || !F->isDeclaration() || F->isIntrinsic()) {
            return;
        }
        auto it = db.find(F->getName().str());
        if (it == db.end()) {
            return;
        }
        const std::string &k = it->second;
        bool var = k.back() == '.';
        size_t np = k.size() - 1 - var;
        FunctionType *FT = CI->getFunctionType();
        IRBuilder<> B(CI);
        bool change = FT->isVarArg() && !var;
        std::vector<Value *> args;
        std::vector<Type *> fixed;
        for (unsigned i = 0; i < CI->arg_size(); i++) {
            Value *a = CI->getArgOperand(i);
            char want = i < np ? k[1 + i] : 0, have = kindOf(a->getType());
            if (want == 'i' && have == 'p') {
                a = B.CreatePtrToInt(game(B, a), I32);
                change = true;
            } else if (want == 'p' && have == 'i') {
                a = host(B, B.CreateIntToPtr(B.CreateZExtOrTrunc(a, I32), Game), true, Host);
                change = true;
            }
            args.push_back(a);
            if (i < FT->getNumParams() || !var) {
                fixed.push_back(a->getType());
            }
        }
        char rw = k[0], rh = kindOf(FT->getReturnType());
        bool ret = (rw == 'i' && rh == 'p') || (rw == 'p' && rh == 'i');
        if (!change && !ret) {
            return;
        }
        Type *RT = !ret ? FT->getReturnType() : rw == 'p' ? (Type *)Host : (Type *)I32;
        CallInst *N = B.CreateCall(FunctionType::get(RT, fixed, FT->isVarArg() && var), F, args);
        N->setCallingConv(CI->getCallingConv());
        N->setTailCallKind(CallInst::TCK_None);
        N->copyMetadata(*CI);
        N->takeName(CI);
        Value *R = N;
        if (ret) {
            if (rw == 'i') { // the definition returns the game address of what the caller takes for a pointer
                R = host(B, B.CreateIntToPtr(N, Game), true, cast<PointerType>(FT->getReturnType()));
            } else { // the definition returns a pointer the caller takes for an integer
                R = B.CreateZExtOrTrunc(B.CreatePtrToInt(game(B, N), I32), FT->getReturnType());
            }
        }
        CI->replaceAllUsesWith(R);
        CI->eraseFromParent();
    }

    void check(Function &F) {
        for (Instruction &I : instructions(F)) {
            Value *p = nullptr;
            if (auto *L = dyn_cast<LoadInst>(&I)) {
                p = L->getPointerOperand();
            } else if (auto *S = dyn_cast<StoreInst>(&I)) {
                p = S->getPointerOperand();
            } else if (auto *A = dyn_cast<AtomicRMWInst>(&I)) {
                p = A->getPointerOperand();
            } else if (auto *X = dyn_cast<AtomicCmpXchgInst>(&I)) {
                p = X->getPointerOperand();
            } else if (auto *CB = dyn_cast<CallBase>(&I)) {
                p = CB->getCalledOperand();
                for (Value *a : CB->args()) {
                    if (CB->getIntrinsicID() != Intrinsic::not_intrinsic && isGame(a->getType())) {
                        p = a;
                    }
                }
            } else if (auto *AC = dyn_cast<AddrSpaceCastInst>(&I)) {
                if (isGame(AC->getSrcTy()) || isGame(AC->getDestTy())) {
                    p = AC->getOperand(0)->getType() == Game ? AC->getOperand(0) : AC;
                }
            }
            if (p && isGame(p->getType())) {
                errs() << "macwin: a 4-byte pointer is still used directly in " << F.getName() << ": " << I << "\n";
                Failed = true;
            }
        }
    }

    void linkNames() {
        // `__asm__("Name")` labels (the game gives some declarations their symbol that way): on ELF the label is the
        // symbol C would make, on Mach-O C's symbol has a leading underscore and the label has none. Made ordinary
        // names again, so that they meet their definitions.
        std::vector<GlobalValue *> labelled;
        for (GlobalValue &G : M.global_values()) {
            if (G.getName().starts_with("\1") && !G.getName().starts_with("\1_")) { // (the SDK's own labels, "\1_fopen", are Mach-O names already)
                labelled.push_back(&G);
            }
        }
        for (GlobalValue *G : labelled) {
            std::string name = G->getName().drop_front(1).str();
            if (GlobalValue *other = M.getNamedValue(name)) {
                G->replaceAllUsesWith(other);
                G->eraseFromParent();
            } else {
                G->setName(name);
            }
        }
        for (const std::string &n : Wrap) {
            Function *real = M.getFunction("__real_" + n), *f = M.getFunction(n);
            if (f && f->isDeclaration()) {
                f->setName("__wrap_" + n);
            }
            if (real && real->isDeclaration()) {
                Function *other = M.getFunction(n);
                if (other) { // the module also declares or defines NAME itself: send the __real_ calls there
                    real->replaceAllUsesWith(other);
                    real->eraseFromParent();
                } else {
                    real->setName(n);
                }
            }
        }
        for (const std::string &a : Abs) {
            size_t eq = a.find('=');
            GlobalVariable *g = M.getGlobalVariable(a.substr(0, eq), true);
            if (g && g->isDeclaration()) {
                uint64_t addr = std::stoull(a.substr(eq + 1), nullptr, 0);
                g->replaceAllUsesWith(ConstantExpr::getGetElementPtr(I8, window(), ConstantInt::get(I64, addr)));
                g->eraseFromParent();
            }
        }
    }

    void report(Function &F) {
        for (Instruction &I : instructions(F)) {
            auto *PI = dyn_cast<PtrToIntInst>(&I);
            if (!PI || PI->getPointerAddressSpace() != 0) {
                continue;
            }
            for (User *U : PI->users()) {
                auto *BO = dyn_cast<BinaryOperator>(U);
                if (BO && BO->getOpcode() == Instruction::Sub && isa<Constant>(BO->getOperand(1))) {
                    continue; // p - W
                }
                if (isa<ICmpInst>(U)) {
                    continue;
                }
                errs() << "macwin-report: " << M.getSourceFileName() << " " << F.getName() << ": " << *PI << "  used by " << *U << "\n";
            }
        }
    }

    bool run() {
        linkNames();
        if (!Defs.empty()) {
            writeDefs();
            return true;
        }
        std::vector<Function *> fs;
        for (Function &F : M) {
            fs.push_back(&F);
        }
        for (Function *F : fs) {
            fixSignature(F);
        }
        for (Function &F : M) {
            std::vector<CallInst *> calls;
            for (Instruction &I : instructions(F)) {
                if (auto *CI = dyn_cast<CallInst>(&I)) {
                    calls.push_back(CI);
                }
            }
            for (CallInst *CI : calls) {
                fixCall(CI);
            }
        }
        for (GlobalVariable &G : M.globals()) {
            if (G.hasInitializer()) {
                Constant *f = fixC(G.getInitializer());
                if (f != G.getInitializer()) {
                    G.setInitializer(f);
                }
            }
        }
        std::map<std::string, std::string> db;
        if (!Sigs.empty()) {
            auto buf = MemoryBuffer::getFile(Sigs);
            if (!buf) {
                errs() << "macwin: cannot read " << Sigs << "\n";
                return false;
            }
            SmallVector<StringRef> lines;
            (*buf)->getBuffer().split(lines, '\n', -1, false);
            for (StringRef l : lines) {
                auto [name, k] = l.split(' ');
                db[name.str()] = k.str();
            }
        }
        for (Function &F : M) {
            if (F.isDeclaration()) {
                continue;
            }
            fixCalledThroughPointer(F);
            fixFunction(F);
            {
                std::vector<CallInst *> calls;
                for (Instruction &I : instructions(F)) {
                    if (auto *CI = dyn_cast<CallInst>(&I)) {
                        calls.push_back(CI);
                    }
                }
                for (CallInst *CI : calls) {
                    fixIndirectResult(CI);
                }
            }
            if (!db.empty()) {
                std::vector<CallInst *> calls;
                for (Instruction &I : instructions(F)) {
                    if (auto *CI = dyn_cast<CallInst>(&I)) {
                        calls.push_back(CI);
                    }
                }
                for (CallInst *CI : calls) {
                    fixAcross(CI, db);
                }
            }
            check(F);
            if (Report) {
                report(F);
            }
            F.removeFnAttr("target-cpu");
            F.addFnAttr("target-features", "-fp-armv8,-neon");
        }
        return !Failed;
    }
};

struct WindowPass : PassInfoMixin<WindowPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        if (!Window(M).run()) {
            report_fatal_error("macwin: the game's memory model could not be applied (see above)", false);
        }
        return PreservedAnalyses::none();
    }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "BT3Window", "1", [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "bt3-window") {
                        MPM.addPass(WindowPass());
                        return true;
                    }
                    return false;
                });
            }};
}
