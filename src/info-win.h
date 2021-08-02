/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

enum i_num 
{
	I_SEL_0,
	I_SEL_1,
	I_SEL_N,
	I_ALL
};

#define INFO_TYPE_WIN info_win_get_type ()

G_DECLARE_FINAL_TYPE ( InfoWin, info_win, INFO, WIN, GtkWindow )

InfoWin * info_win_new ( enum i_num, const char *, GList *, GApplication *, double );

