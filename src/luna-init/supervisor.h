/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_SUPERVISOR_H
#define MAHINA_SUPERVISOR_H

/*
 * supervisor.h — Service Supervisor
 * Authority: Volume II / 04_init_system.md §Service Supervisor
 *
 * Starts services in topological order, applies restart policy on failure,
 * and provides the reaper callback. Uses fork() + execve() for spawning.
 */

#include "service.h"
#include <stdbool.h>
#include <sys/types.h>

/*
 * supervisor_start_all() — Start all services in dependency order (Async).
 *
 * This now simply sets up the initial state for the async pump.
 */
int supervisor_start_all(void);

/*
 * supervisor_pump() — Event loop tick for the supervisor.
 *
 * Checks readiness of STARTING services, and spawns PENDING services
 * whose dependencies are met and scheduled time has arrived.
 */
void supervisor_pump(void);

/*
 * supervisor_is_boot_complete() — Check if all services have finished starting.
 *
 * Returns true if no services are in PENDING or STARTING state.
 */
bool supervisor_is_boot_complete(void);

/*
 * supervisor_start_one() — Start a single service by name.
 *
 * Used by luna-init-ctl for manual service start.
 * Service must be in STOPPED or DEGRADED state.
 * Returns 0 on success (process spawned), -1 on error.
 */
int supervisor_start_one(const char *name);

/*
 * supervisor_stop_one() — Stop a single running service.
 *
 * Sends stop_signal, waits up to stop_timeout_ms, then SIGKILL.
 * Returns 0 when the process has exited.
 */
int supervisor_stop_one(const char *name);

/*
 * supervisor_on_exit() — Called by reaper when a supervised PID exits.
 *
 * This IS reaper_on_exit() — implemented here, declared in reaper.h.
 * Applies restart policy, updates service state, logs the event.
 */
void supervisor_on_exit(pid_t pid, int exit_code, bool by_signal);

/*
 * supervisor_check_ready() — Poll a service's readiness condition.
 *
 * Returns true if the service is ready (file exists / socket connects).
 * Returns false if the timeout has been exceeded (caller marks DEGRADED).
 */
bool supervisor_check_ready(service_t *svc, long long start_ms);

#endif /* MAHINA_SUPERVISOR_H */
