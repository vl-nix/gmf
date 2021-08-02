/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-dialog.h"

#include <errno.h>
#include <sys/stat.h>
#include <glib/gstdio.h>

ulong get_file_size ( const char *file )
{
	long size = 0;

	struct stat sb;

	if ( stat ( file, &sb ) == 0 ) size = sb.st_size;

	return (ulong)size;
}

gboolean link_exists ( const char *path )
{
	gboolean link_exists = FALSE;

	g_autofree char *link = g_file_read_link ( path, NULL );

	if ( !link ) return FALSE;

	if ( !g_path_is_absolute ( link ) )
	{
		g_autofree char *dirname = g_path_get_dirname ( path );
		g_autofree char *real = g_build_filename ( dirname, link, NULL );

		link_exists = g_file_test ( real, G_FILE_TEST_EXISTS );
	}
	else
		link_exists = g_file_test ( link, G_FILE_TEST_EXISTS );

	return link_exists;
}

GIcon * emblemed_icon ( const char *name, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

static gboolean get_icon_names ( GIcon *icon )
{
	const char **names = g_themed_icon_get_names ( G_THEMED_ICON ( icon ) );

	gboolean has_icon = gtk_icon_theme_has_icon ( gtk_icon_theme_get_default (), names[0] );

	if ( !has_icon && names[1] != NULL ) has_icon = gtk_icon_theme_has_icon ( gtk_icon_theme_get_default (), names[1] );

	return has_icon;
}

GdkPixbuf * file_get_pixbuf ( const char *path, gboolean is_dir, gboolean is_link, gboolean preview, int icon_size )
{
	g_return_val_if_fail ( path != NULL, NULL );

	if ( !g_file_test ( path, G_FILE_TEST_EXISTS ) ) return NULL;

	GdkPixbuf *pixbuf = NULL;

	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	const char *mime_type = ( finfo ) ? g_file_info_get_content_type ( finfo ) : NULL;

	if ( mime_type && preview && g_str_has_prefix ( mime_type, "image" ) )
		pixbuf = gdk_pixbuf_new_from_file_at_size ( path, icon_size, icon_size, NULL );

	if ( pixbuf == NULL )
	{
		gboolean is_link_exist = ( is_link ) ? link_exists ( path ) : FALSE;

		GIcon *emblemed = NULL, *icon_set = ( finfo ) ? g_file_info_get_icon ( finfo ) : NULL;

		gboolean has_icon = ( icon_set ) ? get_icon_names ( icon_set ) : FALSE;

		if ( !has_icon ) icon_set = g_themed_icon_new ( ( is_dir ) ? "folder" : "unknown" );

		if ( is_link ) emblemed = emblemed_icon ( ( !is_link_exist ) ? "error" : "emblem-symbolic-link", icon_set );

		GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( itheme, ( is_link ) ? emblemed : icon_set, icon_size, GTK_ICON_LOOKUP_FORCE_SIZE );

		pixbuf = ( icon_info ) ? gtk_icon_info_load_icon ( icon_info, NULL ) : NULL;

		if ( emblemed ) g_object_unref ( emblemed );
		if ( !has_icon && icon_set ) g_object_unref ( icon_set );
		if ( icon_info ) g_object_unref ( icon_info );
	}

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	return pixbuf;
}

GtkImage * create_image ( const char *icon, uint8_t size )
{
	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), icon, size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL );

	GtkImage *image   = (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf );
	gtk_image_set_pixel_size ( image, size );

	if ( pixbuf ) g_object_unref ( pixbuf );

	return image;
}

GtkButton * button_set_image ( const char *icon, uint8_t size )
{
	GtkButton *button = (GtkButton *)gtk_button_new ();

	GtkImage *image   = create_image ( icon, size );
	gtk_button_set_image ( button, GTK_WIDGET ( image ) );

	return button;
}

void gmf_about ( GtkWindow *window )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), window );

	gtk_about_dialog_set_logo_icon_name ( dialog, "system-file-manager" );
	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );
	gtk_widget_set_opacity   ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	const char *authors[] = { "Stepan Perun", " ", NULL };

	gtk_about_dialog_set_program_name ( dialog, "Gmf" );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_3_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/gmf" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2021 Gmf" );
	gtk_about_dialog_set_comments  ( dialog, "File manager" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

void gmf_message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
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
			gmf_message_dialog ( "", error->message, GTK_MESSAGE_WARNING, window );
			g_error_free ( error );
		}

		g_object_unref ( app );
	}
}

void launch_app ( GFile *file, GAppInfo *appi, GtkWindow *window )
{
	GError *error = NULL;
	GList *list   = NULL;

	list = g_list_append ( list, file );

	g_app_info_launch ( appi, list, NULL, &error );

	if ( error )
	{
		gmf_message_dialog ( "", error->message, GTK_MESSAGE_WARNING, window );
		g_error_free ( error );
	}

	g_list_free ( list );
}

static void gmf_win_run_exec  ( GtkButton *button, char *path ) 
{
	GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( button ) );

	launch_cmd ( path, GTK_WINDOW ( toplevel ) );
}

static void gmf_win_run_trml  ( GtkButton *button, char *path ) 
{
	g_autofree char *pwd = g_get_current_dir ();
	g_autofree char *dir = g_path_get_dirname ( path );
	g_autofree char *cmd = g_strconcat ( "/usr/bin/x-terminal-emulator -e ", path, NULL );

	g_chdir ( dir );

	GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( button ) );
	
	launch_cmd ( cmd, GTK_WINDOW ( toplevel ) );

	g_chdir ( pwd );
}

static void gmf_win_run_edit ( GtkButton *button, char *path ) 
{
	GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( button ) );

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *file_info = g_file_query_info ( file, "standard::*", 0, NULL, NULL );

	const char *mime_type = ( file_info ) ? g_file_info_get_content_type ( file_info ) : NULL;

	GAppInfo *app_info = ( mime_type ) ? g_app_info_get_default_for_type ( mime_type, FALSE ) : NULL;

	launch_app ( file, app_info, GTK_WINDOW ( toplevel ) );

	g_object_unref ( file );
	if ( file_info ) g_object_unref ( file_info );
	if ( app_info  ) g_object_unref ( app_info  );
}

static void gmf_win_run_destroy ( G_GNUC_UNUSED GtkWindow *window, char *path )
{
	free ( path );
}

static void exec_dialog ( GFile *file, gboolean edit, GtkWindow *win_base )
{
	char *path = g_file_get_path ( file );

	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	if ( GTK_IS_WINDOW ( win_base ) ) gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_icon_name ( window, "system-run" );
	gtk_window_set_default_size ( window, 300, -1 );
	g_signal_connect ( window, "destroy", G_CALLBACK ( gmf_win_run_destroy ), path );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	uint8_t c = 0, n = ( edit ) ? 0 : 1;
	const char *labels[] = { "gtk-edit", "terminal", "system-run", "window-close" };
	const void *funcs[]  = { gmf_win_run_edit, gmf_win_run_trml, gmf_win_run_exec, NULL };

	for ( c = n; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = button_set_image ( labels[c], 16 );

		if ( funcs[c] ) g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), path );
		g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	if ( GTK_IS_WINDOW ( win_base ) ) gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}

void exec ( GFile *file, GtkWindow *window )
{
	GFileInfo *file_info = g_file_query_info ( file, "standard::*", 0, NULL, NULL );

	if ( file_info == NULL )
	{
		g_autofree char *path = g_file_get_path ( file );

		if ( g_file_test ( path, G_FILE_TEST_IS_EXECUTABLE ) )
			exec_dialog ( file, FALSE, window );
		else
			gmf_app_chooser_dialog ( file, NULL, window );

		return;
	}

	const char *mime_type = ( file_info ) ? g_file_info_get_content_type ( file_info ) : NULL;

	if ( mime_type && ( g_str_has_suffix ( mime_type, "desktop" ) || g_str_has_suffix ( mime_type, "script" )
		|| g_str_has_suffix ( mime_type, "perl" ) || g_str_has_suffix ( mime_type, "python" ) ) )
	{
		exec_dialog ( file, TRUE, window );

		g_object_unref ( file_info );

		return;
	}

	GAppInfo *app_info = ( mime_type ) ? g_app_info_get_default_for_type ( mime_type, FALSE ) : NULL;

	if ( app_info ) 
	{
		launch_app ( file, app_info, window );

		g_object_unref ( app_info );
	}
	else
	{
		g_autofree char *path = g_file_get_path ( file );

		if ( g_file_test ( path, G_FILE_TEST_IS_EXECUTABLE ) )
			exec_dialog ( file, FALSE, window );
		else
			gmf_app_chooser_dialog ( file, NULL, window );
	}

	g_object_unref ( file_info );
}

char * gmf_open_dir ( const char *path, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = (GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
		" ",  window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		"gtk-cancel", GTK_RESPONSE_CANCEL,
		"gtk-apply",  GTK_RESPONSE_ACCEPT,
		NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "folder-open" );
	if ( GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), path );

	char *dirname = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		dirname = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return dirname;
}

void gmf_app_chooser_dialog ( GFile *file, GList *list, GtkWindow *window )
{
	GAppInfo *app_info = NULL;
	GtkAppChooserDialog *dialog = (GtkAppChooserDialog *)gtk_app_chooser_dialog_new ( window, GTK_DIALOG_MODAL, file );
	if ( window && GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	int result = gtk_dialog_run ( GTK_DIALOG (dialog) );

	switch ( result )
	{
		case GTK_RESPONSE_OK:
		{
			app_info = gtk_app_chooser_get_app_info ( GTK_APP_CHOOSER ( dialog ) );

			if ( app_info ) 
			{
				GError *error = NULL;
				GList *list_f = NULL;

				if ( list )
				{
					while ( list != NULL )
					{
						list_f = g_list_append ( list_f, g_file_new_for_path ( (char *)list->data ) );

						list = list->next;
					}
				}
				else
					list_f = g_list_append ( list_f, file );

				g_app_info_launch ( app_info, list_f, NULL, &error );

				if ( error )
				{
					gmf_message_dialog ( "", error->message, GTK_MESSAGE_WARNING, window );
					g_error_free ( error );
				}

				if ( list )
					g_list_free_full ( list_f, (GDestroyNotify) g_object_unref );
				else
					g_list_free ( list_f );

				g_object_unref ( app_info );
			}

			break;
		}
        
		default:
			break;
	}

	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static void gmf_recent_run_item ( GtkRecentChooser *chooser )
{
	GtkRecentInfo *info = gtk_recent_chooser_get_current_item ( chooser );

	if ( info && gtk_recent_info_get_uri (info) )
	{
		GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( chooser ) );

		GFile *file = g_file_new_for_uri ( gtk_recent_info_get_uri (info) );

		exec ( file, GTK_WINDOW ( toplevel ) );

		g_object_unref ( file );

		gtk_recent_info_unref ( info );
	}
}

static void gmf_recent_item_activated ( GtkRecentChooser *chooser, G_GNUC_UNUSED gpointer data )
{
	gmf_recent_run_item ( chooser );
}

static void gmf_recent_run ( G_GNUC_UNUSED GtkButton *button, GtkRecentChooser *chooser )
{
	gmf_recent_run_item ( chooser );
}

static void gmf_recent_clear ( void )
{
	GtkRecentManager *mn = gtk_recent_manager_new ();

	gtk_recent_manager_purge_items ( mn, NULL );

	g_object_unref ( mn );
}

void gmf_recent_dialog ( GtkWindow *win_base )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_icon_name ( window, "document-open-recent" );
	gtk_window_set_default_size ( window, 250, 350  );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkRecentChooserWidget *recent = (GtkRecentChooserWidget *)gtk_recent_chooser_widget_new ();
	g_signal_connect ( recent, "item-activated", G_CALLBACK ( gmf_recent_item_activated ), recent );
	gtk_widget_set_visible ( GTK_WIDGET ( recent ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( recent ), TRUE, TRUE, 0 );

	const char *labels[] = { "window-close" , "edit-clear", "system-run" };
	const void *funcs[] = { gtk_widget_destroy, gmf_recent_clear, gmf_recent_run };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = button_set_image ( labels[c], 16 );

		( c == 0 ) ? g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window )
			   : g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), recent );

		if ( c == 2 ) g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}

static void gmf_trash_treeview_add ( const char *name, const char *path, GFile *file, GtkListStore *store )
{
	GFileInfo *info = g_file_query_info ( file, "*", 0, NULL, NULL );

	GIcon *icon = ( info ) ? g_file_info_get_icon ( info ) : NULL;

	GtkIconInfo *icon_info = ( icon ) ? gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), icon, 16, GTK_ICON_LOOKUP_FORCE_REGULAR ) : NULL;

	GdkPixbuf *pixbuf = ( icon_info ) ? gtk_icon_info_load_icon ( icon_info, NULL )
					  : gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), "unknown", 16, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

	GtkTreeIter iter;
	gtk_list_store_append ( store, &iter );
	gtk_list_store_set    ( store, &iter, 0, pixbuf, 1, name, 2, path, -1 );

	if ( pixbuf ) g_object_unref ( pixbuf );
	if ( icon_info ) g_object_unref ( icon_info );
	if ( info ) g_object_unref ( info );
}

static gboolean gmf_trash_files ( gboolean add, gboolean clear, gboolean undelete, const char *name_und, const char *path_und, GtkListStore *store )
{
	gboolean ret = FALSE;

	GFile *trash = g_file_new_for_uri ("trash:///");

	GError *error = NULL;
	GFileEnumerator *enumerator = g_file_enumerate_children ( trash,
		G_FILE_ATTRIBUTE_STANDARD_NAME ","
		G_FILE_ATTRIBUTE_TRASH_DELETION_DATE ","
		G_FILE_ATTRIBUTE_TRASH_ORIG_PATH,
		G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
		NULL, &error );

	if ( enumerator )
	{
		GFileInfo *info;
		const char *path = NULL, *name = NULL;

		while ( ( info = g_file_enumerator_next_file ( enumerator, NULL, &error ) ) != NULL )
		{
			path = g_file_info_get_attribute_byte_string ( info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH );
			name = g_file_info_get_name ( info );

			GFile *dest  = g_file_new_for_path ( path );
			char *b_name = g_file_get_basename ( dest );

			GFile *file = g_file_get_child ( trash, name );

			if ( add )
			{
				gmf_trash_treeview_add ( b_name, path, file, store );
			}

			if ( undelete && g_str_equal ( b_name, name_und ) && g_str_equal ( path, path_und ) )
			{
				GError *err = NULL;
				g_file_move ( file, dest, G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL, NULL, NULL, &err );

				if ( err != NULL )
				{
					ret = TRUE;
					gmf_message_dialog ( "", err->message, GTK_MESSAGE_WARNING, NULL );
					g_error_free ( err );
				}
			}

			if ( clear )
			{
				GError *err = NULL;
				g_file_delete ( file, NULL, &err );

				if ( err != NULL )
				{
					ret = TRUE;
					gmf_message_dialog ( "", err->message, GTK_MESSAGE_WARNING, NULL );
					g_error_free ( err );
				}
			}

			free ( b_name );
			g_object_unref ( dest );
			g_object_unref ( file );
			if ( info ) g_object_unref ( info );
		}

		g_file_enumerator_close ( enumerator, FALSE, NULL );
		g_object_unref ( enumerator );
	}

	g_object_unref ( trash );

	if ( error != NULL )
	{
		ret = TRUE;
		gmf_message_dialog ( "", error->message, GTK_MESSAGE_WARNING, NULL );
		g_error_free ( error );
	}

	return ret;
}

static void gmf_trash_undelete ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *treeview )
{
	GtkTreeIter iter;
	GtkListStore *store = GTK_LIST_STORE ( gtk_tree_view_get_model ( treeview ) );

	if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( treeview ), NULL, &iter ) )
	{
		g_autofree char *name = NULL;
		g_autofree char *path = NULL;
		gtk_tree_model_get ( gtk_tree_view_get_model ( treeview ), &iter, 1, &name, 2, &path, -1 );

		gboolean err = gmf_trash_files ( FALSE, FALSE, TRUE, name, path, store );

		if ( !err ) gtk_list_store_remove ( store, &iter );
	}
}

static void gmf_trash_clear ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *tree_view )
{
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( !ind ) return;

	gboolean err = gmf_trash_files ( FALSE, TRUE, FALSE, NULL, NULL, GTK_LIST_STORE ( model ) );

	if ( !err ) gtk_list_store_clear ( GTK_LIST_STORE ( model ) );
}

static GtkTreeView * gmf_trash_create_treeview ( void )
{
	GtkListStore *store = gtk_list_store_new ( 3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING );

	GtkTreeView *treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	gtk_tree_view_set_headers_visible ( treeview, FALSE );
	gtk_widget_set_visible ( GTK_WIDGET ( treeview ), TRUE );

	GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new ();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ( "Icon", renderer,  "pixbuf", 0, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "File", renderer,  "text", 1, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "Path", renderer,  "text", 2, NULL );
	gtk_tree_view_append_column ( treeview, column );
	gtk_tree_view_column_set_visible ( column, FALSE );

	gmf_trash_files ( TRUE, FALSE, FALSE, NULL, NULL, store );

	g_object_unref ( store );

	return treeview;
}

void gmf_trash_dialog ( GtkWindow *win_base )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_icon_name ( window, "user-trash-full" );
	gtk_window_set_default_size ( window, 250, 350  );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	GtkTreeView *treeview = gmf_trash_create_treeview ();

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( treeview ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	const char *labels[] = { "window-close" , "edit-clear", "gtk-undelete" };
	const void *funcs[] = { gtk_widget_destroy, gmf_trash_clear, gmf_trash_undelete };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = button_set_image ( labels[c], 16 );

		( c == 0 ) ? g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( funcs[c] ), window )
			   : g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), treeview );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}

const char *d_icons[D_ALL][2] = 
{
	[D_DR] = { "folder-new",   "New-folder"   },
	[D_FL] = { "document-new", "New-document" },
	[D_ED] = { "gtk-edit",     "New-name"     }
};

static void gmf_dfe_dialog_apply_entry ( GtkEntry *entry, GmfWin *win )
{
	const char *text = gtk_entry_get_text ( entry );
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( entry ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	g_signal_emit_by_name ( win, "win-apply-dfe", num, text );

	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

	gtk_widget_destroy ( GTK_WIDGET ( window ) );
}

static void gmf_dfe_dialog_icon_press_entry ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, GmfWin *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY   ) gtk_entry_set_text ( entry, "" );
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY ) gmf_dfe_dialog_apply_entry ( entry, win );
}

static void gmf_dfe_dialog_activate_entry ( GtkEntry *entry, GmfWin *win )
{
	gmf_dfe_dialog_apply_entry ( entry, win );
}

static GtkEntry * gmf_dfe_dialog_create_entry ( enum d_num num, const char *text, GmfWin *win )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );

	char buf[10];
	sprintf ( buf, "%u", num );
	gtk_widget_set_name ( GTK_WIDGET ( entry ), buf );

	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, "edit-clear" );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "dialog-apply" );

	g_signal_connect ( entry, "activate",   G_CALLBACK ( gmf_dfe_dialog_activate_entry   ), win );
	g_signal_connect ( entry, "icon-press", G_CALLBACK ( gmf_dfe_dialog_icon_press_entry ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

void gmf_dfe_dialog ( const char *name, enum d_num num, GmfWin *win )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, GTK_WINDOW ( win ) );
	gtk_window_set_icon_name ( window, d_icons[num][0] );
	gtk_window_set_default_size ( window, 400, -1 );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER_ON_PARENT );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkEntry *entry = gmf_dfe_dialog_create_entry ( num, ( name ) ? name : d_icons[num][1], win );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( entry ), TRUE, TRUE, 0 );

	GtkButton *button = button_set_image ( "window-close", 16 );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), FALSE, FALSE, 0 );

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win ) ) );
}

