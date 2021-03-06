
#ifndef __INSTRUMENTER_HH_
#define __INSTRUMENTER_HH_

#include <unordered_map>

#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstIterator.h"

#include "stid/tlsemit.hh"
#include "verbosity.h"

namespace stid {

class Instrumenter : public llvm::InstVisitor<Instrumenter>
{
public:

   bool instrument (llvm::Module &m, Tlsoffsetmap &tlsoffsetmap);

   void visitLoadInst (llvm::LoadInst &i);
   void visitStoreInst (llvm::StoreInst &i);
   void visitAllocaInst (llvm::AllocaInst &i);
   void visitReturnInst (llvm::ReturnInst &i);
   void visitCallInst (llvm::CallInst &i);

   bool do_external_load (llvm::LoadInst &i);
   bool do_external_store (llvm::StoreInst &i);
   void do_tls_variables (Tlsoffsetmap &map);

   // visit(M) in parent class should never be called ;)
   void visitModule (llvm::Module &m) { ASSERT (0); }

private:
   llvm::LLVMContext *ctx;
   llvm::Module      *m;
   //llvm::DataLayout *layout;

   llvm::Function *load_pre;
   llvm::Function *load_post;
   llvm::Function *store_pre;
   llvm::Function *store_post;

   llvm::Function *allo;
   llvm::Function *call;
   llvm::Function *ret;

   llvm::Function *tls_get_var_addr;

   int next_call_id;
   std::map<llvm::Function*,int> funids;
   std::map<llvm::Function*,llvm::Function*> substmap_funs;
   std::map<llvm::Value*,llvm::Function*> substmap_loads;
   std::map<llvm::Value*,llvm::Function*> substmap_stores;
   size_t count;

   bool find_rt ();
   bool is_rt_fun (llvm::Function *f);
   void reset (llvm::Module &m);
   void init_maps ();
   void replace_tls_var (llvm::GlobalVariable *g, unsigned offset);
};

} // namespace
#endif
