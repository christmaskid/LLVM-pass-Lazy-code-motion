// This project is greatly aided by ChatGPT for LLVM syntax usage. 
// You can see the process here at this link: 
// https://chatgpt.com/share/67585d68-91e4-8003-b8bc-14576da175ad

#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

// Ref.: https://stackoverflow.com/questions/21708209/get-predecessors-for-basicblock-in-llvm
// Get a BB's predecessors
#include "llvm/IR/CFG.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/BitVector.h" // set operation
#include "llvm/ADT/DenseMap.h" // mapping expression to bitvector position
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/PostOrderIterator.h"
#include <map> // DenseMap is hard to use...
#include <string>
#include <queue>


typedef std::pair<llvm::BasicBlock*, llvm::BasicBlock*> BBpair;

using namespace llvm;

namespace {

/* Expression */
struct Expression {
    Instruction* I;
    Value* dest;
    unsigned opcode;
    SmallVector<Value*, 4> operands;

	// https://stackoverflow.com/questions/7204283/how-can-i-use-a-struct-as-key-in-a-stdmap
	bool operator<(const Expression &x) const {
		if (x.dest != dest)
			return (x.dest < dest);
		if (x.opcode != opcode) 
			return (x.opcode < opcode);
		if (x.operands.size() != operands.size()) 
			return (x.operands.size() < operands.size());

		int n = x.operands.size();
		for(int i=0; i<n; i++) {
			if (x.operands[i] != operands[i])
				return (x.operands[i] < operands[i]);
		}
		return false;
	}
	bool operator==(const Expression &x) const {
		if (x.dest != dest)
			return false;
		if (x.opcode != opcode) 
			return false;
		if (x.operands.size() != operands.size()) 
			return false;

		int n = x.operands.size();
		for(int i=0; i<n; i++) {
			if (x.operands[i] != operands[i])
				return false;
		}
		return true;
	}
};

// Ref. https://www.cs.toronto.edu/~pekhimenko/courses/cscd70-w18/docs/Tutorial%202%20-%20Intro%20to%20LLVM%20(Cont).pdf
// User-Use-Usee Design:
// Important: class hierarchies - Value -> User -> Instruction
// An User keeps track of a list of Values that it uses as Operands
Expression InstrToExpr(Instruction* I) {
	// https://llvm.org/doxygen/classllvm_1_1Instruction.html
	// errs() << *I << "\n";
	Expression expr;
	expr.I = I;
	expr.opcode = I->getOpcode();

	if (isa<StoreInst>(*I))
		expr.dest = I->getOperand(1);
	else
		expr.dest = I; // the destination value is the instruction itself

	// https://stackoverflow.com/questions/44946645/traversal-of-llvm-operands
	for (unsigned i = 0; i < I->getNumOperands(); ++i) {
		if (isa<StoreInst>(*I) && i == 1) continue; // Skip the destination operand
		Value* op = I->getOperand(i);
		expr.operands.push_back(op);
	}
	// errs() << expr.dest << " = " << " " << I->getOpcodeName() << " ";
	return expr;
}

bool ignore_instr(Instruction* I) {
	return (isa<AllocaInst>(*I) || I->isTerminator() || isa<CallInst>(*I));
	// terminator: branch, return
}

/* BasicBlockInfo */
struct BasicBlockInfo {
	BasicBlock* B;
	SmallVector<Expression> exprs;

	BitVector Exprs;
	BitVector DEExpr;
	BitVector UEExpr;
	BitVector ExprKill;
	BitVector AvailOut;
	BitVector AvailIn;
	BitVector AntOut;
	BitVector AntIn;

	BitVector LaterIn;
	BitVector Delete;

	bool operator==(const BasicBlockInfo &x) const {
		return (B == x.B);
	}
	bool operator<(const BasicBlockInfo &x) const {
		return (B < x.B);
	}

};

void initBasicBlockInfoBitVector(BasicBlockInfo* b, unsigned n) {
	b->Exprs.resize(n); b->Exprs.reset();
	b->ExprKill.resize(n); b->ExprKill.reset();
	b->DEExpr.resize(n); b->DEExpr.reset();
	b->UEExpr.resize(n); b->UEExpr.reset();

	b->AvailOut.resize(n); b->AvailOut.reset();

	b->AvailIn.resize(n); b->AvailIn.reset();
	b->AntOut.resize(n); b->AntOut.reset();
	b->AntIn.resize(n); b->AntIn.reset();

	b->LaterIn.resize(n); b->LaterIn.reset();
	b->Delete.resize(n); b->Delete.reset();
}

/* EdgeInfo */
struct EdgeInfo {
	BBpair edge;
	BasicBlock* start;
	BasicBlock* end;

	BitVector Earliest;
	BitVector Later;
	BitVector Insert;

	BasicBlock* InsertBlock;

	bool operator==(const EdgeInfo &x) const { return (edge == x.edge); }
	bool operator<(const EdgeInfo &x) const { return (edge < x.edge); }
};
void initEdgeInfoBitVector(EdgeInfo* edgeinfo, BBpair pair, unsigned n) {
	edgeinfo->start = pair.first; edgeinfo->end = pair.second;
	edgeinfo->Earliest.resize(n); edgeinfo->Earliest.reset();
	edgeinfo->Later.resize(n); edgeinfo->Later.reset();
	edgeinfo->Insert.resize(n); edgeinfo->Insert.reset();
	edgeinfo->InsertBlock = NULL;
}

/* Helper functions */

void print_value(Value v) {
	if (v.hasName())
		errs() << v.getName();
	else
		errs() << v;
}

void print_bitvector(BitVector bv) {
	int n = bv.size();
	for(int i=0;i<n;i++) {
		errs() << bv[i];
		if (!((i+1)%10))
			errs() << " ";
	}
	errs() <<" ("<<n<< ")\n";
}

void printBasicBlockInfo(BasicBlockInfo* bbinfo) {
	errs() << "> Block:\t";
	errs() << *(bbinfo->B);
	errs() << "Exprs:\t\t";
	print_bitvector(bbinfo->Exprs);
	errs() << "ExprKill:\t";
	print_bitvector(bbinfo->ExprKill);
	errs() << "DEExpr:\t\t";
	print_bitvector(bbinfo->DEExpr);
	errs() << "UEExpr:\t\t";
	print_bitvector(bbinfo->UEExpr);

	errs() << "AvailOut:\t";
	print_bitvector(bbinfo->AvailOut);
	errs() << "AvailIn:\t";
	print_bitvector(bbinfo->AvailIn);
	errs() << "AntOut:\t\t";
	print_bitvector(bbinfo->AntOut);
	errs() << "AntIn:\t\t";
	print_bitvector(bbinfo->AntIn);

	errs() << "LaterIn:\t";
	print_bitvector(bbinfo->LaterIn);
	errs() << "Delete:\t\t";
	print_bitvector(bbinfo->Delete);
}

void print_edges(SmallVector<BBpair, 8> edges) {
	for(auto &pair : edges) {
		errs() << "(";
		pair.first->printAsOperand(errs(), false);
		errs() << ",";
		pair.second->printAsOperand(errs(), false);
		errs() << ") ";
	}
	errs() << "\n";
}

void printEdgeInfo(EdgeInfo* edgeinfo) {
	errs() << "(";
	edgeinfo->edge.first->printAsOperand(errs(), false);
	errs() << ",";
	edgeinfo->edge.second->printAsOperand(errs(), false);
	errs() << ")\n";

	errs() << "Earliest:\t";
	print_bitvector(edgeinfo->Earliest);
	errs() << "Later:\t\t";
	print_bitvector(edgeinfo->Later);
	errs() << "Insert:\t\t";
	print_bitvector(edgeinfo->Insert);
	errs() << "\n";
}

struct LCMPass : public PassInfoMixin<LCMPass> {

    // Expression related stuff
    std::map<Expression, unsigned> exprmap; // Expression -> # of expression in BitVectors
    SmallVector<Expression, 128> inv_exprmap; // # of expression in BitVectors -> Expression

    // CFG related stuff
    SmallVector<BBpair, 8> edges;
    std::map<BasicBlock*, BasicBlockInfo> blockmap;
    std::map<BBpair, EdgeInfo> edgemap;

    void init(Function &F) {
        exprmap.clear();
        inv_exprmap.clear();
        blockmap.clear();
        edgemap.clear();
    }

    void buildNodes(Function &F) {
        // first pass: build CFG and collect all expressions
        int n = 0;
        for (auto &B : F) {
            BasicBlockInfo bbinfo;
            bbinfo.B = &B;

            for(auto &I : B) {
                // Filter out non store/load or binary operator instructions
                if (ignore_instr(&I))
                    continue;

                Expression expr = InstrToExpr(&I);
                bbinfo.exprs.push_back(expr);
                if (exprmap.find(expr) == exprmap.end()) {
                    exprmap.insert(std::make_pair(expr, n++));
                    inv_exprmap.push_back(expr);
                }
            }
            blockmap.insert(std::make_pair(&B, bbinfo));
        }

        // Second pass: build BitVectors
        for (auto &B : F) {
            initBasicBlockInfoBitVector(&(blockmap[&B]), n);

            for (auto &expr : blockmap[&B].exprs) {
                unsigned idx = exprmap[expr];
                blockmap[&B].Exprs[idx] = 1;
            }
        }
    }

    void buildEdges(Function &F) {
        for(auto &B : F) {
            if (succ_empty(&B))
                continue;

            for (BasicBlock* succ : successors(&B))
                edges.push_back(std::make_pair(&B, succ));
            // add edge (B, succ)
        }

        for (auto &edge : edges) {
            EdgeInfo edgeinfo;
            edgeinfo.edge = edge; // pair of BasicBlock*; do not use &edge !!!
            initEdgeInfoBitVector(&edgeinfo, edge, exprmap.size());
            edgemap[edge] = edgeinfo;
        }
    }


    void buildExprKill(BasicBlockInfo* bbinfo) {
        // For each block
        // Naive O(N^2 * C) method; C = max. operand of an instruction

        // Initialization: empty set
        bbinfo->ExprKill.reset();

        for (auto &pair : exprmap) {
            const Expression* expr = &(pair.first);
            unsigned bit = exprmap[*expr];
            Instruction* I = expr->I;

            for (auto &op : expr->operands) {
                int n = exprmap.size();

                for (int i=0; i<n; i++) {
                    // Pick expression from BitVector
                    if (!bbinfo->Exprs[i])
                        continue;

                    Expression* definition = &(inv_exprmap[i]);
                    if (definition->dest == op) {
                        bbinfo->ExprKill[bit] = 1;
                        break;
                    }
                }
            }
        }
    }

    void buildDEExpr(BasicBlockInfo* bbinfo) {
        // For each block
        // "Not changed after last use"

		// the expression is evaluated AFTER (re)definition within the same block, 
		// and its operands are not redefined afterwards

        bbinfo->DEExpr.reset();
        bbinfo->DEExpr |= bbinfo->Exprs;

        std::map<Value*, unsigned> defined;
        defined.clear();

        BitVector used;
        used.resize(exprmap.size());
        used.reset();

        for (auto it = bbinfo->B->rbegin(); it != bbinfo->B->rend(); ++it) {
            Instruction &I = *it;
            if (ignore_instr(&I))
                continue;
            Expression expr = InstrToExpr(&I);
            unsigned cur_bit = exprmap[expr];

            for (auto &op : expr.operands) {
                if (defined.find(op) != defined.end())  {
                    // operand defined before in this block
                    bbinfo->DEExpr[cur_bit] = 0;
                }
            }
            defined.insert(std::make_pair(expr.dest, 1));
        }
    }

    void buildUEExpr(BasicBlockInfo* bbinfo) {
        // For each block
        // "Not used after last change"

		// the expression is evaluated BEFORE any (re)definition within the same block, 
		// and its operands are not redefined before

        bbinfo->UEExpr.reset();
        bbinfo->UEExpr |= bbinfo->Exprs;

        std::map<Value*, unsigned> defined;
        defined.clear();

        for (auto it = bbinfo->B->begin(); it != bbinfo->B->end(); ++it) {
            Instruction &I = *it;
            if (ignore_instr(&I))
                continue;
            Expression expr = InstrToExpr(&I);
            unsigned cur_bit = exprmap[expr];

            for (auto &op : expr.operands) {
                if (defined.find(op) != defined.end()) 
                    // operand defined afterwards in this block
                    bbinfo->UEExpr[cur_bit] = 0;
            }
            defined.insert(std::make_pair(expr.dest, 1));
        }
    }

    void buildAvailExpr(Function &F) {
        // Forward flow
        // AvailOut = DEExpr + (AvailIn - ExprKill)
        // AvailIn(n) = INTERSECT(AvailOut(m)) for m in preds(n)

        std::queue<BasicBlock*> q;
        std::map<BasicBlock*, bool> visited;
        visited.clear();
        for (auto &B : F) {
            visited.insert(std::make_pair(&B, false));
        }

        BasicBlock* entryBlock = &(F.getEntryBlock());
        q.push(entryBlock);
        visited[entryBlock] = true;

        // Init: AvailIn(n_0) = {}, AvailIn(n) = {all} for n != n0
        for (auto &B : F) {
            BasicBlockInfo* bbinfo = &(blockmap[&B]);
            bbinfo->AvailIn.reset();
            bbinfo->AvailIn.flip();
            bbinfo->AvailOut.reset();
            bbinfo->AvailOut.flip();
        }
        blockmap[entryBlock].AvailIn.reset();

        int changed = 0;
        int n = exprmap.size();
        // worklist: push in successors every time -> guarantee that each block will
        // be traversed after each of predecessor at least once
        while(!q.empty()) {
            BasicBlock* p = q.front();
            q.pop();
            visited[p] = false;
            changed = 0;

            BitVector old(blockmap[p].AvailOut);

            // blockmap[p].AvailOut |= (blockmap[p].DEExpr | (blockmap[p].AvailIn & negExprkill));
            blockmap[p].AvailOut.reset();
            blockmap[p].AvailOut |= blockmap[p].DEExpr;
            BitVector negExprkill = blockmap[p].ExprKill;
            negExprkill.flip();
            BitVector tmp = blockmap[p].AvailIn;
            tmp &= negExprkill;
            blockmap[p].AvailOut |= tmp;

            changed |= (old != blockmap[p].AvailOut);

            for(auto succ : successors(p)) {
                BitVector old(blockmap[succ].AvailIn);
                blockmap[succ].AvailIn &= blockmap[p].AvailOut;

                if ((changed | old != blockmap[succ].AvailIn) \
					 && !visited[succ]) {
					q.push(succ);
					visited[succ] = true;
                }
            }
        }
    }

    void buildAnticiExpr(Function &F) {
        // Backward flow
        // AntIn = UEExpr + (AntOut - ExprKill)
        // AntOut(n) = INTERSECT(AntIn(m)) for m in succs(n), for n != n_f]

        std::queue<BasicBlock*> q;
        std::map<BasicBlock*, bool> visited;
        visited.clear();
        for (auto &B : F) {
            visited.insert(std::make_pair(&B, false));
        }

        SmallVector<BasicBlock*, 4> leafNodes;
        for (auto &B : F) {
            if (succ_empty(&B)) {
                q.push(&B);
                leafNodes.push_back(&B);
                visited[&B] = true;
            }
        }

        // Init: AntOut(n_f) = {}, AntOut(n) = {all} for n != n_f
        for (auto &B : F) {
            BasicBlockInfo* bbinfo = &(blockmap[&B]);
            bbinfo->AntOut.reset();
            bbinfo->AntOut.flip();
            bbinfo->AntIn.reset();
            bbinfo->AntIn.flip();
        }
        for (auto it : leafNodes) {
            BasicBlockInfo* bbinfo = &(blockmap[it]);
            bbinfo->AntOut.reset();
        }

        int changed = 0;
        int n = exprmap.size();
        // worklist: push in predecessors every time -> guarantee that each block will
        // be traversed after each of successors at least once
        while(!q.empty()) {
            BasicBlock* p = q.front();
            q.pop();
            visited[p] = false;
            changed = 0;

            BitVector old(blockmap[p].AntIn);

            blockmap[p].AntIn.reset();
            // blockmap[p].AntIn |= (blockmap[p].UEExpr | (blockmap[p].AntOut & negExprkill));
            blockmap[p].AntIn |= blockmap[p].UEExpr;
            BitVector negExprkill = blockmap[p].ExprKill;
            negExprkill.flip();
            BitVector tmp(blockmap[p].AntOut);
            tmp &= negExprkill;
            blockmap[p].AntIn |= tmp;

            changed |= (old != blockmap[p].AntIn);

			if (pred_empty(p))
				continue;
            
            for(BasicBlock *pred : predecessors(p)) {
                BitVector old(blockmap[pred].AntOut);
                blockmap[pred].AntOut &= blockmap[p].AntIn;
                
                if ((changed | (old != blockmap[pred].AntOut)) \
					 && !visited[pred]) {
					q.push(pred);
					visited[pred] = true;
                }
            }
        }
    }

    void buildEarliest(Function &F) {
        // For each edge
        // Earliest(i, j) = (AntIn(j) - AvailOut(i)) & (ExprKill(i) + ~AntOut(i))
        for (auto &edge : edges) {
            EdgeInfo *edgeinfo = &(edgemap[edge]);
            BasicBlock* i = edgeinfo->start;
            BasicBlock* j = edgeinfo->end;
            
            BitVector tmp1(blockmap[j].AntIn);
            BitVector negAvailOut_i(blockmap[i].AvailOut);
            negAvailOut_i.flip();
            tmp1 &= negAvailOut_i;

            BitVector tmp2(blockmap[i].ExprKill);
            BitVector negAntOut_i(blockmap[i].AntOut);
            negAntOut_i.flip();
            tmp2 |= negAntOut_i;
            
            // print_bitvector(tmp1);
            // print_bitvector(tmp2);
            // printEdgeInfo(edgeinfo);

            edgeinfo->Earliest.reset();
            edgeinfo->Earliest |= tmp1;
            edgeinfo->Earliest &= tmp2; 
        }
    }

    void buildLater(Function &F) {
        // Forward flow
        // LaterIn(j) = INTERSECT(Later(i, j)) for i in pred(j), j != n_0
        // Later(i, j) = Earliest(i, j) + (LaterIn(i) & UEExpr(i))

        std::queue<BasicBlock*> q;
        std::map<BasicBlock*, bool> visited;
        visited.clear();
        for (auto &B : F) {
            visited.insert(std::make_pair(&B, false));
        }

        BasicBlock* entryBlock = &(F.getEntryBlock());
        q.push(entryBlock);
        visited[entryBlock] = true;

        // Init: LaterIn(n_0) = {}, LaterIn(n) = {all} for n != n_0
        for (auto &B : F) {
            BasicBlockInfo* bbinfo = &(blockmap[&B]);
            bbinfo->LaterIn.reset();
            bbinfo->LaterIn.flip();
        }
        blockmap[entryBlock].LaterIn.reset();

        int changed = 0;
        int n = exprmap.size();
        // worklist: push in successors every time -> guarantee that each block will
        // be traversed after each of predecessor at least once
        while(!q.empty()) {
            BasicBlock* p = q.front();
            q.pop();
            visited[p] = false;
            changed = 0;

            // Later(p, succ) = Earliest(p, succ) + (LaterIn(p) & UEExpr(p))
            for(BasicBlock* succ : successors(p)) {
                EdgeInfo* edgeinfo = &(edgemap[std::make_pair(p, succ)]);
                BitVector old(edgeinfo->Later);

                edgeinfo->Later.reset();
                edgeinfo->Later |= blockmap[p].LaterIn;
                edgeinfo->Later &= blockmap[p].UEExpr;
                edgeinfo->Later |= edgeinfo->Earliest;
                // edgeinfo->Later |= edgeinfo->Earliest;

                // BitVector tmp(blockmap[p].LaterIn);
                // tmp &= blockmap[p].UEExpr;
                // edgeinfo->Later |= tmp;
                
                changed |= (old != edgeinfo->Later);
            }

            // LaterIn(j) = INTERSECT(Later(i, j)) for i in pred(j), j != n_0

            for(BasicBlock *succ : successors(p)) {
                BitVector old(blockmap[succ].LaterIn);
                blockmap[succ].LaterIn &= edgemap[std::make_pair(p, succ)].Later;

                if (changed | (old != blockmap[succ].LaterIn)) {
                    if (!visited[succ]) {
                        q.push(succ);
                        visited[succ] = true;
                    }
                }
            }
        }
    }

    void buildInsertDelete(Function &F) {
        // For each block / edge
        // Insert(i, j) = Later(i, j) - LaterIn(j)

        for (auto &edge : edges) {
            EdgeInfo *edgeinfo = &(edgemap[edge]);
            edgeinfo->Insert.reset();
            edgeinfo->Insert |= edgeinfo->Later;

            BitVector negLaterIn_j(blockmap[edgeinfo->end].LaterIn);
            negLaterIn_j.flip();
            edgeinfo->Insert &= negLaterIn_j;
        }

        // Delete(i) = UEExpr(i) - LaterIn(i), i != n_0
        //             {}, i = n_0

        for (auto &B : F) {
            BasicBlockInfo *bbinfo = &(blockmap[&B]);
            bbinfo->Delete.reset();
            if (&B != &(F.getEntryBlock())) {
                bbinfo->Delete |= bbinfo->UEExpr;
                BitVector negLaterIn(bbinfo->LaterIn);
                negLaterIn.flip();
                bbinfo->Delete &= negLaterIn;
            }
        }

    }

    int codeMotion(Function &F) {
        int n = exprmap.size();
        
        // Insert
        int newBlockCnt = 0;
        for (auto &edge : edges) {
            EdgeInfo* edgeinfo = &(edgemap[edge]);
            if (edgeinfo->Insert.empty())
                continue;

            IRBuilder<> *builder = nullptr;

            for (int idx = 0; idx < n; idx++) {
            
                if (!edgeinfo->Insert[idx])
                    continue;

                BasicBlock* i = edgeinfo->start;
                BasicBlock* j = edgeinfo->end;
                Expression* expr = &(inv_exprmap[idx]);
                Instruction* cloned_instr = expr->I->clone();

                if (i->getSingleSuccessor()) {
                    // node i has only one successor: insert at the end of i
                    builder = new IRBuilder<>(i->getContext());
                    builder->SetInsertPoint(i->getTerminator());
                }
                else if (j->getSinglePredecessor()) {
                    // node j has only one predecessor: insert at the start of j
                    builder = new IRBuilder<>(j->getContext());
                    builder->SetInsertPoint(j->getFirstInsertionPt());
                }
                else {
                    // split edge (i, j)
                    // Don't put the new block into the function yet!!!
                    // Ref.: https://stackoverflow.com/questions/61181446/how-to-build-a-empty-basicblock-and-insert-a-instruction-in-the-front
                    LLVMContext &context = i->getContext();
                    builder = new IRBuilder<>(i->getContext());
                    edgeinfo->InsertBlock = BasicBlock::Create(i->getContext(), "newblock"+std::to_string(newBlockCnt++));
                    builder->SetInsertPoint(edgeinfo->InsertBlock->getFirstInsertionPt());
                }
                
                builder->Insert(cloned_instr);
                
                // Ref. Cornell course, LLVM for grad: addtomul example
                // Everywhere the old instruction was used as an operand,
                // use our new instruction instead.
                for (auto& U : expr->I->uses()) {
                        User* user = U.getUser(); // A User is anything with operands
                        user->setOperand(U.getOperandNo(), cloned_instr);
                }
            }
			delete builder;
        }

		for(auto &B : F) {
			BasicBlockInfo* bbinfo = &(blockmap[&B]);
			for(int idx=0; idx<n; idx++) {
				if (!bbinfo->Delete[idx]) continue;
				Instruction* I = inv_exprmap[idx].I;
				// remove the expression
				I->eraseFromParent();
			}
		}

		// Put all newly created blocks into the function
		// Ref.: https://stackoverflow.com/questions/61181446/how-to-build-a-empty-basicblock-and-insert-a-instruction-in-the-front
		for(auto &edge : edges) {
			EdgeInfo* edgeinfo = &(edgemap[edge]);
			BasicBlock* i = edgeinfo->start;
			BasicBlock* j = edgeinfo->end;

			if (!(edgeinfo->InsertBlock))
				continue;
			// Set function parent and upstream node
			BasicBlock* newBlock = edgeinfo->InsertBlock;
			newBlock->insertInto(&F, i);

			// Set descendant
			Instruction *terminator = i->getTerminator();
			int n = terminator->getNumSuccessors();
			for(unsigned edge_num=0; edge_num<n; ++edge_num) {
				if (terminator->getSuccessor(edge_num)==j) {
					terminator->setSuccessor(edge_num, newBlock);
					break;
				}
			}
			// Add terminator instruction (BranchInst) into the end ot fhe new block
			IRBuilder<> builder(newBlock);
			builder.CreateBr(j);
		}


        return 1;
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        int changed = 0;
        for (auto &F : M) {
        	// errs() << F.getName() << " " << F.size() << "\n";
        	// errs() << F << "\n";

			if (F.isDeclaration()) // exclude external functions
				continue;
			if (F.empty()) {
				// errs() << "No entry block found.\n";
				continue;
			}
            
            init(F);
            buildNodes(F);
            buildEdges(F);

            for (auto &B : F) {
                BasicBlockInfo* bbinfo = &(blockmap[&B]);
                buildExprKill(bbinfo);
                buildDEExpr(bbinfo);
                buildUEExpr(bbinfo);
            }
            buildAvailExpr(F);
            buildAnticiExpr(F);

            buildEarliest(F);
            buildLater(F);
            buildInsertDelete(F);

            /* Print out for debug*/
            for (auto &B : F) {
                printBasicBlockInfo(&blockmap[&B]);
            }
            for (auto &edge : edges) {
                printEdgeInfo(&edgemap[edge]);
            }

            int this_changed = codeMotion(F);
            changed |= this_changed;

            // errs() << F << "\n";

        }
		
		return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "LCM pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(LCMPass());
                });
        }
    };
}
