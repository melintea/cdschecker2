#include <threads.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "librace.h"
#include "model-assert.h"

#define MAX_NODES           0xf

typedef unsigned long long pointer;
typedef atomic_ullong pointer_t;

#define MAKE_POINTER(ptr, count)	((((pointer)count) << 32) | ptr)
#define PTR_MASK 0xffffffffLL
#define COUNT_MASK (0xffffffffLL << 32)

static inline void set_count(pointer *p, unsigned int val) { *p = (*p & ~COUNT_MASK) | ((pointer)val << 32); }
static inline void set_ptr(pointer *p, unsigned int val) { *p = (*p & ~PTR_MASK) | val; }
static inline unsigned int get_count(pointer p) { return (p & COUNT_MASK) >> 32; }
static inline unsigned int get_ptr(pointer p) { return p & PTR_MASK; }

typedef struct node {
	unsigned int value;
	pointer_t next;
} node_t;

typedef struct {
	pointer_t head;
	pointer_t tail;
	node_t nodes[MAX_NODES + 1];
} queue_t;

void init_queue(queue_t *q, int num_threads);

#include <list>
using namespace std;
/**
	@Begin
	@Global_define:
		@DeclareStruct:
		typedef struct tag_elem {
			Tag id;
			unsigned int data;
		} tag_elem_t;
		
		@DeclareVar:
		list<tag_elem_t> __queue;
		Tag tag;
		@InitVar:
			__queue = list<tag_elem_t>();
			tag = 1; // Beginning of available id
	@Happens_before:
		# Only check the happens-before relationship according to the id of the
		# commit_point_set. For commit_point_set that has same ID, A -> B means
		# B happens after the previous A.
		Enqueue -> Dequeue
	@End
*/


int get_thread_num();


#define relaxed memory_order_relaxed
#define release memory_order_release
#define acquire memory_order_acquire

#define MAX_FREELIST 4 /* Each thread can own up to MAX_FREELIST free nodes */
#define INITIAL_FREE 2 /* Each thread starts with INITIAL_FREE free nodes */

#define POISON_IDX 0x666

static unsigned int (*free_lists)[MAX_FREELIST];

/* Search this thread's free list for a "new" node */
static unsigned int new_node()
{
	int i;
	int t = get_thread_num();
	for (i = 0; i < MAX_FREELIST; i++) {
		unsigned int node = load_32(&free_lists[t][i]);
		if (node) {
			store_32(&free_lists[t][i], 0);
			return node;
		}
	}
	/* free_list is empty? */
	MODEL_ASSERT(0);
	return 0;
}

/* Place this node index back on this thread's free list */
static void reclaim(unsigned int node)
{
	int i;
	int t = get_thread_num();

	/* Don't reclaim NULL node */
	MODEL_ASSERT(node);

	for (i = 0; i < MAX_FREELIST; i++) {
		/* Should never race with our own thread here */
		unsigned int idx = load_32(&free_lists[t][i]);

		/* Found empty spot in free list */
		if (idx == 0) {
			store_32(&free_lists[t][i], node);
			return;
		}
	}
	/* free list is full? */
	MODEL_ASSERT(0);
}

void init_queue(queue_t *q, int num_threads)
{
	/**
		@Begin
		@Entry_point
		@End
	*/

	int i, j;

	/* Initialize each thread's free list with INITIAL_FREE pointers */
	/* The actual nodes are initialized with poison indexes */
	free_lists = (unsigned int**) malloc(num_threads * sizeof(*free_lists));
	for (i = 0; i < num_threads; i++) {
		for (j = 0; j < INITIAL_FREE; j++) {
			free_lists[i][j] = 2 + i * MAX_FREELIST + j;
			atomic_init(&q->nodes[free_lists[i][j]].next, MAKE_POINTER(POISON_IDX, 0));
		}
	}

	/* initialize queue */
	atomic_init(&q->head, MAKE_POINTER(1, 0));
	atomic_init(&q->tail, MAKE_POINTER(1, 0));
	atomic_init(&q->nodes[1].next, MAKE_POINTER(0, 0));
}

/**
	@Begin
	@Interface: Enqueue
	@Commit_point_set: Enqueue_Success_Point
	@ID: tag++
	@Action:
		# __ID__ is an internal macro that refers to the id of the current
		# interface call
		tag_elem_t elem;
		elem.id = __ID__;
		elem.data = val;
		__queue.push_back(elem);
	@End
*/
void enqueue(queue_t *q, unsigned int val)
{
	int success = 0;
	unsigned int node;
	pointer tail;
	pointer next;
	pointer tmp;

	node = new_node();
	store_32(&q->nodes[node].value, val);
	tmp = atomic_load_explicit(&q->nodes[node].next, relaxed);
	set_ptr(&tmp, 0); // NULL
	atomic_store_explicit(&q->nodes[node].next, tmp, relaxed);

	while (!success) {
		tail = atomic_load_explicit(&q->tail, acquire);
		next = atomic_load_explicit(&q->nodes[get_ptr(tail)].next, acquire);
		if (tail == atomic_load_explicit(&q->tail, relaxed)) {

			/* Check for uninitialized 'next' */
			MODEL_ASSERT(get_ptr(next) != POISON_IDX);

			if (get_ptr(next) == 0) { // == NULL
				pointer value = MAKE_POINTER(node, get_count(next) + 1);
				success = atomic_compare_exchange_strong_explicit(&q->nodes[get_ptr(tail)].next,
						&next, value, release, release);
			}
			if (!success) {
				unsigned int ptr = get_ptr(atomic_load_explicit(&q->nodes[get_ptr(tail)].next, acquire));
				pointer value = MAKE_POINTER(ptr,
						get_count(tail) + 1);
				int commit_success = 0;
				commit_success = atomic_compare_exchange_strong_explicit(&q->tail,
						&tail, value, release, release);
				/**
					@Begin
					@Commit_point_define_check: commit_success == true
					@Label: Enqueue_Success_Point
					@End
				*/
				thrd_yield();
			}
		}
	}
	atomic_compare_exchange_strong_explicit(&q->tail,
			&tail,
			MAKE_POINTER(node, get_count(tail) + 1),
			release, release);
}

/**
	@Begin
	@Interface: Dequeue
	@Commit_point_set: Dequeue_Success_Point
	@ID: __queue.back().id
	@Action:
		unsigned int _Old_Val = __queue.front().data;
		__queue.pop_front();
	@Post_check:
		_Old_Val == __RET__
	@End
*/
unsigned int dequeue(queue_t *q)
{
	unsigned int value;
	int success = 0;
	pointer head;
	pointer tail;
	pointer next;

	while (!success) {
		head = atomic_load_explicit(&q->head, acquire);
		tail = atomic_load_explicit(&q->tail, relaxed);
		next = atomic_load_explicit(&q->nodes[get_ptr(head)].next, acquire);
		if (atomic_load_explicit(&q->head, relaxed) == head) {
			if (get_ptr(head) == get_ptr(tail)) {

				/* Check for uninitialized 'next' */
				MODEL_ASSERT(get_ptr(next) != POISON_IDX);

				if (get_ptr(next) == 0) { // NULL
					return 0; // NULL
				}
				atomic_compare_exchange_strong_explicit(&q->tail,
						&tail,
						MAKE_POINTER(get_ptr(next), get_count(tail) + 1),
						release, release);
				thrd_yield();
			} else {
				value = load_32(&q->nodes[get_ptr(next)].value);
				success = atomic_compare_exchange_strong_explicit(&q->head,
						&head,
						MAKE_POINTER(get_ptr(next), get_count(head) + 1),
						release, release);
				/**
					@Begin
					@Commit_point_define_check: success == true
					@Label: Dequeue_Success_Point
					@End
				*/
				if (!success)
					thrd_yield();
			}
		}
	}
	reclaim(get_ptr(head));
	return value;
}



#include <stdlib.h>
#include <stdio.h>
#include <threads.h>

#include "my_queue.h"
#include "model-assert.h"

static int procs = 2;
static queue_t *queue;
static thrd_t *threads;
static unsigned int *input;
static unsigned int *output;
static int num_threads;

int get_thread_num()
{
	thrd_t curr = thrd_current();
	int i;
	for (i = 0; i < num_threads; i++)
		if (curr.priv == threads[i].priv)
			return i;
	MODEL_ASSERT(0);
	return -1;
}

static void main_task(void *param)
{
	unsigned int val;
	int pid = *((int *)param);

	if (!pid) {
		input[0] = 17;
		enqueue(queue, input[0]);
		output[0] = dequeue(queue);
	} else {
		input[1] = 37;
		enqueue(queue, input[1]);
		output[1] = dequeue(queue);
	}
}

int user_main(int argc, char **argv)
{
	int i;
	int *param;
	unsigned int in_sum = 0, out_sum = 0;

	queue = (queue_t*) calloc(1, sizeof(*queue));
	MODEL_ASSERT(queue);

	num_threads = procs;
	threads = (thrd_t*) malloc(num_threads * sizeof(thrd_t));
	param = (int*) malloc(num_threads * sizeof(*param));
	input = (unsigned int*) calloc(num_threads, sizeof(*input));
	output = (unsigned int*) calloc(num_threads, sizeof(*output));

	init_queue(queue, num_threads);
	for (i = 0; i < num_threads; i++) {
		param[i] = i;
		thrd_create(&threads[i], main_task, &param[i]);
	}
	for (i = 0; i < num_threads; i++)
		thrd_join(threads[i]);

	for (i = 0; i < num_threads; i++) {
		in_sum += input[i];
		out_sum += output[i];
	}
	for (i = 0; i < num_threads; i++)
		printf("input[%d] = %u\n", i, input[i]);
	for (i = 0; i < num_threads; i++)
		printf("output[%d] = %u\n", i, output[i]);
	MODEL_ASSERT(in_sum == out_sum);

	free(param);
	free(threads);
	free(queue);

	return 0;
}
