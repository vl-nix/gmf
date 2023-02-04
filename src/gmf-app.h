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

#define GMF_TYPE_APP gmf_app_get_type ()

G_DECLARE_FINAL_TYPE ( GmfApp, gmf_app, GMF, APP, GtkApplication )

GmfApp * gmf_app_new ( void );
