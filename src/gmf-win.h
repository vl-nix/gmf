/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include "gmf-app.h"

#define GMF_TYPE_WIN gmf_win_get_type ()

G_DECLARE_FINAL_TYPE ( GmfWin, gmf_win, GMF, WIN, GtkWindow )

GmfWin * gmf_win_new ( GFile *, GmfApp * );
