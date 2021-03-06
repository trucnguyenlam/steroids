
#include <pthread.h> // in /usr/include
#include "pthread.h" // in .
#include "thread-sched.h"
#include "tls.h"

static struct {
   /// global context switch mutex, only 1 thread executes at a time
   pthread_mutex_t m;
   /// the state of every thread
   struct rt_tcb tcbs[RT_MAX_THREADS];
   /// the index of the next available thread TCB, for thread creation
   unsigned next;
   /// only one thread executes at a time, pointer stored here
   struct rt_tcb *current;
   /// Pointer to the base of the thread-local storage of the
   /// currently-executing thread
   void *tlsptr;
   /// number of threads currently alive
   int num_ths_alive;

   /// the list of all threads currently blocked in the sleep set
   struct {
      struct rt_tcb* tab[RT_MAX_THREADS];
      unsigned size;
   } sleepset;

   /// mapping tids in the replay to threads in the runtime
   struct rt_tcb *tidmap[RT_MAX_THREADS];

   /// Initial values for the TLS of each created thread
   struct rt_tls tlsinit;
   /// the the tid of the next thread that will be created in the replay
   unsigned next_tid;
} __state;

#define TID(t) ((unsigned) ((t) - __state.tcbs))

int __rt_thread_sched_update (struct rt_tcb *t)
{
   int ret;

   // the thread is not availale if it is not alive
   if (! t->flags.alive) return 0;

   // if alive, we check its state
   switch (t->state)
   {
   case SCHED_RUNNABLE :
      //DEBUG ("stid: rt: threading: sched: update: t%d is RUNNABLE", TID (t));
      return 1;

   case SCHED_WAIT_MUTEX :
      ASSERT (t->wait_mutex);
      // check if the mutex is now available
      ret = pthread_mutex_trylock (t->wait_mutex);
      // if it is
      if (ret == 0)
      {
         // unlock the mutex, as we just locked it!
         ret = pthread_mutex_unlock (t->wait_mutex);
         // FIXME -- issue a warning into the stream here
         if (ret != 0) ALERT ("error: unlock: %s\n", strerror (ret));
         // wake up threads sleeping on this mutex
         __rt_thread_sleepset_awake (t->wait_mutex);
         // change the thread state
         t->state = SCHED_RUNNABLE;
         //DEBUG ("stid: rt: threading: sched: update: t%d, " "WAIT_MUTEX(%p) -> RUNNABLE", TID (t), t->wait_mutex);
         return 1;
      }
      ASSERT (ret == EBUSY); // could also be other errors ...
      return 0;

   case SCHED_WAIT_JOIN :
      ASSERT (t->wait_join);
      // change state if the joined thread is now dead
      if (! t->wait_join->flags.alive)
      {
         t->state = SCHED_RUNNABLE;
         //DEBUG ("stid: rt: threading: sched: update: t%d, WAIT_JOIN(t%d) -> RUNNABLE", TID (t), TID (t->wait_join));
         return 1;
      }
      return 0;

   case SCHED_WAIT_SS :
      // the only way to wake him up is when some other thread locks on
      // t->wait_mutex, and that will turn this thread from SCHED_WAIT_SS into
      // SCHED_WAIT_MUTEX
      return 0;

   case SCHED_WAIT_ALLEXIT :
      // only the main thread can be in this state, it gets here only if it
      // executes pthread_exit() and there is at least one other thread alive;
      // it becomes runnable only when all other threads are dead
      ASSERT (TID(t) == 0);
      if (__state.num_ths_alive == 1)
      {
         t->state = SCHED_RUNNABLE;
         //DEBUG ("stid: rt: threading: sched: update: t%d, WAIT_ALLEXIT -> RUNNABLE", TID (t));
         return 1;
      }
      return 0;
   }
}

struct rt_tcb* __rt_thread_sched_find_any ()
{

   /// In this function we scan, round-robbin all threads alive, and return the
   /// first that is runnable. If none is ready to run, we have detected a
   /// deadlock.

   unsigned i, j;
   int ret;
   struct rt_tcb *t;

   // choose a thread to start the round of checks for executability
   //i = TID (__state.current);
   i = 0;

   // we scan all tcbs to search for one that is ready to execute, stating from
   // thread i
   for (j = 0; j < __state.next; j++)
   {
      if (__rt_thread_sched_update (__state.tcbs + i))
         return __state.tcbs + i;
      i = (i + 1) % __state.next;
   }

   // no thread is runnable, if we have sleepset blocked theads, we release
   // all of them and run the first, at this point this execution is an SSB
   if (__state.sleepset.size)
   {
      //DEBUG ("stid: rt: threading: sched: find-any: no runnable thread, %u in sleepset (SSB!!!)", __state.sleepset.size);

      // release all
      for (i = 0; i < __state.sleepset.size; i++)
      {
         t = __state.sleepset.tab[i];
         ASSERT (t);
         ASSERT (t->state == SCHED_WAIT_SS);
         t->state = SCHED_WAIT_MUTEX;
         //DEBUG ("stid: rt: threading: sched: find-any: t%d, WAIT_SS(%p) -> WAIT_MUTEX(%p)", TID (t), t->wait_mutex, t->wait_mutex);
      }
      __state.sleepset.size = 0;
      // schedule the first
      t = __state.sleepset.tab[0];
      ret = __rt_thread_sched_update (t);
      ASSERT (ret);
      return t;
   }

   ALERT ("deadlock found: no thread is runnable, empty sleep set");
   return 0;
}

struct rt_tcb* __rt_thread_sched_find_next ()
{
   /// This function chooses the next thread to run.

   int ret;
   struct rt_tcb *t;

   // if we are in free mode, any thread works
   if (rt->replay.current->tid == -1)
      return __rt_thread_sched_find_any ();

   // if we are replaying and we can play again the current thread, the
   // scheduler should agree with that
   if (rt->replay.current->count >= 1)
   {
      ret = __rt_thread_sched_update (__state.current);
      ASSERT (ret); (void) ret;
      return __state.current;
   }

   // otherwise we are replaying but we ran out of events to replay in this
   // thread, so we need to context switch to another thread of the replay or
   // switch into free mode; the replay should never ask us to play a THCREAT
   // now
   ASSERT (rt->replay.current->count == 0);
   rt->replay.current++;
   if (rt->replay.current->tid == -1)
   {
      INFO ("stid: rt: threading: sched: find-next: transition to FREEMODE");
      // if we switch to free mode, we initialize the structure __state.sleepset
      // and pick any thread to run
      __rt_thread_sleepset_init ();
      return __rt_thread_sched_find_any ();
   }

   // otherwise we have to replay the thread that the replay asks us to replay;
   // we use the tidmap to map the tids in the backend to tids in this runtime
   ASSERT (rt->replay.current->tid >= 0 && (unsigned) rt->replay.current->tid < __state.next_tid);
   ASSERT (rt->replay.current->count >= 1);
   t = __state.tidmap[rt->replay.current->tid];
   ASSERT (t);
   ASSERT (t->flags.alive);
   INFO ("stid: rt: threading: sched: find-next: "
         "replaying context switch to t%d (r%u), and then %d events",
         TID(t), rt->replay.current->tid, rt->replay.current->count);
   // the scheduler should to agree to run this thread now
   ret = __rt_thread_sched_update (t);
   ASSERT (ret);
   return t;
}

void __rt_thread_sleepset_init ()
{
   unsigned i, j;
   struct rt_tcb *t;

   ASSERT (__state.sleepset.size == 0);
   j = 0;
   for (i = 0; i < RT_MAX_THREADS; i++)
   {
      if (rt->replay.sleepset[i])
      {
         // if the entry at index i in replay.sleepset is a non-null pointer to
         // a mutex, then thread __state.tidmap[i] needs to be right now waiting
         // for that mutex; we make the thread sleep and push it into the set of
         // sleeping threads, function __rt_thread_sleepset_awake will look for
         // it later when another thread locks on the mutex
         t = __state.tidmap[i];
         ASSERT (t->state == SCHED_WAIT_MUTEX);
         ASSERT (t->wait_mutex == rt->replay.sleepset[i]);
         //DEBUG ("stid: rt: threading: sched: sleepset-init: t%d, " "WAIT_MUTEX -> WAIT_SS(%p)", TID (t), t->wait_mutex);
         t->state = SCHED_WAIT_SS;
         __state.sleepset.tab[j] = t;
         j++;
      }
   }
   __state.sleepset.size = j;
}

void __rt_thread_sleepset_awake (pthread_mutex_t *m)
{
   /// We extract from the sleepset all threads that are sleeping until someone
   /// else locks on mutex m.

   unsigned i;
   struct rt_tcb *t;

   for (i = 0; i < __state.sleepset.size; i++)
   {
      t = __state.sleepset.tab[i];
      ASSERT (t);
      ASSERT (t->state == SCHED_WAIT_SS);
      if (t->wait_mutex == m)
      {
         // let the thread compete for the mutex
         t->state = SCHED_WAIT_MUTEX;
         //DEBUG ("stid: rt: threading: sched: sleepset-awake: t%d, " "WAIT_SS -> WAIT_MUTEX", TID (t));
         // extract thread i from the sleep set
         if (i != __state.sleepset.size - 1)
            __state.sleepset.tab[i] =
                  __state.sleepset.tab[__state.sleepset.size-1];
         i--;
         __state.sleepset.size--;
      }
   }
}

void  __rt_thread_init (void)
{
   int ret;
#ifdef CONFIG_DEBUG
   int i;
#endif

   // whoever calls this function becomes the main thread (tid = 0)
   __state.next = 1;
   __state.current = __state.tcbs;
   __state.num_ths_alive = 1;
   __state.sleepset.size = 0;
   __state.tcbs[0].flags.alive = 1;
   __state.tcbs[0].flags.needsjoin = 0;
   __state.tcbs[0].flags.detached = 0;
   __state.tcbs[0].stackaddr = 0;
   __state.tcbs[0].stacksize = 0;

   // initializes the __state.tlsinit
   __rt_tls_init ();
   // initializes the main thread's TLB
   __rt_tls_thread_start (__state.tcbs + 0);
   __state.tlsptr = __state.tcbs[0].tls.block;

   // clear the tidmap, this will allow to catch defects in the replay sequence
   // in __rt_thread_sched_find_next (precisely, a ctxsw to an non-existent
   // thread)
#ifdef CONFIG_DEBUG
   for (i = 0; i < RT_MAX_THREADS; i++) __state.tidmap[i] = 0;
#endif
   // tid 0 is always the main thread
   __state.tidmap[0] = __state.tcbs + 0;
   __state.next_tid = 1;

   // initialize the cs mutex
   ret = pthread_mutex_init (&__state.m, 0);
   if (ret)
   {
      // FIXME -- issue a warning into the stream here
      ALERT ("error: initializing cs mutex: %s\n", strerror (ret));
   }

   // initialize main's conditional variable
   ret = pthread_cond_init (&__state.tcbs[0].cond, 0);
   if (ret)
   {
      ALERT ("t0: error: initializing t0 conditional variable: %s; ignoring",
            strerror (ret));
   }

   __rt_thread_protocol_wait_first ();
}

void  __rt_thread_term (void)
{
   int ret;

   // only the main thread should be calling this, stop the model checker
   // otherwise
   if (TID (__state.current) != 0)
   {
      ALERT ("error: thread %d called exit() but this runtime "
            "only supports calls to exit() from the main thread",
            TID (__state.current));
      fflush (stdout);
      fflush (stderr);
      exit (1);
   }

   // and it should call it only when the main thread is the only one alive
   if  (__state.num_ths_alive > 1)
   {
      ALERT ("error: main thread called exit() but %d other threads are still alive;"
            " this is not currently supported by the runtime",
            __state.num_ths_alive - 1);
      fflush (stdout);
      fflush (stderr);
      exit (1);
   }

   // copy information to the trace structure
   rt->trace.num_ths = __state.next;
   //rt->trace.num_mutex = 0;

   // release the cs mutex (no other thread can acquire it)
   ret = pthread_mutex_unlock (&__state.m);
   if (ret)
   {
      ALERT ("t%d: errors while unlocking cs mutex: %s; ignoring",
            TID(__state.current), strerror (ret));
   }

   // destroy the cs mutex
   ret = pthread_mutex_destroy (&__state.m);
   if (ret)
   {
      ALERT ("t%d: errors while destroying cs mutex: %s; ignoring",
            TID(__state.current), strerror (ret));
   }
}

void *__rt_thread_start (void *arg)
{
   struct rt_tcb *t = (struct rt_tcb *) arg;
   void *ret;

   //DEBUG ("stid: rt: threading: start: t%d: starting!", TID (t));

   // start protocol: we wait to get our context switch
   __rt_thread_protocol_wait (t);

   // generate the THSTART blue action
   REPLAY_CONSUME_ONE ();
   rt->trace.num_blue[TID(t)]++;

   // run the function provided, with the good argument
   ret = t->start (t->arg);

   // exit the thread through one unique place in the code
   _rt_pthread_exit (ret);

   return ret; // unreachable
}

void __rt_thread_protocol_wait_first ()
{
   int ret;

   // lock on the global mutex
   ret = pthread_mutex_lock (&__state.m);
   if (ret != 0)
   {
      ALERT ("error: t0: acquiring internal mutex: %s", strerror (ret));
      __rt_cend (255);
   }
   //DEBUG ("stid: rt: threading: proto: t0: acquired cs lock");

   // consume the THSTART event of main
   REPLAY_CONSUME_ONE ();

   // increase my number of blue events
   rt->trace.num_blue[0]++;
}

void __rt_thread_protocol_wait (struct rt_tcb *me)
{
   /// This function blocks the calling thread (which is "me") until the
   /// scheduler determines that the thread needs to execute again, unless the
   /// thread is already dead (not alive), case in which it returns immediately
   /// (this will only happen when called from pthread_exit())

   int ret;

   // return immediately if the thread is dead
   if (! me->flags.alive) return;

   // lock on the global mutex
   ret = pthread_mutex_lock (&__state.m);
   if (ret != 0) goto err_panic;
   DEBUG ("stid: rt: threading: proto: t%d: acquired cs lock", TID (me));

   // if I am not the next thread to execute, then this was a spurious lock and
   // I wait until it's my turn
   if (__state.current != me)
   {
      DEBUG ("stid: rt: threading: proto: t%d: "
            "spurious acquisition (next is t%d), waiting",
            TID (me), TID (__state.current));
      ret = pthread_cond_wait (&me->cond, &__state.m);
      if (ret != 0) goto err_wait;
      DEBUG ("stid: rt: threading: proto: t%d: "
            "acquired cs lock after cond_wait", TID (me));
   }

   // now I should be the next thread to run
   ASSERT (__state.current == me);
   return;

err_wait :
   ALERT ("error: t%d: waiting on conditional variable: %s",
         TID (me), strerror (ret));
   __rt_cend (255);
err_panic :
   ALERT ("error: t%d: acquiring cs mutex: %s", TID (me), strerror (ret));
   __rt_cend (255);
}

void __rt_thread_protocol_yield ()
{
   int ret;
   struct rt_tcb *t, *me;

   // record who I am
   me = __state.current;

   // choose the next thread to execute
   t = __rt_thread_sched_find_next ();
   if (!t)
   {
      // FIXME -- improve this to kill all threads
      __rt_cend (255);
   }

   // if the next thread is still me, we just return
   if (t == me) return;

   // otherwise we need to context switch to thread t
   TRACE3 (RT_THCTXSW, TID (t));
   __state.current = t;
   __state.tlsptr = t->tls.block;
   DEBUG ("stid: rt: threading: proto: t%d: signaling to t%d", TID (me), TID(t));
   ret = pthread_cond_signal (&t->cond);
   if (ret)
   {
      ALERT ("error: t%d: signal for t%d: %s", TID (me), TID(t), strerror (ret));
      __rt_cend (255);
   }

   // unlock on the cs mutex
   //DEBUG ("stid: rt: threading: proto: t%d: released cs lock", TID (me));
   ret = pthread_mutex_unlock (&__state.m);
   if (ret)
   {
      ALERT ("error: t%d: releasing internal mutex: %s", TID (me), strerror (ret));
      __rt_cend (255);
   }

   // reacquire again the global mutex when it's our turn to execute again
   __rt_thread_protocol_wait (me);
}
