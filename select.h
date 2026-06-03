#pragma once

#include "state.h"
#include "scripts.h"

#define SELECT_OK     0
#define SELECT_CANCEL 1
#define SELECT_ERROR  2

// Run the interactive selection loop.
// Drives the STATE_SELECTING -> STATE_SELECTED -> STATE_DONE FSM.
// Returns SELECT_OK, SELECT_CANCEL, or SELECT_ERROR.
int run_selection(void);

// Results after SELECT_OK:
int     select_x(void);
int     select_y(void);
int     select_w(void);
int     select_h(void);
int     select_chosen_script(void);   // -1 = default save, >=0 = index into select_scripts()
Script *select_scripts(void);
int     select_nscripts(void);
