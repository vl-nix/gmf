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

enum cm_num 
{
	CM_CP,
	CM_MV,
	CM_PS,
	CM_ALL
};

#define COPY_TYPE_WIN copy_win_get_type ()

G_DECLARE_FINAL_TYPE ( CopyWin, copy_win, COPY, WIN, GtkWindow )

CopyWin * copy_win_new ( enum cm_num, const char *, char **, GApplication *, double );

