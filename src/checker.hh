
#ifndef __CHECKER_HH_
#define __CHECKER_HH_

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint> // uintxx

#include "evconf.hh"
#include "vclock.hh"
#include "verbosity.h"
#include "../rt/rt.h"

#include "stid/action.hh"
#include "stid/action_stream.hh"

namespace stid {

#if 0
/* @TODO: Add a CheckerConfig
  that controls the checks
  when building the partial order
*/
struct CheckerConfig
{
   int num_ths;
   int num_lock;
}; 

// Statistics generated by the checker
//  False alarms and errors
class CheckerStats;

class Checker 
{
public :
   Checker ();
   ~Checker ();

   void run ();
   // Conf * get_po ();
   
private :
   // configuration for the checker
   CheckerConfig  conf;

   // partial order generated
   Conf *po;

   // map from lock addr to index (hash)
   //std::unordered_map <addr, int> locks_idx;
 
};
#endif

} // namespace
#endif
