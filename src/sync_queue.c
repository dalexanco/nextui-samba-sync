#include <string.h>

#include "defines.h" // api.h needs the platform defines first
#include "api.h"
#include "sync_queue.h"

static LinkPhase phases[MAX_LINKS];
static LinkCheck checks[MAX_LINKS];
static LinkState lasts[MAX_LINKS];

static QueueMode mode = QUEUE_IDLE;
static int current = 0; // index of the link being processed by the current run
static int sync_total = 0;
static int sync_position = 0;
static bool sync_finished = false;

static bool isValid(int index)
{
	return !sync_config_link_get(index)->config_error[0];
}

void sync_queue_reload(void)
{
	sync_config_load();
	for (int i = 0; i < sync_config_link_count(); i++) {
		phases[i] = LINK_PHASE_IDLE;
		memset(&checks[i], 0, sizeof(checks[i]));
		link_state_load(sync_config_link_get(i), &lasts[i]);
	}
	mode = QUEUE_IDLE;
	sync_finished = false;
}

// First link at or after `from` still waiting in `pending`, or the link count.
static int nextPending(int from, LinkPhase pending)
{
	int count = sync_config_link_count();
	while (from < count && phases[from] != pending) from++;
	return from;
}

void sync_queue_check_all(void)
{
	sync_finished = false;
	for (int i = 0; i < sync_config_link_count(); i++)
		phases[i] = isValid(i) ? LINK_PHASE_CHECK_PENDING : LINK_PHASE_IDLE;
	current = nextPending(0, LINK_PHASE_CHECK_PENDING);
	mode = current < sync_config_link_count() ? QUEUE_CHECKING : QUEUE_IDLE;
}

void sync_queue_sync_all(void)
{
	sync_finished = false;
	sync_total = 0;
	sync_position = 0;
	for (int i = 0; i < sync_config_link_count(); i++) {
		if (!isValid(i)) continue;
		phases[i] = LINK_PHASE_SYNC_PENDING;
		sync_total++;
	}
	current = nextPending(0, LINK_PHASE_SYNC_PENDING);
	mode = current < sync_config_link_count() ? QUEUE_SYNCING : QUEUE_IDLE;
	// A sync is the one thing this pak does that is not a menu: SMB2 packet
	// handling, NTLM signing and the writes to the SD card all want the
	// cores. Back to the menu governor as soon as the run ends.
	if (mode == QUEUE_SYNCING) PWR_setCPUSpeed(CPU_SPEED_PERFORMANCE);
}

static bool tickCheck(void)
{
	const Link *link = sync_config_link_get(current);
	if (phases[current] == LINK_PHASE_CHECK_PENDING) {
		phases[current] = LINK_PHASE_CHECKING;
		return true;
	}

	checks[current] = sync_engine_check(link);
	phases[current] = LINK_PHASE_CHECKED;
	current = nextPending(current + 1, LINK_PHASE_CHECK_PENDING);
	if (current >= sync_config_link_count()) mode = QUEUE_IDLE;
	return true;
}

// Records the outcome of the link sync_engine.c just finished.
static void endLinkSync(void)
{
	const Link *link = sync_config_link_get(current);
	lasts[current] = *sync_engine_result();
	link_state_save(link, &lasts[current]);
	phases[current] = LINK_PHASE_SYNCED;
}

static bool tickSync(void)
{
	if (phases[current] == LINK_PHASE_SYNC_PENDING) {
		phases[current] = LINK_PHASE_SYNCING;
		sync_position++;
		return true;
	}

	SyncState state = sync_engine_state();
	if (state != SYNC_STATE_COPYING && state != SYNC_STATE_DELETING) {
		// Just moved to SYNCING by the previous call: start it (blocking check).
		sync_engine_start(sync_config_link_get(current));
	}
	else {
		sync_engine_tick();
	}

	state = sync_engine_state();
	if (state == SYNC_STATE_DONE || state == SYNC_STATE_CANCELLED) {
		endLinkSync();
		current = nextPending(current + 1, LINK_PHASE_SYNC_PENDING);
		if (current >= sync_config_link_count()) {
			mode = QUEUE_IDLE;
			sync_finished = true;
			PWR_setCPUSpeed(CPU_SPEED_AUTO);
		}
	}
	return true;
}

bool sync_queue_tick(void)
{
	if (mode == QUEUE_CHECKING) return tickCheck();
	if (mode == QUEUE_SYNCING) return tickSync();
	return false;
}

void sync_queue_cancel(void)
{
	int count = sync_config_link_count();
	if (mode == QUEUE_CHECKING) {
		for (int i = 0; i < count; i++)
			if (phases[i] == LINK_PHASE_CHECK_PENDING || phases[i] == LINK_PHASE_CHECKING) phases[i] = LINK_PHASE_IDLE;
	}
	else if (mode == QUEUE_SYNCING) {
		if (phases[current] == LINK_PHASE_SYNCING) {
			SyncState state = sync_engine_state();
			if (state == SYNC_STATE_COPYING || state == SYNC_STATE_DELETING) {
				sync_engine_cancel();
				endLinkSync();
			}
			else {
				phases[current] = LINK_PHASE_SYNC_CANCELLED; // not started yet
			}
		}
		for (int i = 0; i < count; i++)
			if (phases[i] == LINK_PHASE_SYNC_PENDING) phases[i] = LINK_PHASE_SYNC_CANCELLED;
		sync_finished = true;
		PWR_setCPUSpeed(CPU_SPEED_AUTO);
	}
	mode = QUEUE_IDLE;
}

QueueMode sync_queue_mode(void)
{
	return mode;
}

int sync_queue_sync_position(void)
{
	return sync_position;
}

int sync_queue_sync_total(void)
{
	return sync_total;
}

bool sync_queue_sync_finished(void)
{
	return sync_finished;
}

LinkPhase sync_queue_phase(int link_index)
{
	return phases[link_index];
}

const LinkCheck *sync_queue_check(int link_index)
{
	return &checks[link_index];
}

const LinkState *sync_queue_last(int link_index)
{
	return &lasts[link_index];
}

bool sync_queue_can_sync(void)
{
	if (mode != QUEUE_IDLE) return false;
	for (int i = 0; i < sync_config_link_count(); i++)
		if (isValid(i)) return true;
	return false;
}
