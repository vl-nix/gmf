/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

GIcon * emblemed_icon ( const char *, GIcon * );

GdkPixbuf * file_get_pixbuf ( const char *, gboolean , gboolean , gboolean , int );

void exec ( GFile *, GtkWindow * );

void launch_cmd ( const char *, GtkWindow * );

void launch_app ( GFile *, GAppInfo *, GtkWindow * );

void message_dialog ( const char *, const char *, GtkMessageType , GtkWindow * );
