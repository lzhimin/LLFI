#include "llvm/Instructions.h"
#include <fstream>
#include <iostream>
#include "FIInstSelector.h"
#include "FICustomSelectorManager.h"
#include "llvm/Intrinsics.h"
 
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CFG.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/GraphTraits.h"

/**
 * This instruction selector only selects the Memory allocation functions as target
 */
using namespace llvm;
namespace llfi {
class _Mem_BufferOverflow2InstSelector: public FIInstSelector {
private: 
  

  virtual bool isInstFITarget(Instruction *inst)

 {
         if(isa<CallInst>(inst))
                                 {
          CallInst* CI=dyn_cast<CallInst>(inst);
              Function* called_func=CI->getCalledFunction();
                //errs()<<called_func->getName()<<"\n";
                 if ((called_func->getName()=="llvm.memcpy.p0i8.p0i8.i64")||(called_func->getName()=="llvm.memmove.p0i8.p0i8.i64"))
                 {  
                std::ofstream outf("Automation-config");
                        outf << "MemBufOverflow2" << "\n";
                         outf.close();  
           return true;
                 }
       else
         return false;
                                 }
 }
                                               };

static RegisterFIInstSelector X( "BufferOverflow-memmove(MEM)", new _Mem_BufferOverflow2InstSelector());
}
