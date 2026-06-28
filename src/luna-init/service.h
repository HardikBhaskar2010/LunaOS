/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_SERVICE_H
#define MAHINA_SERVICE_H

/*
 * service.h — Service Definition and State Machine
 * Authority: Volume II / 04_init_system.md §Service File Format
 * Authority: Volume II / 04_init_system.md §Service States
 *
 * Service files live in /etc/luna/services/ directory
 * Each file describes one service with the schema defined in 04_init_system.md.
 */

#include <stdbool.h>
#include <sys/types.h>

/* ─── Limits ─────────────────────────────────────────────────────────────── */

#define SERVICE_MAX_COUNT        32
#define SERVICE_MAX_NAME_LEN     64
#define SERVICE_MAX_BINARY_LEN   256
#define SERVICE_MAX_ARGS         16
#define SERVICE_MAX_ARG_LEN      256
#define SERVICE_MAX_DEPS         8
#define SERVICE_MAX_ENV_VARS     8
#define SERVICE_MAX_ENV_LEN      256
#define SERVICE_MAX_READY_PATH   256

/* ─── Service state machine ──────────────────────────────────────────────── */

/* States per Volume II / 04_init_system.md §Service States */
typedef enum {
    SERVICE_STATE_PENDING  = 0,  /* Not yet started — waiting for deps       */
    SERVICE_STATE_STARTING = 1,  /* exec()'d — waiting for readiness signal  */
    SERVICE_STATE_RUNNING  = 2,  /* Running and ready                        */
    SERVICE_STATE_DEGRADED = 3,  /* Exceeded restart attempts — stopped      */
    SERVICE_STATE_STOPPING = 4,  /* Stop signal sent — waiting for exit      */
    SERVICE_STATE_STOPPED  = 5,  /* Exited cleanly                           */
    SERVICE_STATE_FAILED   = 6,  /* Exited with error; may restart           */
} service_state_t;

/* ─── Restart policy ─────────────────────────────────────────────────────── */

typedef enum {
    RESTART_NEVER      = 0,
    RESTART_ALWAYS     = 1,
    RESTART_ON_FAILURE = 2,
} restart_policy_t;

/* ─── Readiness detection method ─────────────────────────────────────────── */

typedef enum {
    READY_NONE   = 0,  /* Assume ready immediately after exec()    */
    READY_FILE   = 1,  /* Poll for file existence                  */
    READY_SOCKET = 2,  /* Attempt connect() to Unix socket         */
    READY_HTTP   = 3,  /* HTTP GET localhost URL (stubbed in v0.1) */
    READY_SIGNAL = 4,  /* Wait for SIGUSR1 to PID 1                */
} ready_method_t;

/* ─── Service definition ─────────────────────────────────────────────────── */

typedef struct {
    /* [service] — required fields */
    char name[SERVICE_MAX_NAME_LEN];
    char binary[SERVICE_MAX_BINARY_LEN];
    char description[256];

    /* [service] — optional args */
    char args[SERVICE_MAX_ARGS][SERVICE_MAX_ARG_LEN];
    int  arg_count;

    /* [service] — optional working directory */
    char workdir[256];

    /* [service.env] — optional environment variables */
    struct {
        char key[SERVICE_MAX_ENV_LEN];
        char val[SERVICE_MAX_ENV_LEN];
    } env[SERVICE_MAX_ENV_VARS];
    int env_count;

    /* [service.identity] — optional user/group */
    char run_user[64];
    char run_group[64];

    /* [service.depends] — dependency declarations */
    char after[SERVICE_MAX_DEPS][SERVICE_MAX_NAME_LEN];
    int  after_count;
    char before[SERVICE_MAX_DEPS][SERVICE_MAX_NAME_LEN];
    int  before_count;

    /* [service.restart] — restart policy */
    restart_policy_t restart_policy;
    int              restart_attempts;   /* max retries before DEGRADED */
    int              restart_delay_ms;

    /* [service.ready] — readiness detection */
    ready_method_t ready_method;
    char           ready_path[SERVICE_MAX_READY_PATH]; /* file or socket path */
    int            ready_timeout_ms;

    /* [service.stop] — shutdown behavior */
    int stop_signal;       /* signal to send (default: SIGTERM) */
    int stop_timeout_ms;   /* time before SIGKILL (default: 3000) */

    /* ─── Runtime state (managed by supervisor) ───────────────────────── */
    service_state_t state;
    pid_t           pid;              /* 0 = not running */
    int             restart_count;    /* current restart attempt count */
    long long       start_time_ms;    /* boot_ms when last started */
    long long       stop_time_ms;     /* boot_ms when last stopped */
    long long       scheduled_start_ms; /* boot_ms when allowed to start (for restart delay) */

    /* Dependency graph indices */
    int  dep_indices[SERVICE_MAX_DEPS];   /* indices into service table */
    int  dep_count;

} service_t;

/* ─── Service table ──────────────────────────────────────────────────────── */

/* Global service table — all services loaded at boot */
extern service_t g_services[SERVICE_MAX_COUNT];
extern int       g_service_count;

/* ─── API ────────────────────────────────────────────────────────────────── */

/*
 * service_load_all() — Load all *.toml files from services_dir.
 *
 * Returns the number of services loaded, or -1 on fatal error.
 * Validates: duplicate names, missing required fields.
 */
int service_load_all(const char *services_dir);

/*
 * service_find() — Look up a service by name.
 *
 * Returns pointer into g_services[], or NULL if not found.
 */
service_t *service_find(const char *name);

/*
 * service_find_by_pid() — Look up a service by its current PID.
 *
 * Returns pointer into g_services[], or NULL if not found.
 */
service_t *service_find_by_pid(pid_t pid);

/*
 * service_state_name() — Human-readable service state string.
 */
const char *service_state_name(service_state_t state);

/*
 * service_load_one() — Parse a single service TOML file into a service_t.
 *
 * Returns 0 on success, -1 on parse error or missing required fields.
 */
int service_load_one(const char *path, service_t *out);

#endif /* MAHINA_SERVICE_H */
