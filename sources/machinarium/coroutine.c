
/*
 * machinarium.
 *
 * cooperative multitasking engine.
 */

#include <string.h>

#include <machinarium/machinarium.h>
#include <machinarium/coroutine.h>
#include <machinarium/call.h>
#include <machinarium/memory.h>

void mm_coroutine_init(mm_coroutine_t *coroutine)
{
	memset(coroutine, 0, sizeof(mm_coroutine_t));
	coroutine->id = UINT64_MAX;
	coroutine->state = MM_CNEW;
	coroutine->errno_ = 0;
	coroutine->call_ptr = NULL;
	coroutine->io_count = 0;
	memset(coroutine->name, 0, MM_COROUTINE_MAX_NAME_LEN + 1);
	mm_list_init(&coroutine->joiners);
	mm_list_init(&coroutine->link);
	mm_list_init(&coroutine->link_join);
	for (int i = 0; i < MM_COROUTINE_CLS_LEN; i++) {
		coroutine->cls_array[i].value = NULL;
		coroutine->cls_array[i].dtor = NULL;
	}

#ifdef MM_MEM_PROF
	coroutine->allocated_bytes = 0;
	coroutine->freed_bytes = 0;
#endif
}

mm_coroutine_t *mm_coroutine_allocate(int stack_size, int stack_size_guard)
{
	mm_coroutine_t *coroutine;
	coroutine = mm_malloc(sizeof(mm_coroutine_t));
	if (coroutine == NULL) {
		return NULL;
	}
	mm_coroutine_init(coroutine);
	int rc;
	rc = mm_contextstack_create(&coroutine->stack, stack_size,
				    stack_size_guard);
	if (rc == -1) {
		mm_free(coroutine);
		return NULL;
	}
	return coroutine;
}

void mm_coroutine_free(mm_coroutine_t *coroutine)
{
	for (int i = 0; i < coroutine->cls_size; i++) {
		if (coroutine->cls_array[i].dtor != NULL) {
			coroutine->cls_array[i].dtor(
				coroutine->cls_array[i].value);
		}
	}
	mm_free(coroutine->cls_array);

	mm_context_destroy(&coroutine->context);
	mm_contextstack_free(&coroutine->stack);
	mm_free(coroutine);
}

void mm_coroutine_cancel(mm_coroutine_t *coroutine)
{
	if (coroutine->cancel) {
		return;
	}
	coroutine->cancel++;
	if (coroutine->call_ptr) {
		mm_call_cancel(coroutine->call_ptr, coroutine);
	}
}

void mm_coroutine_set_name(mm_coroutine_t *coro, const char *name)
{
	if (name == NULL) {
		memset(coro->name, 0, MM_COROUTINE_MAX_NAME_LEN + 1);
		return;
	}

	stpncpy(coro->name, name, MM_COROUTINE_MAX_NAME_LEN);
	coro->name[MM_COROUTINE_MAX_NAME_LEN] = 0;
}

const char *mm_coroutine_get_name(mm_coroutine_t *coro)
{
	return coro->name;
}

int mm_cls_set(mm_coroutine_t *coroutine, int key, void *value,
	       mm_cls_dtor_t dtor)
{
	if (key < 0 || key >= coroutine->cls_size) {
		return -1;
	}

	mm_cls_node_t *node = &coroutine->cls_array[key];
	if (node->value != NULL) {
		return -2;
	}

	node->value = value;
	node->dtor = dtor;

	return 0;
}

void *mm_cls_get(mm_coroutine_t *coroutine, int key)
{
	if (key < 0 || key >= coroutine->cls_size) {
		return NULL;
	}

	return coroutine->cls_array[key].value;
}
