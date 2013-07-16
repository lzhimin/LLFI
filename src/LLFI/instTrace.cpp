/***************
instTrace.cpp
Author: Sam Coulter
	This llvm pass is part of the greater LLFI framework
	
	Run the pass with the opt -instTrace option after loading LLFI.so
	
	This pass injects a function call before every non-void-returning, 
	non-phi-node instruction that prints trace information about the executed
	instruction to a file specified during the pass.
***************/

#include <vector>
#include <cmath>

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/GlobalValue.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetData.h"

#include "utils.h"

using namespace llvm;

cl::opt<std::string> OutputFilename("tout", 
									cl::desc("Specify output filename"), 
									cl::value_desc("filename"),
									cl::init("traceOutput"));
cl::opt<bool> debugTrace( "debugTrace",
							cl::desc("Output Trace insertion information"),
							cl::init(false));
cl::opt<int> maxTrace( "maxTrace",
						cl::desc("Maximum number of instructions that will be traced after fault"),
						cl::init(-1));

namespace llfi {

struct instTrace : public FunctionPass {

static char ID;
Function::iterator lastBlock;
BasicBlock::iterator lastInst;
char *oFilename;
int oFilenameLength;

instTrace() : FunctionPass(ID) {}

//Add AnalysisUsage Pass as prerequisite for instTrace Pass
virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetData>();
 }

virtual bool doInitialization(Module &M) {
	oFilenameLength = OutputFilename.size() + 1;
	oFilename = new char[oFilenameLength];
	std::copy(OutputFilename.begin(), OutputFilename.end(), oFilename);
	oFilename[OutputFilename.size()] = '\0'; // don't forget the terminating 0
	return true;
}

virtual bool doFinalization(Module &) {
	delete[] oFilename;
	return true;
}

long fetchLLFIInstructionID(Instruction *targetInst) {
	return llfi::getLLFIIndexofInst(targetInst);
}

virtual bool runOnFunction(Function &F) {

	//Create handles to the functions parent module and context
	LLVMContext& context = F.getContext();
	Module *M = F.getParent();			

	//iterate through each basicblock of the function
	for (Function::iterator blockIterator = F.begin(), lastBlock = F.end();
		blockIterator != lastBlock; ++blockIterator) {

		BasicBlock* block = blockIterator;

		//Iterate through each instruction of the basicblock
		for (BasicBlock::iterator instIterator = blockIterator->begin(), 
			 lastInst = blockIterator->end(); 
			 instIterator != lastInst;
			 ++instIterator) {

			//Print some Debug Info as the pass is being run
			Instruction *inst = instIterator;

			if (debugTrace) {
				errs() << llfi::isLLFIIndexedInst(inst) << " instTrace: Found Instruction\n";
				if (!llfi::isLLFIIndexedInst(inst)) {
					errs() << "   Instruction was not indexed\n";
				} else {
					errs() << "   Opcode Name: " << inst->getOpcodeName() << "\n"
					   	   << "   Opcode: " << inst->getOpcode() << "\n"
					       << "   Parent Function Name: " << inst->getParent()->getParent()->getNameStr() << "\n";
				}
			}
			if (inst->getType() != Type::getVoidTy(context) && 
				llfi::isLLFIIndexedInst(inst) && 
				inst != block->getTerminator()) {

				//Find instrumentation point for current instruction
				Instruction *insertPoint = llfi::getInsertPtrforRegsofInst(inst, inst);
				//insert an instruction Allocate stack memory to store/pass instruction value
				AllocaInst* ptrInst = new AllocaInst(inst->getType(), NULL, "", insertPoint);
				//Insert an instruction to Store the instruction Value!
				new StoreInst(inst, ptrInst, insertPoint);
				
				//Insert instructions to allocate stack memory for output filename
				llvm::Value* oFileValue = llvm::ConstantArray::get(context, oFilename);
				AllocaInst* ofileptrInst = new AllocaInst(oFileValue->getType(), NULL, "", insertPoint);
				new StoreInst(oFileValue, ofileptrInst, insertPoint);

				//Insert instructions to allocate stack memory for opcode name
				llvm::Value* OPCodeName = llvm::ConstantArray::get(context, inst->getOpcodeName());
				AllocaInst* OPCodePtr = new AllocaInst(OPCodeName->getType(), NULL, "", insertPoint);
				new StoreInst(OPCodeName, OPCodePtr, insertPoint);

				//Create the decleration of the printInstTracer Function
				std::vector<const Type*> parameterVector(6);
				parameterVector[0] = Type::getInt32Ty(context); //ID
				parameterVector[1] = OPCodePtr->getType(); 		//Ptr to OpCode
				parameterVector[2] = Type::getInt32Ty(context); //Size of Inst Value
				parameterVector[3] = ptrInst->getType();		//Ptr to Inst Value
				parameterVector[4] = ofileptrInst->getType();	//Ptr to name of ofile
				parameterVector[5] = Type::getInt32Ty(context); //Int of max traces
				
				FunctionType* traceFuncType = FunctionType::get(Type::getVoidTy(context), parameterVector, 0 );
				Constant *traceFunc = M->getOrInsertFunction("printInstTracer", traceFuncType); 

				//Insert the tracing function, passing it the proper arguments
				std::vector<Value*> traceArgs;
				//Fetch the LLFI Instruction ID:
				ConstantInt* IDConstInt = ConstantInt::get(IntegerType::get(context,32), fetchLLFIInstructionID(inst));
				
				//Fetch size of instruction value				
				TargetData &td = getAnalysis<TargetData>();
				//The size must be rounded up before conversion to bytes because some data in llvm
				//can be like 6 bits if it only needs 6 bits out of an 8bit/1byte data type
  				float bitSize = (float)td.getTypeSizeInBits(inst->getType());
  				int byteSize = (int)ceil(bitSize/8.0);
  				ConstantInt* instValSize = ConstantInt::get(IntegerType::get(context,32), byteSize);
				
				//Fetch maxTrace number:
				ConstantInt* maxTraceConstInt = ConstantInt::get(IntegerType::get(context,32), maxTrace);
				
				//Load All Arguments
				traceArgs.push_back(IDConstInt);
				traceArgs.push_back(OPCodePtr);
				traceArgs.push_back(instValSize);
				traceArgs.push_back(ptrInst);
				traceArgs.push_back(ofileptrInst);
				traceArgs.push_back(maxTraceConstInt);

				//Create the Function
				CallInst::Create(traceFunc, traceArgs.begin(),traceArgs.end(), "", insertPoint);
			}
		}//Instruction Iteration
	}//BasicBlock Iteration

	return true; //Tell LLVM that the Function was modified
}//RunOnFunction
};//struct InstTrace

//Register the pass with the llvm
char instTrace::ID = 0;
static RegisterPass<instTrace> X("instTrace", "Traces instruction execution through program: -tout <filename> (-debugTrace)", false, false);

}//namespace llfi

