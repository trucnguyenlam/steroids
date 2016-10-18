
#include <unistd.h>
#include "pthread.h"

// this file will be #include'd in the main.c file

#define ALIGN16(i) (((uint64_t) (i)) & 0xf ? (((uint64_t) (i)) + 16) & -16ul : (uint64_t) (i))

int _rt_errno = 0;
FILE *stdin = 0;
FILE *stdout = 0;
FILE *stderr = 0;

static uint64_t __malloc_ptr;

void _rt_libc_init ()
{
   stdin = rt->stdin;
   stdout = rt->stdout;
   stderr = rt->stderr;

   _rt_mm_init ();
   _rt_thread_init ();
}

void _rt_libc_term ()
{
   _rt_thread_term ();
   _rt_mm_term ();
}

void _rt_mm_init ()
{
   printf ("stid: rt: mm: initializing memory manager\n");
   __malloc_ptr = (uint64_t) rt->heap.begin;
}

void _rt_mm_term ()
{
   printf ("stid: rt: mm: terminating memory manager\n");
}

void *_rt_calloc  (size_t n, size_t size)
{
   return _rt_malloc (n * size);
}

void *_rt_malloc  (size_t size)
{
   void *ptr;
   ptr = (void*) __malloc_ptr;
   __malloc_ptr = ALIGN16 (__malloc_ptr + size);
   //printf ("stid: rt: malloc: ret %p size %zu\n", ptr, size);
   TRACE2 (RT_MALLOC, ptr, size);
   memset (ptr, 0, size); // necessary for repeatable execution!!!!
   return ptr;
}

void _rt_free (void *ptr)
{
   TRACE1 (RT_FREE, ptr);
   //printf ("stid: rt: free: ptr %p", ptr);
   (void) ptr; 
}

void *_rt_realloc (void *ptr, size_t size)
{
   void *newptr;

   if (! ptr) return _rt_malloc (size);
   if (size == 0)
   {
      _rt_free (ptr);
      return 0;
   }
   newptr = _rt_malloc (size);
   //printf ("stid: rt: realloc: ptr %p newptr %p size %zu", ptr, newptr, size);
   if (!newptr) return 0;

   // this is strictly not safe, as size could be larger than the size of the
   // area pointed by ptr and then the memory areas could overlap
   memcpy (newptr, ptr, size);
   _rt_free (ptr);
   return newptr;
}

void _rt_exit (int status)
{
   fflush (stdout);
   fflush (stderr);

   // EXIT event for the calling thread (which will be main at this point)
   TRACE0 (RT_THEXIT);

   // return control to the host
   _rt_cend (status);
}

unsigned int _rt_sleep (unsigned int sec)
{
   //struct rt_tcb *me = __rt_thst.current;
   unsigned ret;

   //_rt_thread_protocol_yield (me);
   ret = sleep (sec);
   //_rt_thread_protocol_wait (me);
   return ret;
}

int _rt_usleep (useconds_t us)
{
   //struct rt_tcb *me = __rt_thst.current;
   unsigned ret;

   //_rt_thread_protocol_yield (me);
   ret = usleep (us);
   //_rt_thread_protocol_wait (me);
   return ret;
}

int *_rt___errno_location ()
{
   _rt_errno = *__errno_location (); // in glibc !!
   printf ("stid: rt: errno_location: called !!!!!!!!!!!!!!\n");
   return &_rt_errno;
}

