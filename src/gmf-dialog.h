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
#include <gio/gdesktopappinfo.h>

ulong get_file_size ( const char * );

void gmf_activated_file ( GFile *, GtkWindow * );
void gmf_launch_cmd ( const char *, GtkWindow * );

GdkPixbuf * get_pixbuf ( const char *, gboolean, uint16_t );
GIcon * emblemed_icon ( const char *, const char *, GIcon * );

void gmf_dialog_trash  ( GtkWindow * );
void gmf_dialog_recent ( GtkWindow * );

void gmf_dialog_about ( GtkWindow * );
void gmf_dialog_app_chooser ( GFile *, GtkWindow * );

void gmf_dialog_copy_dir_err ( GList *, GtkWindow * );
void gmf_dialog_message ( const char *, const char *, GtkMessageType , GtkWindow * );
char * gmf_dialog_open_dir_file ( const char *, const char *, const char *, uint8_t, GtkWindow * );
