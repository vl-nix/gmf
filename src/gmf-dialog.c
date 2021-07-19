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
#include <sys/stat.h>

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
		icon_set = ( has_icon ) ? ( ( dsktp ) ? g_file_info_get_icon ( finfo ) : g_content_type_get_icon ( mime_type ) ) 
					: g_themed_icon_new ( ( is_dir ) ? "folder" : "unknown" );

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

void about ( GtkWindow *window )
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

void gmf_app_chooser_dialog ( GFile *file, GtkWindow *window )
{
	GAppInfo *app_info;
	GtkAppChooserDialog *dialog = (GtkAppChooserDialog *)gtk_app_chooser_dialog_new ( window, GTK_DIALOG_MODAL, file );

	if ( GTK_IS_WINDOW ( window ) ) gtk_widget_set_opacity ( GTK_WIDGET ( dialog ), gtk_widget_get_opacity ( GTK_WIDGET ( window ) ) );
	int result = gtk_dialog_run ( GTK_DIALOG (dialog) );

	switch ( result )
	{
		case GTK_RESPONSE_OK:
		{
			app_info = gtk_app_chooser_get_app_info ( GTK_APP_CHOOSER ( dialog ) );

			if ( app_info ) 
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
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	GFileInfo *info = g_file_query_info ( file, "*", 0, NULL, NULL );

	const char *mime_type = g_file_info_get_content_type ( info );
	g_autofree char *icon_name = g_content_type_get_generic_icon_name ( mime_type );

	gboolean has_icon = gtk_icon_theme_has_icon ( icon_theme, icon_name );
	GIcon *icon = ( has_icon ) ? g_content_type_get_icon ( mime_type ) : g_themed_icon_new ( ( info ) ? "text-x-generic" : "unknown" );

	GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, icon, 16, GTK_ICON_LOOKUP_FORCE_SIZE );

	GdkPixbuf *pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

	GtkTreeIter iter;
	gtk_list_store_append ( store, &iter );
	gtk_list_store_set    ( store, &iter, 0, pixbuf, 1, name, 2, path, -1 );

	g_object_unref ( icon );
	g_object_unref ( info );
	g_object_unref ( icon_info );
	if ( pixbuf ) g_object_unref ( pixbuf );
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
					message_dialog ( "", err->message, GTK_MESSAGE_WARNING, NULL );
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
					message_dialog ( "", err->message, GTK_MESSAGE_WARNING, NULL );
					g_error_free ( err );
				}
			}

			free ( b_name );
			g_object_unref ( dest );
			g_object_unref ( file );
			g_object_unref ( info );
		}

		g_file_enumerator_close ( enumerator, FALSE, NULL );
		g_object_unref ( enumerator );
	}

	g_object_unref ( trash );

	if ( error != NULL )
	{
		ret = TRUE;
		message_dialog ( "", error->message, GTK_MESSAGE_WARNING, NULL );
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

void gmf_dfe_dialog ( enum d_num num, GmfWin *win )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, GTK_WINDOW ( win ) );
	gtk_window_set_icon_name ( window, d_icons[num][0] );
	gtk_window_set_default_size ( window, 400, -1 );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkEntry *entry = gmf_dfe_dialog_create_entry ( num, d_icons[num][1], win );
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

static uint info_get_file_mode ( const char *file )
{
	uint mode = 0;

	struct stat sb;

	if ( stat ( file, &sb ) == 0 ) mode = sb.st_mode;

	return mode;
}

static uint info_get_file_mode_acces ( const char *file, const char *acces )
{
	mode_t mode = info_get_file_mode ( file );

	uint ret = 3;

	if ( g_str_has_prefix ( acces, "owner" ) )
	{
		ret = 2;
		if ( ( mode & S_IRUSR ) == S_IRUSR ) ret = 0;
		if ( ( mode & S_IWUSR ) == S_IWUSR ) ret = 1;
	}
	if ( g_str_has_prefix ( acces, "group" ) )
	{
		ret = 2;
		if ( ( mode & S_IRGRP ) == S_IRGRP ) ret = 0;
		if ( ( mode & S_IWGRP ) == S_IWGRP ) ret = 1;
	}
	if ( g_str_has_prefix ( acces, "other" ) )
	{
		ret = 2;
		if ( ( mode & S_IROTH ) == S_IROTH ) ret = 0;
		if ( ( mode & S_IWOTH ) == S_IWOTH ) ret = 1;
	}
	if ( g_str_has_prefix ( acces, "exec" ) )
	{
		ret = 3;
		if ( ( mode & S_IXUSR ) == S_IXUSR ) ret = 0;
		if ( ( mode & S_IXGRP ) == S_IXGRP ) ret = 1;
		if ( ( mode & S_IXOTH ) == S_IXOTH ) ret = 2;
	}

	return ret;
}

static char * info_file_time_to_str ( uint64_t time )
{
	GDateTime *date = g_date_time_new_from_unix_local ( (int64_t)time );

	char *str_time = g_date_time_format ( date, "%a; %e-%b-%Y %T %z" );

	g_date_time_unref ( date );

	return str_time;
}

static GtkEntry * gmf_info_dialog_create_entry ( const char *text )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );
	gtk_editable_set_editable ( GTK_EDITABLE (entry), FALSE );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

static GtkImage * gmf_info_dialog_create_image ( GdkPixbuf *pixbuf, const char *icon )
{
	GtkImage *image = ( pixbuf ) 
		? (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf ) 
		: (GtkImage *)gtk_image_new_from_icon_name ( icon, GTK_ICON_SIZE_LARGE_TOOLBAR );

	gtk_widget_set_halign ( GTK_WIDGET ( image ), GTK_ALIGN_START );

	gtk_widget_set_visible ( GTK_WIDGET ( image ), TRUE );

	return image;
}

static GtkLabel * gmf_info_dialog_create_label ( const char *text, uint8_t alg )
{
	GtkLabel *label = (GtkLabel *)gtk_label_new ( text );
	
	gtk_widget_set_halign ( GTK_WIDGET ( label ), alg );

	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );

	return label;
}

void gmf_info_dialog ( GFile *file, GmfWin *win )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_default_size ( window, 400, -1 );
	gtk_window_set_transient_for ( window, GTK_WINDOW ( win ) );
	gtk_window_set_icon_name ( window, "system-file-manager" );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );

	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	g_autofree char *path = g_file_get_path ( file );
	g_autofree char *name = g_file_get_basename ( file );
	g_autofree char *dir  = g_path_get_dirname  ( path );

	gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
	gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

	GtkBox *m_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_set_border_width ( GTK_CONTAINER (m_box), 10 );

	gtk_box_set_spacing ( m_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( m_box ), TRUE );

	GtkEntry *entry = gmf_info_dialog_create_entry ( name );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, TRUE, 0 );

	entry = gmf_info_dialog_create_entry ( dir );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, TRUE, 0 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	const char *mime_type = g_file_info_get_content_type ( finfo );
	g_autofree char *description = g_content_type_get_description ( mime_type );
	g_autofree char *icon_name = g_content_type_get_generic_icon_name ( mime_type );

	GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_slk, FALSE, 48 );

	GtkImage *image = gmf_info_dialog_create_image ( pixbuf, "unknown" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	uint64_t fsize = (uint64_t)g_file_info_get_size ( finfo );
	g_autofree char *str_fsize = g_format_size ( fsize );

	GtkLabel *label_size = gmf_info_dialog_create_label ( ( is_dir ) ? "None" : str_fsize, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label_size ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET (h_box), FALSE, FALSE, 0 );

	GtkLabel *label = gmf_info_dialog_create_label ( description, GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	label = gmf_info_dialog_create_label ( " ", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 5 );

	const char *icon_name_a[] = { "text-x-preview", "gtk-edit", "locked" };
	const char *icon_name_b[] = { "owner", "group", "other", "exec" };
	const char *icon_name_c[] = { "avatar-default", "system-users", "system-users", "system-run" };
	const char *icon_name_d[] = { "avatar-default", "system-users", "system-users", "locked" };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( icon_name_c ); c++ )
	{
		h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
		gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

		image = gmf_info_dialog_create_image ( NULL, icon_name_c[c] );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

		uint m = info_get_file_mode_acces ( path, icon_name_b[c] );
		image = gmf_info_dialog_create_image ( NULL, ( c == 3 ) ? icon_name_d[m] : icon_name_a[m] );
		gtk_box_pack_end ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

		gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
	}

	label = gmf_info_dialog_create_label ( " ", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	uint64_t atime = g_file_info_get_attribute_uint64 ( finfo, "time::access" );
	uint64_t mtime = g_file_info_get_attribute_uint64 ( finfo, "time::modified" );

	g_autofree char *str_atime = info_file_time_to_str ( atime );
	g_autofree char *str_mtime = info_file_time_to_str ( mtime );

	image = gmf_info_dialog_create_image ( NULL, "text-x-preview" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	label = gmf_info_dialog_create_label ( str_atime, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	image = gmf_info_dialog_create_image ( NULL, "gtk-edit" );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	label = gmf_info_dialog_create_label ( str_mtime, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	label = gmf_info_dialog_create_label ( " ", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 5 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkButton *button = button_set_image ( "window-close", 16 );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

	gtk_box_pack_end ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( m_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win ) ) );

	if ( finfo  ) g_object_unref ( finfo  );
	if ( pixbuf ) g_object_unref ( pixbuf );
}

const char *c_icons[C_ALL][2] = 
{
	[C_CP] = { "edit-copy",  "Copy"  },
	[C_MV] = { "edit-cut",   "Cut"   },
	[C_PS] = { "edit-paste", "Paste" }
};

typedef struct _CopyData CopyData;

struct _CopyData
{
	enum c_num cp_mv;

	GMutex mutex;
	GCancellable *cancellable;

	char **list;
	char *path_dest;

	uint16_t indx;
	uint8_t prg[UINT16_MAX];

	uint16_t indx_set;
	uint8_t prg_set[UINT16_MAX];
	char *list_error[UINT16_MAX];

	gboolean copy_done;
};

typedef struct _CopyWin CopyWin;

struct _CopyWin
{
	enum c_num cp_mv;

	GtkEntry *entry;
	GtkTreeView *treeview;

	CopyData *job;

	gboolean exit;
};

static void gmf_copy_file_progress ( int64_t current, int64_t total, gpointer data )
{
	CopyData *job = (CopyData *)data;

	uint8_t pr = ( total ) ? ( uint8_t )( current * 100 / total ) : 0;

	g_mutex_lock ( &job->mutex );

	job->indx_set = job->indx;
	job->prg[job->indx] = pr;

	g_mutex_unlock ( &job->mutex );
}

static gpointer gmf_copy_paste_thread ( CopyData *job )
{
	g_mutex_init ( &job->mutex );

	gboolean w_break = FALSE;

	job->indx = 0;

	while ( job->list[job->indx] != NULL )
	{
		if ( job->indx >= UINT16_MAX ) break;

		g_mutex_lock ( &job->mutex );
			w_break = job->copy_done;
		g_mutex_unlock ( &job->mutex );

		if ( w_break ) { free ( job->list[job->indx++] ); continue; }

		char *path = job->list[job->indx];
		char *name = g_path_get_basename ( path );

		GError *error = NULL;
		GFile *file_src  = g_file_new_for_path ( path );
		GFile *file_dest = g_file_new_build_filename ( job->path_dest, name, NULL );

		if ( job->cp_mv == C_MV )
			g_file_move ( file_src, file_dest, G_FILE_COPY_NOFOLLOW_SYMLINKS, job->cancellable, gmf_copy_file_progress, job, &error );
		else
			g_file_copy ( file_src, file_dest, G_FILE_COPY_NOFOLLOW_SYMLINKS, job->cancellable, gmf_copy_file_progress, job, &error );

		g_object_unref ( file_dest );
		g_object_unref ( file_src  );

		free ( name );
		free ( path );

		if ( error )
		{
			g_mutex_lock ( &job->mutex );
				job->list_error[job->indx] = g_strdup ( error->message );
			g_mutex_unlock ( &job->mutex );

			g_message ( "%s:: error: %s ", __func__, error->message );
			g_error_free ( error );

			g_mutex_lock ( &job->mutex );
				job->indx_set = job->indx;
			g_mutex_unlock ( &job->mutex );
		}

		job->indx++;
	}

	g_mutex_lock ( &job->mutex );
		job->copy_done = TRUE;
	g_mutex_unlock ( &job->mutex );

	free ( job->list );
	free ( job->path_dest );

	g_mutex_clear ( &job->mutex );

	usleep ( 250000 );

	uint16_t n = 0; for ( n = 0; n < job->indx; n++ ) if ( job->list_error[n] ) { free ( job->list_error[n] ); job->list_error[n] = NULL; }

	return NULL;
}

static void gmf_copy_paste_update_prg ( uint16_t indx, CopyWin *cwin )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( cwin->treeview );

	GtkTreePath *tree_path = gtk_tree_path_new_from_indices ( indx, -1 );

	if ( gtk_tree_model_get_iter ( model, &iter, tree_path ) )
	{
		uint ind = 0;
		gtk_tree_model_get ( model, &iter, 3, &ind, -1 );

		cwin->job->prg_set[indx] = cwin->job->prg[indx];

		if ( cwin->job->list_error[indx] )
			gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 4, cwin->job->list_error[indx], -1 );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 3, cwin->job->prg[indx], -1 );
	}

	if ( tree_path ) gtk_tree_path_free ( tree_path );
}

static void gmf_copy_paste_update_all ( gboolean all, CopyWin *cwin )
{
	uint16_t n = 0; for ( n = 0; n < cwin->job->indx_set + 1; n++ )
	{
		if ( cwin->exit ) return;

		if ( all ) gmf_copy_paste_update_prg ( n, cwin );
		else if ( cwin->job->prg_set[n] < 100 ) gmf_copy_paste_update_prg ( n, cwin );
	}
}

static gboolean gmf_copy_paste_update_timeout ( CopyWin *cwin )
{
	if ( cwin->exit ) return FALSE;

	if ( cwin->job->copy_done )
	{
		gmf_copy_paste_update_all ( TRUE, cwin );

		return FALSE;
	}

	gmf_copy_paste_update_all ( FALSE, cwin );
	gmf_copy_paste_update_prg ( cwin->job->indx_set, cwin );

	return TRUE;
}

static void gmf_copy_paste ( G_GNUC_UNUSED GtkButton *button, CopyWin *cwin )
{
	cwin->job->copy_done = FALSE;

	g_cancellable_reset ( cwin->job->cancellable );

	cwin->job->path_dest = g_strdup ( gtk_entry_get_text ( cwin->entry ) );

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( cwin->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	ulong len = 0, indx = (ulong)ind + 1;
	cwin->job->list = g_malloc0 ( indx * sizeof ( char * ) );

	cwin->job->indx = 0;
	cwin->job->indx_set = 0;

	uint16_t n = 0; for ( n = 0; n < UINT16_MAX; n++ )
	{
		cwin->job->prg[n] = 0;
		cwin->job->prg_set[n] = 0;
		cwin->job->list_error[n] = NULL;
	}

	gmf_copy_paste_update_all ( TRUE, cwin );

	gboolean valid = FALSE;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( len >= UINT16_MAX ) break;

		char *path = NULL;
		gtk_tree_model_get ( model, &iter, 5, &path, -1 );

		cwin->job->list[len++] = g_strdup ( path );

		free ( path );
	}

	cwin->job->list[len] = NULL;

	GThread *thread = g_thread_new ( "copy-thread", (GThreadFunc)gmf_copy_paste_thread, cwin->job );
	g_thread_unref ( thread );

	g_timeout_add ( 100, (GSourceFunc)gmf_copy_paste_update_timeout, cwin );
}

static void gmf_copy_stop ( G_GNUC_UNUSED GtkButton *button, CopyWin *cwin )
{
	cwin->job->copy_done = TRUE;

	g_cancellable_cancel ( cwin->job->cancellable );
}

static void gmf_copy_dialog_icon_press_entry ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, G_GNUC_UNUSED gpointer *data )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

		g_autofree char *path = gmf_open_dir ( g_get_home_dir (), window );

		if ( path ) gtk_entry_set_text ( entry, path );
	}
}

static GtkEntry * gmf_copy_dialog_create_entry ( const char *text )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );

	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder" );

	g_signal_connect ( entry, "icon-press", G_CALLBACK ( gmf_copy_dialog_icon_press_entry ), NULL );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

static void gmf_copy_treeview_add ( const char *name, const char *path, GFile *file, uint16_t ind, GtkListStore *store )
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	GFileInfo *info = g_file_query_info ( file, "*", 0, NULL, NULL );

	const char *mime_type = g_file_info_get_content_type ( info );
	g_autofree char *icon_name = g_content_type_get_generic_icon_name ( mime_type );

	gboolean has_icon = gtk_icon_theme_has_icon ( icon_theme, icon_name );
	GIcon *icon = ( has_icon ) ? g_content_type_get_icon ( mime_type ) : g_themed_icon_new ( ( info ) ? "text-x-generic" : "unknown" );

	GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon ( icon_theme, icon, 16, GTK_ICON_LOOKUP_FORCE_SIZE );

	GdkPixbuf *pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

	GtkTreeIter iter;
	gtk_list_store_append ( store, &iter );
	gtk_list_store_set    ( store, &iter, 0, ind, 1, pixbuf, 2, name, 3, 0, 4, " ", 5, path, -1 );

	g_object_unref ( icon );
	g_object_unref ( info );
	g_object_unref ( icon_info );
	if ( pixbuf ) g_object_unref ( pixbuf );
}

static void gmf_copy_add_treeview ( GList *list, GtkTreeView *treeview )
{
	uint16_t ind = 1;
	GtkTreeModel *model = gtk_tree_view_get_model ( treeview );

	while ( list != NULL )
	{
		if ( ind >= UINT16_MAX ) break;

		char *path = (char *)list->data;

		GFile *file = g_file_new_for_path ( path );
		char *name  = g_file_get_basename ( file );

		gmf_copy_treeview_add ( name, path, file, ind++, GTK_LIST_STORE ( model ) );

		free ( name );
		g_object_unref ( file );

		list = list->next;
	}
}

static GtkTreeView * gmf_copy_create_treeview ( void )
{
	GtkListStore *store = gtk_list_store_new ( 6, G_TYPE_UINT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING );

	GtkTreeView *treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	//gtk_tree_view_set_headers_visible ( treeview, FALSE );
	gtk_widget_set_visible ( GTK_WIDGET ( treeview ), TRUE );

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ( "Num", renderer,  "text", 0, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ( "Icon", renderer,  "pixbuf", 1, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "File", renderer, "text", 2, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "Percent", renderer, "text", 3, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "Error", renderer, "text", 4, NULL );
	gtk_tree_view_append_column ( treeview, column );

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ( "Path", renderer, "text", 5, NULL );
	gtk_tree_view_append_column ( treeview, column );
	gtk_tree_view_column_set_visible ( column, FALSE );

	g_object_unref ( store );

	return treeview;
}

static void gmf_copy_dialog_destroy ( G_GNUC_UNUSED GtkWindow *window, CopyWin *cwin )
{
	cwin->exit = TRUE;

	gmf_copy_stop ( NULL, cwin );

	usleep ( 100000 );

	if ( cwin->job->cancellable ) g_object_unref ( cwin->job->cancellable );

	free ( cwin->job );
	free ( cwin );
}

void gmf_copy_dialog ( const char *path, GList *list, enum c_num cp_mv, GmfWin *win )
{
	CopyWin *copy_win = g_new0 ( CopyWin, 1 );
	copy_win->cp_mv = cp_mv;
	copy_win->exit = FALSE;

	copy_win->job = g_new0 ( CopyData, 1 );
	copy_win->job->cp_mv = cp_mv;
	copy_win->job->copy_done = FALSE;
	copy_win->job->cancellable = g_cancellable_new ();

	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, GTK_WINDOW ( win ) );
	gtk_window_set_icon_name ( window, c_icons[cp_mv][0] );
	gtk_window_set_default_size ( window, 500, 300 );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );
	g_signal_connect ( window, "destroy", G_CALLBACK ( gmf_copy_dialog_destroy ), copy_win );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	copy_win->treeview = gmf_copy_create_treeview ();
	gmf_copy_add_treeview ( list, copy_win->treeview );

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( copy_win->treeview ) );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	const char *labels[] = { "window-close" , "media-playback-stop", c_icons[cp_mv][0] };
	const void *funcs[] = { gtk_widget_destroy, gmf_copy_stop, gmf_copy_paste };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = button_set_image ( labels[c], 16 );

		( c == 0 ) ? g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( funcs[c] ), window )
			   : g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), copy_win );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	copy_win->entry = gmf_copy_dialog_create_entry ( path );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( copy_win->entry ), TRUE, TRUE, 0 );
	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( window );

	gtk_widget_set_opacity ( GTK_WIDGET ( window ), gtk_widget_get_opacity ( GTK_WIDGET ( win ) ) );
}

