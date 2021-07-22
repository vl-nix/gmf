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

enum c_num 
{
	C_CP,
	C_MV,
	C_PS,
	C_ALL
};

void gmf_trash_dialog  ( GtkWindow * );
void gmf_recent_dialog ( GtkWindow * );

void gmf_info_dialog ( GFile *, GmfWin * );
void gmf_dfe_dialog  ( enum d_num , GmfWin * );
void gmf_copy_dialog ( const char *, char **, enum c_num, GApplication *app );

void exec ( GFile *, GtkWindow * );
void launch_cmd ( const char *, GtkWindow * );
void launch_app ( GFile *, GAppInfo *, GtkWindow * );

char * gmf_open_dir ( const char *, GtkWindow * );
void gmf_app_chooser_dialog ( GFile *, GtkWindow * );

GtkImage * create_image ( const char *, uint8_t );
GtkButton * button_set_image ( const char *, uint8_t );

GIcon * emblemed_icon ( const char *, GIcon * );
GdkPixbuf * file_get_pixbuf ( const char *, gboolean , gboolean , gboolean , int );

void about ( GtkWindow * );
void message_dialog ( const char *, const char *, GtkMessageType , GtkWindow * );

