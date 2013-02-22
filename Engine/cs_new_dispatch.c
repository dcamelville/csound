/*
** CSound-parallel-dispatch.c
** 
**    Copyright (C)  Martin Brain (mjb@cs.bath.ac.uk) 04/08/12
**
**    Realisation in code John ffitch Jan 2013
**
    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA


** Fast system for managing task dependencies and dispatching to threads.
** 
** Has a DAG of tasks and has to assign them to worker threads while respecting
** dependency order.
**
** OPT marks code relevant to particular optimisations (listed below the code).
** INV marks invariants
** NOTE marks notes
*/

#ifdef NEW_DAG

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "csoundCore.h"
#include "cs_par_base.h"
#include "cs_par_orc_semantics.h"
#include "csGblMtx.h"

/* Used as an error value */
typedef int taskID;
#define INVALID (-1)
#define WAIT    (-2)

/* Each task has a status */
enum state { INACTIVE = 5,         /* No task */
             WAITING = 4,          /* Dependencies have not been finished */
	     AVAILABLE = 3,        /* Dependencies met, ready to be run */
	     INPROGRESS = 2,       /* Has been started */
	     DONE = 1 };           /* Has been completed */

/* Sets of prerequiste tasks for each task */
typedef struct _watchList {
  taskID id;
  struct _watchList *next;
} watchList;

/* Array of states of each task -- need to move to CSOUND structure */
static enum state *task_status = NULL;          /* OPT : Structure lay out */
static watchList **task_watch = NULL; 
//static INSDS **task_map = NULL; 

/* INV : Read by multiple threads, updated by only one */
/* Thus use atomic read and write */

static char ** task_dep;                        /* OPT : Structure lay out */
static watchList * wlmm;

#define INIT_SIZE (100)
static int task_max_size;
static int dag_dispatched;
static int dag_done;

static void dag_print_state(CSOUND *csound)
{
    int i;
    printf("*** %d tasks\n", csound->dag_num_active);
    for (i=0; i<csound->dag_num_active; i++) {
      printf("%d: ", i);
      switch (task_status[i]) {
      case DONE:
        printf("status=DONE\n"); break;
      case INPROGRESS:
        printf("status=INPROGRESS\n"); break;
      case AVAILABLE:
        printf("status=AVAILABLE\n"); break;
      case WAITING: 
        {
          char *tt = task_dep[i];
          int j;
          printf("status=WAITING for task %d (", wlmm[i].id);
          for (j=0; j<i; j++) if (tt[j]) printf("%d ", j);
          printf(")\n");
        }
        break;
      default:
        printf("status=???\n"); break;
      }
    }
    printf("watches: ");
    for (i=0; i<csound->dag_num_active; i++) {
      watchList *t = task_watch[i];
      printf("\t%d: ", i);
      while (t) {
        printf("%d ", t->id); t = t->next;
      }
      printf("\n");
    }
}
	     
/* For now allocate a fixed maximum number of tasks; FIXME */
void create_dag(CSOUND *csound)
{
    /* Allocate the main task status and watchlists */
    task_status = mcalloc(csound, sizeof(enum state)*(task_max_size=INIT_SIZE));
    task_watch  = mcalloc(csound, sizeof(watchList**)*task_max_size);
    csound->dag_task_map    = mcalloc(csound, sizeof(INSDS*)*task_max_size);
    task_dep    = (char **)mcalloc(csound, sizeof(char*)*task_max_size);
    wlmm        = (watchList *)mcalloc(csound, sizeof(watchList)*task_max_size);
}

static INSTR_SEMANTICS *dag_get_info(CSOUND* csound, int insno)
{
    INSTR_SEMANTICS *current_instr =
      csp_orc_sa_instr_get_by_num(csound, insno);
    if (current_instr == NULL) {
      current_instr =
        csp_orc_sa_instr_get_by_name(csound,
                                     csound->engineState.instrtxtp[insno]->insname);
      if (current_instr == NULL)
        csound->Die(csound,
                    Str("Failed to find semantic information"
                        " for instrument '%i'"),
                    insno);
    }
    return current_instr;
}

static int dag_intersect(CSOUND *csound, struct set_t *current, 
                         struct set_t *later, int cnt)
{
    struct set_t *ans;
    int res = 0;
    struct set_element_t *ele;
    csp_set_intersection(csound, current, later, &ans);
    res = ans->count;
    ele = ans->head;
    while (ele != NULL) {
      struct set_element_t *next = ele->next;
      csound->Free(csound, ele);
      ele = next; res++;
    }
    csound->Free(csound, ans);
    return res;
}

void dag_build(CSOUND *csound, INSDS *chain)
{
    INSDS *save = chain;
    INSDS **task_map;
    int i;
    printf("DAG BUILD************************\n");
    if (task_status == NULL) 
      create_dag(csound); /* Should move elsewhere */
    else { 
      memset(task_watch, '\0', sizeof(watchList*)*task_max_size);
      for (i=0; i<task_max_size; i++) {
        task_dep[i]= NULL;
        wlmm[i].id = INVALID;
      }
    }
    task_map = csound->dag_task_map;
    csound->dag_num_active = 0;
    while (chain != NULL) {
      //INSTR_SEMANTICS *current_instr = dag_get_info(csound, chain->insno);
      csound->dag_num_active++;
      //dag->weight += current_instr->weight;
      chain = chain->nxtact;
    }
    if (csound->dag_num_active>task_max_size) {
      printf("**************need to extend task vector\n");
      exit(1);
    }
    for (i=0; i<csound->dag_num_active; i++) 
      task_status[i] = AVAILABLE, wlmm[i].id=i;
    csound->dag_changed = 0;
    printf("dag_num_active = %d\n", csound->dag_num_active);
    i = 0; chain = save;
    while (chain != NULL) {     /* for each instance check against later */
      int j = i+1;              /* count of instance */
      printf("\nWho depends on %d (instr %d)?\n", i, chain->insno);
      INSDS *next = chain->nxtact;
      INSTR_SEMANTICS *current_instr = dag_get_info(csound, chain->insno); 
      //csp_set_print(csound, current_instr->read);
      //csp_set_print(csound, current_instr->write);
      while (next) {
        INSTR_SEMANTICS *later_instr = dag_get_info(csound, next->insno);
        int cnt = 0;
        printf("%d ", j);
        //csp_set_print(csound, later_instr->read);
        //csp_set_print(csound, later_instr->write);
        //csp_set_print(csound, later_instr->read_write);
        if (dag_intersect(csound, current_instr->write,
                          later_instr->read, cnt++)       ||
            dag_intersect(csound, current_instr->read_write,
                          later_instr->read, cnt++)       ||
            dag_intersect(csound, current_instr->read,
                          later_instr->write, cnt++)      ||
            dag_intersect(csound, current_instr->write,
                          later_instr->write, cnt++)      ||
            dag_intersect(csound, current_instr->read_write,
                          later_instr->write, cnt++)      ||
            dag_intersect(csound, current_instr->read,
                          later_instr->read_write, cnt++) ||
            dag_intersect(csound, current_instr->write,
                          later_instr->read_write, cnt++)) {
          if (task_dep[j]==NULL) {
            /* get dep vector if missing and set watch first time */
            task_dep[j] = (char*)mcalloc(csound, sizeof(char)*(j-1));
            task_status[j] = WAITING;
            wlmm[j].next = task_watch[i];
            wlmm[j].id = j;
            task_watch[i] = &wlmm[j]; 
            printf("set watch %d to %d\n", j, i);
          }
          task_dep[j][i] = 1;
          printf("-yes ");
        }
        j++; next = next->nxtact;
      }
      task_map[i] = chain;
      i++; chain = chain->nxtact;
    }
    dag_print_state(csound);
}

void dag_reinit(CSOUND *csound)
{
    int i;
    dag_dispatched = 0;
    dag_done = 0;
    printf("DAG REINIT************************\n");
    for (i=csound->dag_num_active; i<task_max_size; i++)
      task_status[i] = DONE;
    for (i=0; i<csound->dag_num_active; i++) {
      int j;
      for (j=i-1; j>=0; j--)
        if (task_dep[i] && task_dep[i][j]) {
          task_status[i] = WAITING;
          wlmm[i].id = i;
          wlmm[i].next = task_watch[j];
          task_watch[j] = &wlmm[i];
          return;
        }
    }
    task_status[i] = AVAILABLE;
}

/* **** Thread code ****  */

static int getThreadIndex(CSOUND *csound, void *threadId)
{
    int index = 0;
    THREADINFO *current = csound->multiThreadedThreadInfo;
    
    if (current == NULL) return -1;
    while(current != NULL) {
      if (pthread_equal(*(pthread_t *)threadId, *(pthread_t *)current->threadId))
        return index;
      index++;
      current = current->next;
    }
    return -1;
}

/* #define ATOMIC_SWAP(x,y) __sync_val_compare_and_swap(&(x),x,y) */
/* #define ATOMIC_READ(x) __sync_fetch_and_or(&(x), 0) */
/* #define ATOMIC_WRITE(x,val) \ */
/*   {    __sync_and_and_fetch(&(x), 0); __sync_or_and_fetch(&(x), val); } */
// ??? _sync_val_compare_and_swap(&(x), x, val)
// #define ATOMIC_SWAP(x,y) { tmp = x; x = y; y = tmp; }
#define ATOMIC_READ(x) (x)
#define ATOMIC_WRITE(x,val) x = val

taskID dag_get_task(CSOUND *csound)
{
    int i;
    int morework = 0;
    printf("**GetTask from %d\n", csound->dag_num_active);
    for (i=0; i<csound->dag_num_active; i++) {
      printf("**%d %d (%d)\n", i, task_status[i], morework);
      if (__sync_bool_compare_and_swap(&(task_status[i]), AVAILABLE, INPROGRESS)) {
        dag_dispatched++;
        return (taskID)i;
      }
      else if (ATOMIC_READ(task_status[i])=WAITING) printf("**%d waiting\n", i);
      else if (ATOMIC_READ(task_status[i])=INPROGRESS) printf("**%d active\n", i);
      else if (ATOMIC_READ(task_status[i])==DONE) {
        printf("**%d done\n", i); morework++;
      }
    }
    dag_print_state(csound);
    if (morework==csound->dag_num_active) return (taskID)INVALID;
    printf("taskstodo=%d(dispatched=%d finished=%d)\n",
           morework, dag_dispatched, dag_done);
    return (taskID)WAIT;
}

watchList DoNotRead = { INVALID, NULL};
static int moveWatch(watchList **w, watchList *t)
{
    watchList *local;
    t->next = NULL;
    printf("moveWatch\n");
    do {
      local = ATOMIC_READ(*w);
      if (local==&DoNotRead) return 0;//was no & earlier
      else t->next = local;
    } while (!__sync_bool_compare_and_swap(w,local,t));// ??odd
    printf("moveWatch done\n");
    return 1;
}

void dag_end_task(CSOUND *csound, taskID i)
{
    watchList *to_notify, *next;
    int canQueue;
    int j, k;

    dag_done++;
    ATOMIC_WRITE(task_status[i], DONE);
    to_notify = task_watch[i]; task_watch[i]= &DoNotRead;
                               //ATOMIC_SWAP(task_watch[i], &DoNotRead);
    printf("Ending task %d\n", i);
    while (to_notify) {         /* walk the list of watchers */
      next = to_notify->next;
      j = to_notify->id;
      printf("notify task %d\n", j);
      canQueue = 1;
      for (k=0; k<i; k++) {     /* seek next watch */
        taskID l = task_dep[j][k];
        if (ATOMIC_READ(task_status[l]) != DONE) {
          printf("found task %d status %d\n", l, task_status[l]);
          if (moveWatch(&task_watch[j], to_notify)) {
            canQueue = 0;
            break;
          } 
          else {
            /* assert task_status[j] == DONE and we are in race */
            printf("Racing %d %d %d\n", i, j, k);
          }
        }
      }
      if (canQueue) {           /*  could use monitor here */
        task_status[j] = AVAILABLE;
      }
      to_notify = next;
    }
    dag_print_state(csound);
    return;    
}

// run one task from index
void dag_nodePerf(CSOUND *csound, taskID work)
{
    INSDS *insds = csound->dag_task_map[work];
    OPDS  *opstart = NULL;
    //int update_hdl = -1;
    int played_count = 0;

    played_count++;
        
    TRACE_2("DAG_work [%i] Playing: %s [%p]\n", 
            work, instr->name, insds);

    opstart = (OPDS *)insds;
    while ((opstart = opstart->nxtp) != NULL) {
      (*opstart->opadr)(csound, opstart); /* run each opcode */
    }
    insds->ksmps_offset = 0; /* reset sample-accuracy offset */
    TRACE_2("[%i] Played:  %s [%p]\n", work, instr->name, insds);

    return;
}

unsigned long dag_kperfThread(void * cs)
{
    INSDS *start;
    CSOUND *csound = (CSOUND *)cs;
    void *threadId;
    int index;
    int numThreads;

    /* Wait for start */
    csound->WaitBarrier(csound->barrier2);

    threadId = csound->GetCurrentThreadID();
    index = getThreadIndex(csound, threadId);
    numThreads = csound->oparms->numThreads;
    start = NULL;
    csound->Message(csound,
                    "Multithread performance: insno: %3d  thread %d of "
                    "%d starting.\n",
                    start ? start->insno : -1,
                    index,
                    numThreads);
    if (index < 0) {
      csound->Die(csound, "Bad ThreadId");
      return ULONG_MAX;
    }
    index++;

    while (1) {
      taskID work;
      csound->WaitBarrier(csound->barrier1);
      work = dag_get_task(csound);
      if (work==INVALID) continue;
      if (work==WAIT) break;
      if (dag_dispatched == csound->dag_num_active) {
        continue;
      }
      
      dag_nodePerf(csound, work);

      TRACE_1("[%i] Done\n", index);

      csound->WaitBarrier(csound->barrier2);
    }
    return 0;
}

#if 0
/* INV : Acyclic */
/* INV : Each entry is read by a single thread,
 *       no writes (but see OPT : Watch ordering) */
/* Thus no protection needed */

/* Used to mark lists that should not be added to, see NOTE : Race condition */
watchList nullList;
watchList *doNotAdd = &nullList;
watchList endwatch = { NULL, NULL };

/* Lists of tasks that depend on the given task */
watchList ** watch;         /* OPT : Structure lay out */

/* INV : Watches for different tasks are disjoint */
/* INV : Multiple threads can add to a watch list but only one will remove
 *       These are the only interactions */
/* Thus the use of CAS / atomic operations */

/* Static memory management for watch list cells */
typedef struct watchListMemoryManagement {
  enum bool used;
  watchList s;
} watchListMemoryManagement;

watchListMemoryManagement *wlmm; /* OPT : Structure lay out */

/* INV : wlmm[X].s.id == X; */  /* OPT : Data structure redundancy */
/* INV : status[X] == WAITING => wlmm[X].used */
/* INV : wlmm[X].s is in watch[Y] => wlmm[X].used */


/* Watch list helper functions */

void initialiseWatch (watchList **w, taskID id) {
  wlmm[id].used = TRUE;
  wlmm[id].s.id = id;
  wlmm[id].s.tail = *w;
  *w = &(wlmm[id].s);
}

watchList * getWatches(taskID id) {

    return __sync_lock_test_and_set (&(watch[id]), doNotAdd);
}

int moveWatch (watchList **w, watchList *t) {
  watchList *local;

  t->tail = NULL;

  do {
    local = atomicRead(*w);

    if (local == doNotAdd) {
      return 0;
    } else {
      t->tail = local;
    }
  } while (!atomicCAS(*w,local,t));   /* OPT : Delay loop */

  return 1;
}

void appendToWL (taskID id, watchList *l) {
  watchList *w;

  do {
    w = watch[id];
    l->tail = w;
    w = __sync_val_compare_and_swap(&(watch[id]),w,l);
  } while (!(w == l));

}

void deleteWatch (watchList *t) {
  wlmm[t->id].used = FALSE;
}




typedef struct monitor {
  pthread_mutex_t l = PTHREAD_MUTEX_INITIALIZER;
  unsigned int threadsWaiting = 0;    /* Shadows the length of workAvailable wait queue */
  queue<taskID> q;                    /* OPT : Dispatch order */
  pthread_cond_t workAvailable = PTHREAD_COND_INITIALIZER;
  pthread_cond_t done = PTHREAD_COND_INITIALIZER;
} monitor;                                    /* OPT : Lock-free */

/* INV : q.size() + dispatched <= ID */
/* INV : foreach(id,q.contents()) { status[id] = AVAILABLE; } */
/* INV : threadsWaiting <= THREADS */

monitor dispatch;


void addWork(monitor *dispatch, taskID id) {
  pthread_mutex_lock(&dispatch->l);

  status[id] = AVAILABLE;
  dispatch->q.push(id);
  if (threadsWaiting >= 1) {
    pthread_cond_signal(&dispatch->c);
  }

  pthread_mutex_unlock(&dispatch->l);
  return;
}

taskID getWork(monitor *dispatch) {
  taskID returnValue;

  pthread_mutex_lock(&dispatch->l);

  while (q.empty()) {
    ++dispatch->threadsWaiting;

    if (dispatch->threadsWaiting == THREADS) {
      /* Will the last person out please turn off the lights! */
      pthread_cond_signal(&dispatch->done);
    }

    pthread_cond_wait(&dispatch->l,&dispatch->workAvailable);
    --dispatch->threadsWaiting;
    
    /* NOTE : A while loop is needed as waking from this requires
     * reacquiring the mutex and in the mean time someone
     * might have got it first and removed the work. */
  }

  returnValue = q.pop();

  pthread_mutex_unlock(&dispatch->l);
  return returnValue;

}

void waitForWorkToBeCompleted (monitor *dispatch) {
  /* Requires
   * INV : threadsWaiting == THREADS <=> \forall id \in ID . status[id] == DONE
   */

  pthread_mutex_lock(&dispatch->l);

  if (dispatch->threadsWaiting < THREADS) {
    pthread_cond_wait(&dispatch->l,&dispatch->done);
  }

  /* This assertion is more difficult to prove than it might first appear */
  assert(dispatch->threadsWaiting == THREADS);

  pthread_mutex_unlock(&dispatch->l);
  return;
}














void mainThread (State *s) {

  /* Set up the DAG */
  if (s->firstRun || s->updateNeeded) {
    dep = buildDAG(s);        /* OPT : Dispatch order */
    /* Other : Update anything that is indexed by task 
     * (i.e. all arrays given length ID) */
  }

  /* Reset the data structure */
  foreach (id in ID) {
    watch[id] = NULL;
  }

  /* Initialise the dispatch queue */
  foreach (id in ID) {       /* OPT : Dispatch order */
    if (dep[id] == EMPTYSET) {
      atomicWrite(status[id] = AVAILABLE);
      addWork(*dispatch,id);

    } else {
      atomicWrite(status[id] = WAITING);
      initialiseWatch(&watch[choose(dep[id])], id);  /* OPT : Watch ordering */

    }
  }

  /* INV : Data structure access invariants start here */
  /* INV : Status only decrease from now */
  /* INV : Watch list for id contains a subset of the things that depend on id */
  /* INV : Each id appears in at most one watch list */
  /* INV : doNotAdd only appears at the head of a watch list */
  /* INV : if (watch[id] == doNotAdd) then { status[id] == DONE; } */


  waitForWorkToBeCompleted(*dispatch);

  return;
}







void workerThread (State *s) {
  taskID work;
  watchList *tasksToNotify, next;
  bool canQueue;

  do {

    task = getWork(dispatch);

    /* Do stuff */
    atomicWrite(status[work] = INPROGRESS);
    doStuff(work);
    atomicWrite(status[work] = DONE);    /* NOTE : Race condition */
    
    
    tasksToNotify = getWatches(work);
    
    while (tasksToNotify != NULL) {
      next = tasksToNotify->tail;
      
      canQueue = TRUE;
      foreach (dep in dep[tasksToNotify->id]) {  /* OPT : Watch ordering */ 
	if (atomicRead(status[dep]) != DONE) {
	  /* NOTE : Race condition */
	  if (moveWatch(watch[dep],tasksToNotify)) {
	    canQueue = FALSE;
	    break;
	  } else {
	    /* Have hit the race condition, try the next option */
	    assert(atomicRead(status[dep]) == DONE);
	  }
	}
      }
      
      if (canQueue) {                    /* OPT : Save one work item */
	addWork(*dispatch,tasksToNotify->id);
	deleteWatch(tasksToNotify);
      }
      
      tasksToNotify = next;
    }
    
  } while (1);  /* NOTE : some kind of control for thread exit needed */

  return;
}




/* OPT : Structure lay out
 *
 * All data structures that are 1. modified by one or more thread and
 * 2. accessed by multiple threads, should be aligned to cache lines and
 * padded so that there is only one instance per cache line.  This will reduce
 * false memory contention between objects that just happen to share a cache
 * line.  Blocking to 64 bytes will probably be sufficient and if people really
 * care about performance that much they can tune to their particular
 * architecture.
 */

/* OPT : Watch ordering
 *
 * Moving a watch is relatively cheap (in the uncontended case) but 
 * it would be best to avoid moving watches where possible.  The ideal
 * situation would be for every task to watch the last pre-requisite.
 * There are two places in the code that affect the watch ordering;
 * the initial assignment and the movement when a watch is triggered.
 * Prefering WAITING tasks (in the later) and lower priority tasks 
 * (if combined with the dispatch order optimisation below) are probably
 * good choices.  One mechanism would be to reorder the set (or array) of
 * dependencies to store this information.  When looking for a (new) watch, 
 * tasks are sorted with increasing status first and then the first one picked.
 * Keeping the list sorted (or at least split between WAITING and others) with
 * each update should (if the dispatch order is fixed / slowly adapting) result
 * in the best things to watch moving to the front and thus adaptively give
 * the best possible tasks to watch.  The interaction with a disaptch order
 * heuristic would need to be considered.  Note that only one thread will
 * look at any given element of dep[] so they can be re-ordered without
 * needing locking.
 */

/* OPT : Structure lay out
 *
 * Some of the fields are not strictly needed and are just there to make
 * the algorithm cleaner and more intelligible.  The id fields of the watch
 * lists are not really needed as there is one per task and their position
 * within the watchListMemoryManager array allows the task to be infered.
 * Likewise the used flag in the memory manager is primarily for book-keeping
 * and checking / assertions and could be omitted.
 */

/* OPT : Delay loop
 * 
 * In theory it is probably polite to put a slowly increasing delay in
 * after a failed compare and swap to reduce pressure on the memory
 * subsystem in the highly contended case.  As multiple threads adding
 * to a task's watch list simultaneously is probably a rare event, the
 * delay loop is probably unnecessary.
 */

/* OPT : Dispatch order
 *
 * The order in which tasks are dispatched affects the amount of 
 * parallelisation possible.  Picking the exact scheduling order, even
 * if the duration of the tasks is known is probably NP-Hard (see 
 * bin-packing*) so heuristics are probably the way to go.  The proporition
 * of tasks which depend on a given task is probably a reasonable primary
 * score, with tie-breaks going to longer tasks.  This can either be 
 * applied to just the initial tasks (either in ordering the nodes in the DAG)
 * or in the order in which they are traversed.  Alternatively by
 * sorting the queue / using a heap / other priority queuing structure
 * it might be possible to do this dynamically.  The best solution would
 * probably be adaptive with a task having its priority incremented 
 * each time another worker thread blocks on a shortage of work, with these
 * increments also propagated 'upwards' in the DAG.
 *
 * *. Which means that a solver could be used to give the best possible
 *    schedule / the maximum parallelisation.  This could be useful for
 *    optimisation.
 */

/* OPT : Lock-free
 *
 * A lock free dispatch mechanism is probably possible as threads can 
 * scan the status array for things listed as AVAILABLE and then atomicCAS
 * to INPROGRESS to claim them.  But this starts to involve busy-waits or
 * direct access to futexes and is probably not worth it.
 */

/* OPT : Save one work item
 *
 * Rather than adding all watching tasks who have their dependencies met to
 * the dispatch queue, saving one (perhaps the best, see OPT : Dispatch order)
 * means the thread does not have to wait.  In the case of a purely linear DAG
 * this should be roughly as fast as the single threaded version.
 */


/* NOTE : Race condition
 *
 * There is a subtle race condition:
 *
 *   Thread 1                             Thread 2
 *   --------                             --------
 *                                        atomicRead(status[dep]) != DONE
 *   atomicWrite(status[work] = DONE);
 *   tasksToNotify = getWatches(work);
 *                                        moveWatch(watch[dep],tasksToNotify);
 * 
 * The key cause is that the status and the watch list cannot be updated
 * simultaneously.  However as getWatches removes all watches and moves or
 * removes them, it is sufficient to have a doNotAdd watchList node to detect
 * this race condition and resolve it by having moveWatch() fail.
 */

void newdag_alloc(CSOUND *csound, int numtasks)
{  
    doNotAdd = &endwatch;
??
    watch = (watchList **)mcalloc(csound, sizeof(watchList *)*numtasks);
    wlmm = (watchListMemoryManagement *)
      mcalloc(csound, sizeof(watchListMemoryManagement)*numtasks);

}

#endif

#endif