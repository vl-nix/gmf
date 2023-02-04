/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

enum info_sel_enm { I_SEL_0, I_SEL_1 };

#define INFO_TYPE_WIN info_win_get_type ()

G_DECLARE_FINAL_TYPE ( InfoWin, info_win, INFO, WIN, GtkWindow )

InfoWin * info_win_new ( enum info_sel_enm, const char *, const char *, double );
