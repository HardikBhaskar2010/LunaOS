/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * depgraph.c — Dependency Graph Implementation (Kahn's Algorithm)
 * Authority: Volume II / 04_init_system.md §Dependency Graph Resolution
 */

#include "depgraph.h"
#include "log.h"

#include <stdbool.h>
#include <string.h>

#define COMP "depgraph"

/* Adjacency matrix: edge[i][j] = true means service i must start before j */
static bool g_edge[SERVICE_MAX_COUNT][SERVICE_MAX_COUNT];

/* In-degree array for Kahn's algorithm */
static int g_indegree[SERVICE_MAX_COUNT];

static bool depgraph_has_cycle(void) {
    int queue[SERVICE_MAX_COUNT];
    int q_head = 0;
    int q_tail = 0;
    int indegree[SERVICE_MAX_COUNT];
    int out_count = 0;

    for (int i = 0; i < g_service_count; i++) {
        indegree[i] = g_indegree[i];
        if (indegree[i] == 0) queue[q_tail++] = i;
    }

    while (q_head < q_tail) {
        int node = queue[q_head++];
        out_count++;

        for (int j = 0; j < g_service_count; j++) {
            if (!g_edge[node][j]) continue;
            if (--indegree[j] == 0) {
                queue[q_tail++] = j;
            }
        }
    }

    if (out_count == g_service_count) return false;

    LUNA_FATAL(COMP, "Circular dependency detected in service files!");
    for (int i = 0; i < g_service_count; i++) {
        if (indegree[i] > 0) {
            LUNA_FATAL(COMP, "  Service in cycle: %s", g_services[i].name);
        }
    }
    return true;
}

int depgraph_build(void) {
    /* Zero all state */
    memset(g_edge,     0, sizeof(g_edge));
    memset(g_indegree, 0, sizeof(g_indegree));

    /* Resolve after[] and before[] name references to indices */
    for (int i = 0; i < g_service_count; i++) {
        service_t *svc = &g_services[i];
        svc->dep_count = 0;

        /* after = ["X"] means X must be RUNNING before we start */
        for (int a = 0; a < svc->after_count; a++) {
            service_t *dep = service_find(svc->after[a]);
            if (!dep) {
                LUNA_FATAL(COMP,
                    "Service '%s' depends on '%s' which does not exist",
                    svc->name, svc->after[a]);
                return -1;
            }
            int dep_idx = (int)(dep - g_services);

            /* Edge: dep_idx → i  (dep must start before i) */
            if (!g_edge[dep_idx][i]) {
                g_edge[dep_idx][i] = true;
                g_indegree[i]++;
            }

            /* Store resolved dep index for supervisor */
            if (svc->dep_count < SERVICE_MAX_DEPS) {
                svc->dep_indices[svc->dep_count++] = dep_idx;
            }
        }

        /* before = ["Y"] means we must start before Y */
        for (int b = 0; b < svc->before_count; b++) {
            service_t *target = service_find(svc->before[b]);
            if (!target) {
                LUNA_FATAL(COMP,
                    "Service '%s' declares before '%s' which does not exist",
                    svc->name, svc->before[b]);
                return -1;
            }
            int target_idx = (int)(target - g_services);

            /* Edge: i → target_idx */
            if (!g_edge[i][target_idx]) {
                g_edge[i][target_idx] = true;
                g_indegree[target_idx]++;
            }
        }
    }

    if (depgraph_has_cycle()) return -1;

    LUNA_INFO(COMP, "Dependency graph built for %d services", g_service_count);
    return 0;
}

int depgraph_topo_sort(int *out_order, int max_count) {
    if (!out_order || max_count < g_service_count) return -1;

    /* Kahn's algorithm:
     *   1. Enqueue all nodes with in-degree 0
     *   2. Process queue: emit node, decrement neighbors' in-degree,
     *      enqueue any neighbor whose in-degree reaches 0
     *   3. If all nodes emitted: success. Otherwise: cycle detected.
     */

    int queue[SERVICE_MAX_COUNT];
    int q_head = 0;
    int q_tail = 0;
    int indegree[SERVICE_MAX_COUNT];
    memset(indegree, 0, sizeof(indegree));
    int out_count = 0;

    /* Copy in-degree (Kahn's modifies it) */
    for (int i = 0; i < g_service_count; i++) {
        indegree[i] = g_indegree[i];
    }

    /* Seed queue with all zero-indegree nodes */
    for (int i = 0; i < g_service_count; i++) {
        if (indegree[i] == 0) queue[q_tail++] = i;
    }

    while (q_head < q_tail) {
        int node = queue[q_head++];
        out_order[out_count++] = node;

        /* Decrement in-degree of all successors */
        for (int j = 0; j < g_service_count; j++) {
            if (!g_edge[node][j]) continue;
            if (--indegree[j] == 0) {
                queue[q_tail++] = j;
            }
        }
    }

    if (out_count != g_service_count) {
        /* Cycle detected — which services are involved? */
        LUNA_FATAL(COMP, "Circular dependency detected in service files!");
        for (int i = 0; i < g_service_count; i++) {
            if (indegree[i] > 0) {
                LUNA_FATAL(COMP, "  Service in cycle: %s", g_services[i].name);
            }
        }
        return -1;
    }

    LUNA_INFO(COMP, "Start order resolved (%d services):", out_count);
    for (int i = 0; i < out_count; i++) {
        LUNA_INFO(COMP, "  [%d] %s", i + 1, g_services[out_order[i]].name);
    }

    return out_count;
}
