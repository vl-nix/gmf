/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include "gmf-win.h"

enum d_num 
{
	D_DR,
	D_FL,
	D_ED,
	D_ALL
};

void gmf_trash_dialog  ( GtkWindow * );
void gmf_recent_dialog ( GtkWindow * );

void gmf_about ( GtkWindow * );
char * gmf_open_dir ( const char *, GtkWindow * );
void gmf_app_chooser_dialog ( GFile *, GList *, GtkWindow * );
void gmf_dfe_dialog ( const char *, enum d_num , GmfWin * );
void gmf_message_dialog ( const char *, const char *, GtkMessageType , GtkWindow * );

void exec ( GFile *, GtkWindow * );
void launch_cmd ( const char *, GtkWindow * );
void launch_app ( GFile *, GAppInfo *, GtkWindow * );

GtkImage * create_image ( const char *, uint8_t );
GtkButton * button_set_image ( const char *, uint8_t );

ulong get_file_size ( const char * );
gboolean link_exists ( const char * );
GIcon * emblemed_icon ( const char *, GIcon * );
GdkPixbuf * file_get_pixbuf ( const char *, gboolean , gboolean , gboolean , int );

