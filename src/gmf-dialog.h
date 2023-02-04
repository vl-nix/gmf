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

ulong get_file_size ( const char * );

gboolean link_exists ( const char * );
GIcon * emblemed_icon ( const char *, GIcon * );
GdkPixbuf * get_pixbuf ( const char *, gboolean, uint16_t );

void gmf_activated_file ( GFile *, GtkWindow * );
void gmf_launch_cmd ( const char *, GtkWindow * );

void gmf_dialog_trash  ( GtkWindow * );
void gmf_dialog_recent ( GtkWindow * );

void gmf_dialog_about ( GtkWindow * );
void gmf_dialog_app_chooser ( GFile *, GtkWindow * );
void gmf_dialog_copy_dir_err ( GList *, GtkWindow * );
void gmf_dialog_message ( const char *, const char *, GtkMessageType , GtkWindow * );
char * gmf_dialog_open_dir ( const char *, const char *, const char *, uint8_t, GtkWindow * );
