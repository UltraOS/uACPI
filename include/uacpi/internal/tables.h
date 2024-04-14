#pragma once

#include <uacpi/internal/context.h>
#include <uacpi/types.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

uacpi_status uacpi_initialize_tables(void);

uacpi_status uacpi_table_append(uacpi_phys_addr addr,
                                struct uacpi_table **out_table);

uacpi_status
uacpi_table_append_mapped(uacpi_virt_addr virt_addr,
                          struct uacpi_table **out_table);

enum uacpi_table_iteration_decision {
    UACPI_TABLE_ITERATION_DECISION_CONTINUE,
    UACPI_TABLE_ITERATION_DECISION_BREAK,
};
typedef enum uacpi_table_iteration_decision
    (*uacpi_table_iteration_callback)(void *user, uacpi_table *tbl);

void uacpi_for_each_table(
    uacpi_size base_idx, uacpi_table_iteration_callback, void *user
);
