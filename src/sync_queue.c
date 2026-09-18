#include <string.h>

#include "sync_queue.h"

static const Job *queue[MAX_JOBS];
static int queue_count = 0;
static int current_index = 0;
static QueueState state = QUEUE_STATE_IDLE;

static SyncQueueResult results[MAX_JOBS];
static int result_count = 0;

void sync_queue_build(void)
{
	queue_count = jobs_count();
	for (int i = 0; i < queue_count; i++) queue[i] = jobs_get(i);
	current_index = 0;
	result_count = 0;
	state = QUEUE_STATE_IDLE;
}

SyncQueuePreview sync_queue_preview(void)
{
	SyncQueuePreview preview = {0};
	preview.job_count = queue_count;

	for (int i = 0; i < queue_count; i++) {
		SyncPreview p = sync_engine_preview(queue[i]);
		if (!p.ok) continue; // this job's failure surfaces later, at confirm time

		preview.to_copy_count += p.to_copy_count;
		preview.to_copy_bytes += p.to_copy_bytes;

		if (queue[i]->mirror && p.to_delete_count > 0) {
			preview.mirror[preview.mirror_count].job = queue[i];
			preview.mirror[preview.mirror_count].to_delete_count = p.to_delete_count;
			preview.mirror_count++;
		}
	}

	return preview;
}

// Re-fetches queue[current_index]'s diff immediately before confirming it --
// sync_queue_preview()'s aggregate pass above left sync_engine.c's
// module-static cache pointing at whichever job it queried last, not
// necessarily this one.
static void startCurrentJob(void)
{
	sync_engine_preview(queue[current_index]);
	sync_engine_confirm(queue[current_index]);
}

void sync_queue_confirm(void)
{
	current_index = 0;
	result_count = 0;
	state = QUEUE_STATE_RUNNING;
	startCurrentJob();
}

static void recordResult(SyncState final_state)
{
	SyncProgress p = sync_engine_progress();
	SyncQueueResult *r = &results[result_count++];
	r->job = queue[current_index];
	r->ok = (final_state == SYNC_STATE_DONE);
	r->files_copied = p.files_copied;
	r->bytes_copied = p.bytes_copied;
	r->files_deleted = p.files_deleted;
}

QueueState sync_queue_tick(void)
{
	if (state != QUEUE_STATE_RUNNING) return state;

	SyncState s = sync_engine_state();
	bool running = (s == SYNC_STATE_COPYING || s == SYNC_STATE_DELETING);
	if (running) {
		sync_engine_tick();
		s = sync_engine_state();
		running = (s == SYNC_STATE_COPYING || s == SYNC_STATE_DELETING);
	}

	if (!running) {
		recordResult(s);
		current_index++;
		if (current_index >= queue_count) state = QUEUE_STATE_DONE;
		else startCurrentJob();
	}

	return state;
}

void sync_queue_cancel(void)
{
	if (state != QUEUE_STATE_RUNNING) return;
	sync_engine_cancel();
	state = QUEUE_STATE_CANCELLED;
}

QueueState sync_queue_state(void)
{
	return state;
}

int sync_queue_count(void)
{
	return queue_count;
}

int sync_queue_current_index(void)
{
	return current_index;
}

const Job *sync_queue_currentJob(void)
{
	return (current_index < queue_count) ? queue[current_index] : NULL;
}

int sync_queue_resultCount(void)
{
	return result_count;
}

const SyncQueueResult *sync_queue_result(int index)
{
	return &results[index];
}
