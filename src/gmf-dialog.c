/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-dialog.h"

#include <errno.h>

GIcon * emblemed_icon ( const char *name, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

GdkPixbuf * file_get_pixbuf ( const char *path, gboolean is_dir, gboolean is_link, gboolean preview, int icon_size )
{
	GdkPixbuf *pixbuf = NULL;

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	// g_autofree char *mime_type = g_content_type_guess ( path, NULL, 100, FALSE );
	const char *mime_type = g_file_info_get_content_type ( finfo );
	g_autofree char *icon_name = g_content_type_get_generic_icon_name ( mime_type );

	if ( preview && g_str_has_prefix ( mime_type, "image" ) )
		pixbuf = gdk_pixbuf_new_from_file_at_size ( path, icon_size, icon_size, NULL );

	if ( pixbuf == NULL )
	{
		GtkIconTheme *itheme = gtk_icon_theme_get_default ();

		gboolean dsktp = FALSE, has_icon = gtk_icon_theme_has_icon ( itheme, icon_name );

		if ( is_dir && g_str_has_prefix ( path, g_get_home_dir () ) ) dsktp = TRUE;

		GIcon *emblemed = NULL, *icon_set = NULL;
		icon_set = ( has_icon ) ? ( ( dsktp ) ? g_file_info_get_icon ( finfo ) : g_content_type_get_icon ( mime_type ) ) : g_themed_icon_new ( "text-x-generic" );

		if ( is_link ) emblemed = emblemed_icon ( "emblem-symbolic-link", icon_set );

		GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( itheme, ( is_link ) ? emblemed : icon_set, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE );

		pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

		if ( emblemed ) g_object_unref ( emblemed );
		if ( !dsktp && icon_set ) g_object_unref ( icon_set );
		if ( icon_info ) g_object_unref ( icon_info );
	}

	g_object_unref ( finfo );
	g_object_unref ( file  );

	return pixbuf;
}

void message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, 
		GTK_BUTTONS_CLOSE, "%s\n%s", f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

void launch_cmd ( const char *cmd, GtkWindow *window )
{
	GAppInfo *app = g_app_info_create_from_commandline ( cmd, NULL, 0, NULL );

	if ( app )
	{
		GError *error = NULL;
		g_app_info_launch ( app, NULL, NULL, &error );

		if ( error )
		{
			message_dialog ( "", error->message, GTK_MESSAGE_WARNING, window );
			g_error_free ( error );
		}

		g_object_unref ( app );
	}
}

void launch_app ( GFile *file, GAppInfo *app_info, GtkWindow *window )
{
	GError *error = NULL;
	GList *list   = NULL;

	list = g_list_append ( list, file );

	g_app_info_launch ( app_info, list, NULL, &error );

	if ( error )
	{
		message_dialog ( "", error->message, GTK_MESSAGE_WARNING, window );
		g_error_free ( error );
	}

	g_list_free ( list );
}

void exec ( GFile *file, GtkWindow *window )
{
	GFileInfo *file_info = g_file_query_info ( file, "standard::*", 0, NULL, NULL );

	if ( file_info == NULL ) return;

	GAppInfo *app_info = g_app_info_get_default_for_type ( g_file_info_get_content_type ( file_info ), FALSE );

	if ( app_info ) 
	{
		launch_app ( file, app_info, window );

		g_object_unref ( app_info );
	}

	g_object_unref ( file_info );
}

