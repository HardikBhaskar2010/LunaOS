/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef MAHINA_CONSOLE_H
#define MAHINA_CONSOLE_H

/*
 * console.h — Console Output and Welcome Message
 * Authority: Volume VII / 01_implementation_roadmap.md §Stage 0 Success Criteria
 *
 * Displays the Mahina welcome message and drops to interactive shell.
 * The welcome message is the Stage 0 success criterion:
 *
 *   Welcome to Mahina.
 *   Architecture Freeze v1
 *   System Initialization Complete.
 */

/*
 * console_print_welcome() — Print the Mahina welcome banner to the console.
 */
void console_print_welcome(void);

/*
 * console_drop_to_shell() — Exec an interactive root shell.
 *
 * Called at the end of Stage 0 boot (drops to busybox sh).
 * This is the final act of luna-init in Stage 0 — in later stages
 * the login manager replaces this path.
 */
void console_drop_to_shell(const char *tty_path);

#endif /* MAHINA_CONSOLE_H */
