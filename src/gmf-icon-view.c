/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-icon-view.h"
#include "gmf-dialog.h"

enum cols
{
	COL_PATH,
	COL_DISPLAY_NAME,
	COL_IS_DIRECTORY,
	COL_IS_SYMLINK,
	COL_IS_PIXBUF,
	COL_PIXBUF,
	COL_SIZE,
	NUM_COLS
};

struct _GmfIcon
{
	GtkBox parent_instance;

	GtkIconView *icon_view;

	GFileMonitor *monitor;

	char *path;

	int icon_size;

	uint src_update;

	gboolean exit;
	gboolean hidden;
	gboolean reload;
	gboolean preview;
};

G_DEFINE_TYPE ( GmfIcon, gmf_icon, GTK_TYPE_BOX )

static void gmf_icon_set_pixbuf ( GtkTreeIter iter, GtkTreeModel *model, GmfIcon *icon )
{
	char *path = NULL;
	gboolean is_dir = FALSE, is_link = FALSE;
	gtk_tree_model_get ( model, &iter, COL_IS_DIRECTORY, &is_dir, COL_IS_SYMLINK, &is_link, COL_PATH, &path, -1 );

	GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, icon->preview, icon->icon_size );

	if ( pixbuf && !icon->exit ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_IS_PIXBUF, TRUE, COL_PIXBUF, pixbuf, -1 );

	free ( path );
	if ( pixbuf ) g_object_unref ( pixbuf );
}

static void gmf_icon_update_pixbuf ( GmfIcon *icon )
{
	if ( icon->exit ) return;

	GtkTreeIter iter;
	GtkTreePath *start_path, *end_path;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( ind < 1 ) return;

	if ( ind == 1 && gtk_tree_model_get_iter_first ( model, &iter ) )
	{
		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb ) gmf_icon_set_pixbuf ( iter, model, icon );

		return;
	}

	if ( !gtk_icon_view_get_visible_range ( icon->icon_view, &start_path, &end_path ) ) return;

	int *ind_s = gtk_tree_path_get_indices ( start_path );
	int *ind_e = gtk_tree_path_get_indices ( end_path   );
	int indx = gtk_tree_model_iter_n_children ( model, NULL );

	int c = 0, add = ( ind_e[0] - ind_s[0] ), end = ( ( indx - ind_e[0] ) > add ) ? add : ( indx - ind_e[0] );

	for ( c = 0; c < end; c++ ) gtk_tree_path_next ( end_path );
	for ( c = 0; c < add; c++ ) if ( !gtk_tree_path_prev ( start_path ) ) break;

	while ( gtk_tree_path_compare ( start_path, end_path ) != 0 )
	{
		if ( icon->exit ) return;

		if ( gtk_tree_model_get_iter ( model, &iter, start_path ) )
		{
			gboolean is_pb = FALSE;
			gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

			if ( !is_pb ) gmf_icon_set_pixbuf ( iter, model, icon );
		}

		gtk_tree_path_next ( start_path );
	}

	gtk_tree_path_free ( end_path );
	gtk_tree_path_free ( start_path );
}
/*
static gboolean gmf_icon_update_slow_timeout ( GmfIcon *icon )
{
	if ( icon->exit ) return FALSE;

	GtkTreeIter iter;
	GtkTreePath *start_path, *end_path;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( ind < 1 ) { icon->src_update = 0; return FALSE; }

	if ( ind == 1 && gtk_tree_model_get_iter_first ( model, &iter ) )
	{
		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb ) gmf_icon_set_pixbuf ( iter, model, icon );

		icon->src_update = 0;
		return FALSE;
	}

	if ( !gtk_icon_view_get_visible_range ( icon->icon_view, &start_path, &end_path ) ) return FALSE;

	int *ind_s = gtk_tree_path_get_indices ( start_path );
	int *ind_e = gtk_tree_path_get_indices ( end_path   );
	int indx = gtk_tree_model_iter_n_children ( model, NULL );

	int c = 0, add = ( ind_e[0] - ind_s[0] ), end = ( ( indx - ind_e[0] ) > add ) ? add : ( indx - ind_e[0] );

	for ( c = 0; c < end; c++ ) gtk_tree_path_next ( end_path );
	for ( c = 0; c < add; c++ ) if ( !gtk_tree_path_prev ( start_path ) ) break;

	while ( gtk_tree_path_compare ( start_path, end_path ) != 0 )
	{
		if ( icon->exit ) { icon->src_update = 0; return FALSE; }

		if ( gtk_tree_model_get_iter ( model, &iter, start_path ) )
		{
			gboolean is_pb = FALSE;
			gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

			if ( !is_pb ) { gmf_icon_set_pixbuf ( iter, model, icon ); return TRUE; }
		}

		gtk_tree_path_next ( start_path );
	}

	gtk_tree_path_free ( end_path );
	gtk_tree_path_free ( start_path );

	icon->src_update = 0;

	return FALSE;
}
*/
static void gmf_icon_update_scrool ( GmfIcon *icon )
{
	gmf_icon_update_pixbuf ( icon );
	// if ( !icon->src_update ) { icon->src_update = g_timeout_add ( 250, (GSourceFunc)gmf_icon_update_slow_timeout, icon ); }
}

static gboolean gmf_icon_update_timeout ( GmfIcon *icon )
{
	gmf_icon_update_pixbuf ( icon );

	return FALSE;
}

static void gmf_icon_open_location ( const char *search, GmfIcon *icon )
{
	// if ( icon->src_update ) g_source_remove ( icon->src_update );
	// icon->src_update = 0;

	GDir *dir = g_dir_open ( icon->path, 0, NULL );
	if ( !dir ) return;

	gboolean f_hidden = FALSE;
	const char *name = NULL;

	const char *icon_name_d = "folder";
	const char *icon_name_f = "text-x-preview";
	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );
	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );

	while ( ( name = g_dir_read_name (dir) ) )
	{
		if ( name[0] == '.' ) f_hidden = TRUE; else f_hidden = FALSE;

		if ( !f_hidden || icon->hidden )
		{
			char *path = g_build_filename ( icon->path, name, NULL );
			char *display_name = g_filename_to_utf8 ( name, -1, NULL, NULL, NULL );

			gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			GdkPixbuf * pixbuf = gtk_icon_theme_load_icon ( itheme, ( is_dir ) ? icon_name_d : icon_name_f, icon->icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL );

			if ( !search || ( search && g_strrstr ( display_name, search ) ) )
			{
				gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );

				gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter,
					COL_PATH, path,
					COL_DISPLAY_NAME, display_name,
					COL_IS_DIRECTORY, is_dir,
					COL_IS_SYMLINK, is_slk,
					COL_IS_PIXBUF, FALSE,
					COL_PIXBUF, pixbuf,
					COL_SIZE, 0,
					-1 );
			}

			free ( path );
			free ( display_name );

			if ( pixbuf ) g_object_unref ( pixbuf );
		}
	}

	g_dir_close ( dir );

	g_timeout_add ( 250, (GSourceFunc)gmf_icon_update_timeout, icon );
}

static void gmf_icon_changed_monitor ( G_GNUC_UNUSED GFileMonitor *monitor, G_GNUC_UNUSED GFile *file, G_GNUC_UNUSED GFile *other, GFileMonitorEvent evtype, GmfIcon *icon )
{
	gboolean reload = FALSE;

	switch ( evtype )
	{
		case G_FILE_MONITOR_EVENT_CHANGED:           reload = FALSE; break;
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: reload = TRUE;  break;
		case G_FILE_MONITOR_EVENT_DELETED:           reload = TRUE;  break;
		case G_FILE_MONITOR_EVENT_CREATED:           reload = TRUE;  break;
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: reload = FALSE; break;
		case G_FILE_MONITOR_EVENT_RENAMED:           reload = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_IN:          reload = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_OUT:         reload = TRUE;  break;

		default: break;
	}

	if ( reload ) g_signal_emit_by_name ( icon, "icon-reload" );
}

static void gmf_icon_run_monitor ( GmfIcon *icon )
{
	GFile *file = g_file_new_for_path ( icon->path );

	if ( icon->monitor )
	{
		g_signal_handlers_disconnect_by_func ( icon->monitor, gmf_icon_changed_monitor, icon );

		g_file_monitor_cancel ( icon->monitor );

		g_object_unref ( icon->monitor );
	}

	icon->monitor = g_file_monitor ( file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL );

	if ( icon->monitor == NULL ) return;

	g_signal_connect ( icon->monitor, "changed", G_CALLBACK ( gmf_icon_changed_monitor ), icon );

	g_object_unref ( file );
}

static void gmf_icon_item_activated ( GtkIconView *icon_view, GtkTreePath *tree_path, GmfIcon *icon )
{
	g_autofree char *path = NULL;

	gboolean is_dir;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );
	
	gtk_tree_model_get_iter ( model, &iter, tree_path );
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_DIRECTORY, &is_dir, -1 );

	if ( is_dir )
	{
		free ( icon->path );
		icon->path = g_strdup ( path );

		gmf_icon_open_location ( NULL, icon );

		g_signal_emit_by_name ( icon, "icon-add-tab", path );

		gmf_icon_run_monitor ( icon );
	}
	else
	{
		GtkWidget *toplevel = gtk_widget_get_toplevel ( GTK_WIDGET ( icon->icon_view ) );

		GFile *file = g_file_new_for_path ( path );

		exec ( file, GTK_WINDOW ( toplevel ) );

		g_object_unref ( file );
	}
}

static gboolean gmf_icon_press_event ( G_GNUC_UNUSED GtkDrawingArea *draw, G_GNUC_UNUSED GdkEventButton *event, GmfIcon *icon )
{
	if ( event->button == 1 ) return FALSE;

	if ( event->button == 3 ) { g_signal_emit_by_name ( icon, "icon-button-press", NULL ); return TRUE; }

	return TRUE;
}


static gboolean gmf_icon_scroll_event ( G_GNUC_UNUSED GtkScrolledWindow *scw, G_GNUC_UNUSED GdkEventScroll *evscroll, GmfIcon *icon )
{
	gmf_icon_update_scrool ( icon );

	return FALSE;
}

static void gmf_icon_scroll_changed ( G_GNUC_UNUSED GtkAdjustment *adj, GmfIcon *icon )
{
	gmf_icon_update_scrool ( icon );
}

static void gmf_icon_drag_data_input ( G_GNUC_UNUSED GtkIconView *icon_view, GdkDragContext *context, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, 
	GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, GmfIcon *icon )
{
	GdkAtom target = gtk_selection_data_get_data_type ( s_data );

	GdkAtom atom = gdk_atom_intern_static_string ( "text/uri-list" );

	if ( target == atom )
	{
		char **uris = gtk_selection_data_get_uris ( s_data );

		gmf_copy_dialog ( icon->path, uris, C_CP, NULL );

		g_strfreev ( uris );

		gtk_drag_finish ( context, TRUE, FALSE, time );
	}
}

static void gmf_icon_drag_data_output ( GtkIconView *icon_view, G_GNUC_UNUSED GdkDragContext *context, 
	GtkSelectionData *s_data, G_GNUC_UNUSED uint info, G_GNUC_UNUSED uint time, G_GNUC_UNUSED gpointer *data )
{
	GList *list = gtk_icon_view_get_selected_items ( icon_view );
	uint len = g_list_length ( list );

	char **uris, *path = NULL;
	uris = g_malloc ( ( len + 1 ) * sizeof (char *) );

	uint i = 0;

	while ( list != NULL )
	{
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );

		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		uris[i++] = g_filename_to_uri ( path, NULL, NULL );

		free ( path );
		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	uris[i] = NULL;

	gtk_selection_data_set_uris ( s_data, uris );
	
	g_strfreev ( uris );
}

static void gmf_icon_drag_drop ( GmfIcon *icon )
{
	GtkTargetEntry targets[] = { { "text/uri-list", GTK_TARGET_OTHER_WIDGET, 0 } };

	gtk_icon_view_enable_model_drag_source ( icon->icon_view, GDK_BUTTON1_MASK, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY );
	g_signal_connect ( icon->icon_view, "drag-data-get", G_CALLBACK ( gmf_icon_drag_data_output ), NULL );

	gtk_icon_view_enable_model_drag_dest ( icon->icon_view, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY );
	g_signal_connect ( icon->icon_view, "drag-data-received", G_CALLBACK ( gmf_icon_drag_data_input ), icon );	
}

static int gmf_icon_sort_func_a_z ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	int ret = 1;

	gtk_tree_model_get ( model, a, COL_IS_DIRECTORY, &is_dir_a, COL_DISPLAY_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIRECTORY, &is_dir_b, COL_DISPLAY_NAME, &name_b, -1 );

	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
		ret = g_utf8_collate ( name_a, name_b );

	return ret;
}

static GtkListStore * gmf_icon_create_store ( void )
{
	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_UINT64 );

	gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE (store), gmf_icon_sort_func_a_z, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return store;
}

static void gmf_icon_handler_set_path ( GmfIcon *icon, const char *path )
{
	if ( path && g_file_test ( path, G_FILE_TEST_IS_DIR ) )
	{
		free ( icon->path );
		icon->path = g_strdup ( path );
	}

	gmf_icon_open_location ( NULL, icon );

	g_signal_emit_by_name ( icon, "icon-add-tab", path );

	gmf_icon_run_monitor ( icon );
}

static void gmf_icon_handler_up_path ( GmfIcon *icon )
{
	g_autofree char *path = g_path_get_dirname ( icon->path );

	gmf_icon_handler_set_path ( icon, path );
}

static char * gmf_icon_handler_get_path ( GmfIcon *icon )
{
	return g_strdup ( icon->path );
}

static uint gmf_icon_handler_get_selected_num ( GmfIcon *icon )
{
	GList *list = gtk_icon_view_get_selected_items ( icon->icon_view );

	uint num = g_list_length ( list );

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	return num;
}

static void gmf_icon_handler_set_hidden ( GmfIcon *icon )
{
	icon->hidden = !icon->hidden;

	gmf_icon_open_location ( NULL, icon );
}

static void gmf_icon_handler_reload ( GmfIcon *icon )
{
	gmf_icon_open_location ( NULL, icon );
}

static void gmf_icon_handler_set_search ( GmfIcon *icon, const char *str )
{
	gmf_icon_open_location ( str, icon );
}

static void gmf_icon_handler_set_size ( GmfIcon *icon, uint size )
{
	icon->icon_size = (int)size;
	gmf_icon_open_location ( NULL, icon );
}


static GList * gmf_icon_handler_get_selected_path ( GmfIcon *icon )
{
	GList *list_ret = NULL;

	GtkTreeIter iter;
	GList *list = gtk_icon_view_get_selected_items ( icon->icon_view );

	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	while ( list != NULL )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		list_ret = g_list_append ( list_ret, path );

		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	return list_ret;
}

static void gmf_icon_quit ( GmfIcon *icon )
{
	gtk_icon_view_unselect_all ( icon->icon_view );
}

static void gmf_icon_init ( GmfIcon *icon )
{
	icon->exit = FALSE;
	icon->hidden = FALSE;
	icon->preview = TRUE;
	icon->icon_size  = 48;
	icon->src_update = 0;
	icon->path = g_strdup ( g_get_home_dir () );

	icon->monitor = NULL;

	GtkBox *v_box = GTK_BOX ( icon );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( v_box ), GTK_ORIENTATION_VERTICAL );
	g_signal_connect ( icon, "destroy", G_CALLBACK ( gmf_icon_quit ), NULL );
	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment ( scw );
	g_signal_connect ( adj, "value-changed", G_CALLBACK ( gmf_icon_scroll_changed ), icon );

	icon->icon_view = (GtkIconView *)gtk_icon_view_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( icon->icon_view ), TRUE );

	GtkListStore *store = gmf_icon_create_store ();
	gtk_icon_view_set_model ( icon->icon_view, GTK_TREE_MODEL ( store ) );
	g_object_unref ( G_OBJECT ( store ) );

	gtk_icon_view_set_item_width    ( icon->icon_view, 80 );
	gtk_icon_view_set_pixbuf_column ( icon->icon_view, COL_PIXBUF );
	gtk_icon_view_set_text_column   ( icon->icon_view, COL_DISPLAY_NAME );

	gtk_icon_view_set_selection_mode ( icon->icon_view, GTK_SELECTION_MULTIPLE );

	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( icon->icon_view ), TRUE );

	gmf_icon_drag_drop ( icon );

	gtk_widget_set_events ( GTK_WIDGET ( icon->icon_view ), GDK_BUTTON_PRESS_MASK );
	g_signal_connect ( icon->icon_view, "item-activated",  G_CALLBACK ( gmf_icon_item_activated ), icon );
	g_signal_connect ( icon->icon_view, "button-press-event", G_CALLBACK ( gmf_icon_press_event ), icon );
	g_signal_connect ( scw, "scroll-event",  G_CALLBACK ( gmf_icon_scroll_event ), icon );

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( icon->icon_view ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	g_signal_connect ( icon, "icon-reload",   G_CALLBACK ( gmf_icon_handler_reload   ), NULL );
	g_signal_connect ( icon, "icon-up-path",  G_CALLBACK ( gmf_icon_handler_up_path  ), NULL );
	g_signal_connect ( icon, "icon-get-path", G_CALLBACK ( gmf_icon_handler_get_path ), NULL );
	g_signal_connect ( icon, "icon-set-path", G_CALLBACK ( gmf_icon_handler_set_path ), NULL );
	g_signal_connect ( icon, "icon-set-size", G_CALLBACK ( gmf_icon_handler_set_size ), NULL );
	g_signal_connect ( icon, "icon-set-search", G_CALLBACK ( gmf_icon_handler_set_search ), NULL );
	g_signal_connect ( icon, "icon-set-hidden", G_CALLBACK ( gmf_icon_handler_set_hidden ), NULL );
	g_signal_connect ( icon, "icon-get-selected-num",  G_CALLBACK ( gmf_icon_handler_get_selected_num  ), NULL );
	g_signal_connect ( icon, "icon-get-selected-path", G_CALLBACK ( gmf_icon_handler_get_selected_path ), NULL );
}

static void gmf_icon_finalize ( GObject *object )
{
	GmfIcon *icon = GMF_ICON (object);

	icon->exit = TRUE;
	free ( icon->path );

	G_OBJECT_CLASS (gmf_icon_parent_class)->finalize (object);
}

static void gmf_icon_class_init ( GmfIconClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = gmf_icon_finalize;

	g_signal_new ( "icon-reload", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "icon-up-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "icon-get-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_STRING, 0 );

	g_signal_new ( "icon-set-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-set-size", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );

	g_signal_new ( "icon-set-search", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-add-tab", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-set-hidden", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "icon-button-press", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "icon-get-selected-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_UINT, 0 );

	g_signal_new ( "icon-get-selected-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_POINTER, 0 );
}

GmfIcon * gmf_icon_new ( void )
{
	return g_object_new ( GMF_TYPE_ICON, NULL );
}

