#pragma once

#include "state.h"
#include "scripts.h"

#define SELECT_OK     0
#define SELECT_CANCEL 1
#define SELECT_ERROR  2

int run_selection(void);

// Results after SELECT_OK:
int     select_x(void);
int     select_y(void);
int     select_w(void);
int     select_h(void);
Action  select_action(void);           // what to do
int     select_script_idx(void);       // valid when action == ACTION_SCRIPT
Script *select_scripts(void);
int     select_nscripts(void);
