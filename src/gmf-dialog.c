/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-dialog.h"

#include <errno.h>
#include <sys/stat.h>

ulong get_file_size ( const char *file )
{
	long size = 0;

	struct stat sb;

	if ( stat ( file, &sb ) == 0 ) size = sb.st_size;

	return (ulong)size;
}

GIcon * emblemed_icon ( const char *name_1, const char *name_2, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name_1 );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	if ( name_2 )
	{
		GIcon *e_icon_2 = g_themed_icon_new ( name_2 );
		GEmblem *emblem_2  = g_emblem_new ( e_icon_2 );

		g_emblemed_icon_add_emblem ( G_EMBLEMED_ICON ( emblemed ), emblem_2 );

		g_object_unref ( e_icon_2 );
		g_object_unref ( emblem_2 );
	}

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

static inline GtkIconInfo * get_icon_info ( const char *content_type, gboolean is_link, uint16_t icon_size, GFileInfo *finfo )
{
	GtkIconInfo *icon_info = NULL;

	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	GIcon *unknown = NULL, *emblemed = NULL, *gicon = g_file_info_get_icon ( finfo );

	if ( gicon )
	{
		if ( is_link ) emblemed = emblemed_icon ( "emblem-symbolic-link", NULL, gicon );

		icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, ( is_link ) ? emblemed : gicon, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );
	}

	if ( !icon_info && is_link )
	{
		unknown = g_themed_icon_new ( "unknown" );

		if ( emblemed ) g_object_unref ( emblemed );

		if ( content_type && g_str_has_prefix ( content_type, "inode/symlink" ) ) 
			emblemed = emblemed_icon ( "dialog-error", "emblem-symbolic-link", unknown );
		else
			emblemed = emblemed_icon ( "emblem-symbolic-link", NULL, unknown );

		icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, emblemed, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );
	}

	if ( unknown ) g_object_unref ( unknown );
	if ( emblemed ) g_object_unref ( emblemed );

	return icon_info;
}

GdkPixbuf * get_pixbuf ( const char *path, gboolean is_link, uint16_t icon_size )
{
	GdkPixbuf *pixbuf = NULL;

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
	const char *content_type = ( finfo ) ? g_file_info_get_content_type ( finfo ) : NULL;

	if ( finfo )
	{
		GtkIconInfo *icon_info = get_icon_info ( content_type, is_link, icon_size, finfo );

		if ( icon_info ) pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

		if ( icon_info ) g_object_unref ( icon_info );
	}

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	if ( !pixbuf ) pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), ( is_dir ) ? "folder" : "unknown", icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

	return pixbuf;
}

void gmf_dialog_about ( GtkWindow *window )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), window );

	gtk_about_dialog_set_logo_icon_name ( dialog, "system-file-manager" );
	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );
	if ( window && GTK_IS_WINDOW ( window ) )gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	const char *authors[] = { "Stepan Perun", " ", NULL };

	gtk_about_dialog_set_program_name ( dialog, "Gmf" );
	gtk_about_dialog_set_version ( dialog, VERSION );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_3_0 );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/gmf" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2022 Gmf" );
	gtk_about_dialog_set_comments  ( dialog, "File manager" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

void gmf_dialog_message ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, GtkWindow *window )
{
	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new ( window, GTK_DIALOG_MODAL, mesg_type, GTK_BUTTONS_CLOSE, "%s\n%s", f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "system-file-manager" );
	if ( window && GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

char * gmf_dialog_open_dir_file ( const char *path, const char *accept, const char *icon, uint8_t num, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new ( " ", window, num, "gtk-cancel", GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_ACCEPT, NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), icon );
	if ( window && GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), path );
	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	char *filename = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT ) filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return filename;
}

void gmf_dialog_app_chooser ( GFile *file, GtkWindow *window )
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

				list_f = g_list_append ( list_f, file );

				g_app_info_launch ( app_info, list_f, NULL, &error );

				if ( error ) { gmf_dialog_message ( "", error->message, GTK_MESSAGE_WARNING, window ); g_error_free ( error ); }

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

void gmf_launch_cmd ( const char *cmd, GtkWindow *window )
{
	GError *error = NULL;
	GAppInfo *app = g_app_info_create_from_commandline ( cmd, NULL, 0, NULL );

	if ( app ) g_app_info_launch ( app, NULL, NULL, &error );

	if ( error ) { gmf_dialog_message ( "", error->message, GTK_MESSAGE_WARNING, window ); g_error_free ( error ); }

	if ( app ) g_object_unref ( app );
}

static void gmf_launch_app ( GFile *file, GtkWindow *window )
{
	GError *error = NULL;
	GList *list   = NULL;

	list = g_list_append ( list, file );

	GFileInfo *file_info = g_file_query_info ( file, "standard::*", 0, NULL, NULL );

	const char *content_type = ( file_info ) ? g_file_info_get_content_type ( file_info ) : NULL;

	GAppInfo *app_info = ( content_type ) ? g_app_info_get_default_for_type ( content_type, FALSE ) : NULL;

	if ( app_info  ) g_app_info_launch ( app_info, list, NULL, &error );

	if ( error ) { gmf_dialog_message ( "", error->message, GTK_MESSAGE_WARNING, window ); g_error_free ( error ); }

	if ( file_info ) g_object_unref ( file_info );
	if ( app_info  ) g_object_unref ( app_info  );

	g_list_free ( list );
}

void gmf_activated_file ( GFile *file, GtkWindow *window )
{
	GFileInfo *file_info = g_file_query_info ( file, "standard::*", 0, NULL, NULL );

	if ( file_info )
	{
		g_autofree char *path = g_file_get_path ( file );

		gboolean is_exec = g_file_test ( path, G_FILE_TEST_IS_EXECUTABLE );
		const char *content_type = g_file_info_get_content_type ( file_info );

		g_debug ( "%s:: content_type: %s ", __func__, content_type );

		if ( content_type && g_str_equal ( content_type, "application/x-desktop" ) )
		{
			GDesktopAppInfo *d_app = g_desktop_app_info_new_from_filename ( path );
			g_autofree char *cmd = ( d_app ) ? g_desktop_app_info_get_string ( d_app, "Exec" ) : NULL;

			if ( cmd ) gmf_launch_cmd ( cmd, window );

			if ( d_app ) g_object_unref ( d_app );
		}
		else if ( content_type && g_str_equal ( content_type, "application/x-executable" ) )
		{
			if ( is_exec ) gmf_launch_cmd ( path, window );
		}
		else
			gmf_launch_app ( file, window );

		g_object_unref ( file_info );
	}
	else
		gmf_dialog_app_chooser ( file, window );
}

static void gmf_recent_run_item ( GtkRecentChooser *chooser )
{
	GtkRecentInfo *info = gtk_recent_chooser_get_current_item ( chooser );

	if ( info && gtk_recent_info_get_uri (info) )
	{
		GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( chooser ) );

		GFile *file = g_file_new_for_uri ( gtk_recent_info_get_uri (info) );

		gmf_activated_file ( file, GTK_WINDOW ( toplevel ) );

		g_object_unref ( file );

		gtk_recent_info_unref ( info );

		if ( toplevel && GTK_IS_WIDGET ( toplevel ) ) gtk_widget_destroy ( toplevel );
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

void gmf_dialog_recent ( GtkWindow *win_base )
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

	const char *labels[] = { "window-close", "edit-clear", "system-run" };
	const void *funcs[] = { NULL, gmf_recent_clear, gmf_recent_run };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( labels[c], GTK_ICON_SIZE_MENU );

		if ( funcs[c] ) g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), recent );
		if ( c == 0 ) g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	if ( win_base && GTK_IS_WINDOW ( win_base ) ) gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}

static void gmf_trash_treeview_add ( const char *name, const char *path, GFile *file, GtkListStore *store )
{
	GFileInfo *info = g_file_query_info ( file, "*", 0, NULL, NULL );

	GIcon *icon = ( info ) ? g_file_info_get_icon ( info ) : NULL;

	GtkIconInfo *icon_info = ( icon ) ? gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), icon, 16, GTK_ICON_LOOKUP_FORCE_REGULAR ) : NULL;

	GdkPixbuf *pixbuf = ( icon_info ) ? gtk_icon_info_load_icon ( icon_info, NULL ) : gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), "unknown", 16, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

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
					gmf_dialog_message ( "", err->message, GTK_MESSAGE_WARNING, NULL );
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
					gmf_dialog_message ( "", err->message, GTK_MESSAGE_WARNING, NULL );
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
		gmf_dialog_message ( "", error->message, GTK_MESSAGE_WARNING, NULL );
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

void gmf_dialog_trash ( GtkWindow *win_base )
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

	const char *labels[] = { "window-close", "edit-clear", "gtk-undelete" };
	const void *funcs[] = { NULL, gmf_trash_clear, gmf_trash_undelete };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( labels[c], GTK_ICON_SIZE_MENU );

		if ( funcs[c] ) g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), treeview );
		if ( c == 0 ) g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	if ( win_base && GTK_IS_WINDOW ( win_base ) ) gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}

void gmf_dialog_copy_dir_err ( GList *list, GtkWindow *win_base )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_icon_name ( window, "dialog-warning" );
	gtk_window_set_default_size ( window, 500, 350  );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	GtkTextView *text_view = (GtkTextView *)gtk_text_view_new ();
	gtk_text_view_set_editable ( text_view, FALSE );
	gtk_text_view_set_wrap_mode ( text_view, GTK_WRAP_WORD );
	gtk_text_view_set_left_margin ( text_view, 10 );
	gtk_text_view_set_right_margin ( text_view, 10 );
	gtk_widget_set_visible ( GTK_WIDGET ( text_view ), TRUE );

	GtkTextIter iter;
	GtkTextBuffer *buffer = (GtkTextBuffer *)gtk_text_view_get_buffer ( text_view );
	gtk_text_buffer_get_start_iter ( buffer, &iter );

	while ( list != NULL )
	{
		gtk_text_buffer_insert ( buffer, &iter, (char *)list->data, -1 );
		gtk_text_buffer_insert ( buffer, &iter, "\n", -1 );

		list = list->next;
	}

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( text_view ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "window-close", GTK_ICON_SIZE_MENU );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	if ( win_base && GTK_IS_WINDOW ( win_base ) ) gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win_base ) ) );
}
