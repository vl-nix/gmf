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

#include "copy-win.h"

#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define ITEM_WIDTH 80

enum cols
{
	COL_PATH,
	COL_NAME,
	COL_IS_DIR,
	COL_IS_LINK,
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
	uint src_reload;
	uint src_change;

	gboolean exit;
	gboolean slow;
	gboolean hidden;
	gboolean preview;

	gboolean debug;
};

G_DEFINE_TYPE ( GmfIcon, gmf_icon, GTK_TYPE_BOX )

static void gmf_icon_set_pixbuf ( GtkTreeIter iter, GtkTreeModel *model, GmfIcon *icon )
{
	g_autofree char *path = NULL;
	gboolean is_dir = FALSE, is_link = FALSE;
	gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

	if ( g_file_test ( path, G_FILE_TEST_EXISTS ) )
	{
		GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, icon->preview, icon->icon_size );

		if ( pixbuf && !icon->exit ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_IS_PIXBUF, TRUE, COL_PIXBUF, pixbuf, -1 );

		if ( pixbuf ) g_object_unref ( pixbuf );
	}
}

static gboolean gmf_icon_update_pixbuf ( GmfIcon *icon )
{
	if ( icon->exit ) return FALSE;

	gboolean ret = FALSE;

	GtkTreeIter iter;
	GtkTreePath *start_path, *end_path;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	if ( !gtk_icon_view_get_visible_range ( icon->icon_view, &start_path, &end_path ) ) return ret;

	gboolean valid = FALSE;
	int *ind_s = gtk_tree_path_get_indices ( start_path );
	int *ind_e = gtk_tree_path_get_indices ( end_path   );

	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( icon->icon_view ) );
	int cw = (int)( ( w / ( ITEM_WIDTH * 1.8 ) ) + 1 );
	int num = 0, item_nums = ( ind_e[0] - ind_s[0] ); item_nums += cw; // ( cw * 2 );

	for ( valid = gtk_tree_model_get_iter ( model, &iter, start_path ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( icon->exit ) break;
		if ( num > item_nums ) break;

		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb && icon->slow ) ret = TRUE;
		if ( !is_pb ) { gmf_icon_set_pixbuf ( iter, model, icon ); if ( icon->slow ) break; }

		num++;
	}

	gtk_tree_path_free ( end_path );
	gtk_tree_path_free ( start_path );

	return ret;
}

static gboolean gmf_icon_update_slow_timeout ( GmfIcon *icon )
{
	if ( icon->exit ) { icon->src_update = 0; return FALSE; }

	gboolean ret = gmf_icon_update_pixbuf ( icon );

	if ( !ret ) icon->src_update = 0;

	return ret;
}

static void gmf_icon_update_scrool ( GmfIcon *icon )
{
	if ( icon->debug && icon->slow ) g_message ( "%s:: src_update %u ", __func__, icon->src_update );

	if ( icon->slow )
		{ if ( !icon->src_update ) { icon->src_update = g_timeout_add ( 500, (GSourceFunc)gmf_icon_update_slow_timeout, icon ); } }
	else
		gmf_icon_update_pixbuf ( icon );
}

static void gmf_icon_open_location ( const char *search, GmfIcon *icon )
{
	if ( icon->exit ) return;

	if ( icon->src_update ) g_source_remove ( icon->src_update );
	icon->src_update = 0;

	GDir *dir = g_dir_open ( icon->path, 0, NULL );

	if ( !dir )
	{
		GtkWidget *window = gtk_widget_get_toplevel ( GTK_WIDGET ( icon->icon_view ) );

		int s_errno = errno;
		gmf_message_dialog ( "", g_strerror ( s_errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( window ) );

		return;
	}

	if ( icon->debug ) g_message ( "%s:: path: %s ", __func__, icon->path );

	int w = gtk_widget_get_allocated_width  ( GTK_WIDGET ( icon->icon_view ) );
	int h = gtk_widget_get_allocated_height ( GTK_WIDGET ( icon->icon_view ) );

	uint16_t cw = (uint16_t)( ( w / ( ITEM_WIDTH * 1.8 ) ) + 1 );
	uint16_t ch = (uint16_t)( ( h / ( ITEM_WIDTH -  20 ) ) + 1 );
	uint16_t num = 0, item_nums = (uint16_t)(cw * ch * 2);

	const char *name = NULL;
	const char *icon_name_d = "folder";
	const char *icon_name_f = "text-x-preview"; // text-x-generic
	gboolean f_hidden = FALSE, valid = FALSE;
	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );
	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );

	while ( ( name = g_dir_read_name (dir) ) )
	{
		if ( icon->exit ) return;

		if ( name[0] == '.' ) f_hidden = TRUE; else f_hidden = FALSE;

		if ( !f_hidden || icon->hidden )
		{
			char *path = g_build_filename ( icon->path, name, NULL );
			char *display_name = g_filename_to_utf8 ( name, -1, NULL, NULL, NULL );

			gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			ulong size = get_file_size ( path );
			GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( itheme, ( is_dir ) ? icon_name_d : icon_name_f, icon->icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL );

			if ( !search || ( search && g_strrstr ( display_name, search ) ) )
			{
				gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );

				gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter,
					COL_PATH, path,
					COL_NAME, display_name,
					COL_IS_DIR, is_dir,
					COL_IS_LINK, is_slk,
					COL_IS_PIXBUF, FALSE,
					COL_PIXBUF, pixbuf,
					COL_SIZE, size,
					-1 );
			}

			if ( icon->debug ) g_message ( "%s:: path: %s ", __func__, path );

			free ( path );
			free ( display_name );

			if ( pixbuf ) g_object_unref ( pixbuf );
		}
	}

	g_dir_close ( dir );

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( icon->exit ) return;
		if ( num > item_nums ) break;

		char *path = NULL;
		gboolean is_dir = FALSE, is_link = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

		GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, icon->preview, icon->icon_size );

		if ( pixbuf && !icon->exit ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PIXBUF, pixbuf, COL_IS_PIXBUF, TRUE, -1 );

		if ( icon->debug ) g_message ( "%s:: num %u, path: %s ", __func__, num, path );

		num++;
		free ( path );
		if ( pixbuf ) g_object_unref ( pixbuf );
	}
}

static GdkPixbuf * gmf_icon_pixbuf_add_text ( const char *fsize, int icon_size, GdkPixbuf *pxbf )
{
	int sf = icon_size / 5;
	int pf = icon_size - sf;

	cairo_surface_t *surface = cairo_image_surface_create ( CAIRO_FORMAT_ARGB32, icon_size, icon_size );
	cairo_t *cr = cairo_create ( surface );

	cairo_rectangle ( cr, 0, 0, icon_size, icon_size );
	gdk_cairo_set_source_pixbuf ( cr, pxbf, 0, 0 );
	cairo_fill ( cr );

	cairo_rectangle ( cr, 0, pf-sf, icon_size, pf );
	cairo_set_source_rgba ( cr, 0.5, 0.5, 0.5, 0.75 );	
	cairo_fill ( cr );

	cairo_set_source_rgb ( cr, 0, 0, 0 );
	cairo_select_font_face ( cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD );
	cairo_set_font_size ( cr, sf );

	cairo_move_to ( cr, 2, pf + ( sf / 2 ) );
	cairo_show_text ( cr, fsize );

	GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface ( surface, 0, 0, icon_size, icon_size );

	cairo_destroy ( cr );
	cairo_surface_destroy ( surface );

	return pixbuf;
}

static void gmf_icon_pixbuf_set_text ( const char *path_draw, GmfIcon *icon )
{
	GtkTreeIter iter;
	GtkTreePath *start_path, *end_path;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	if ( !gtk_icon_view_get_visible_range ( icon->icon_view, &start_path, &end_path ) ) return;

	int *ind_s = gtk_tree_path_get_indices ( start_path );
	int *ind_e = gtk_tree_path_get_indices ( end_path   );

	gboolean valid = FALSE;
	int num = 0, item_nums = ( ind_e[0] - ind_s[0] ); item_nums *= 2;

	for ( valid = gtk_tree_model_get_iter ( model, &iter, start_path ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( icon->exit ) break;
		if ( num > item_nums ) break;

		char *path = NULL;
		gboolean fbreak = FALSE, is_dir = FALSE, is_link = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

		if ( g_str_equal ( path, path_draw ) )
		{
			ulong size = get_file_size ( path );
			g_autofree char *fsize = g_format_size ( size );

			GdkPixbuf *pxbf = file_get_pixbuf ( path, is_dir, is_link, icon->preview, icon->icon_size );

			GdkPixbuf *pixbuf = ( pxbf ) ? gmf_icon_pixbuf_add_text ( fsize, icon->icon_size, pxbf ) : NULL;

			if ( pixbuf && !icon->exit ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PIXBUF, pixbuf, COL_IS_PIXBUF, TRUE, -1 );

			if ( icon->debug ) g_message ( "%s:: path: %s ", __func__, path_draw );

			if ( pxbf ) g_object_unref ( pxbf );
			if ( pixbuf ) g_object_unref ( pixbuf );

			fbreak = TRUE;
		}

		num++;
		free ( path );
		if ( fbreak ) break;
	}

	gtk_tree_path_free ( end_path );
	gtk_tree_path_free ( start_path );
}

static gboolean gmf_icon_reload_timeout ( GmfIcon *icon )
{
	if ( icon->exit ) { icon->src_reload = 0; return FALSE; }

	g_signal_emit_by_name ( icon, "icon-reload" );

	icon->src_reload = 0;

	return FALSE;
}

static void gmf_icon_changed_monitor ( G_GNUC_UNUSED GFileMonitor *monitor, G_GNUC_UNUSED GFile *file, G_GNUC_UNUSED GFile *other, GFileMonitorEvent evtype, GmfIcon *icon )
{
	if ( icon->exit ) return;

	gboolean reload = FALSE, changed = FALSE;

	switch ( evtype )
	{
		case G_FILE_MONITOR_EVENT_CHANGED:           changed = TRUE;  break;
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_DELETED:           reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_CREATED:           reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_RENAMED:           reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_IN:          reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_OUT:         reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: reload  = FALSE; break;

		default: break;
	}

	if ( changed )
	{
		g_autofree char *path = g_file_get_path ( file );

		gmf_icon_pixbuf_set_text ( path, icon );
	}

	if ( reload && icon->debug ) g_message ( "%s:: src_reload %u ", __func__, icon->src_reload );
	if ( reload && !icon->src_reload ) icon->src_reload = g_timeout_add ( 250, (GSourceFunc)gmf_icon_reload_timeout, icon );
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

static gboolean gmf_icon_changed_timeout ( GmfIcon *icon )
{
	if ( icon->exit ) { icon->src_change = 0; return FALSE; }

	GtkTreeIter iter;
	GList *list = gtk_icon_view_get_selected_items ( icon->icon_view );

	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	while ( list != NULL )
	{
		if ( icon->exit ) break;

		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );

		gboolean is_dir = FALSE, is_link = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

		if ( !is_dir )
		{
			ulong size = get_file_size ( path );
			g_autofree char *fsize = g_format_size ( size );

			GdkPixbuf *pxbf = file_get_pixbuf ( path, is_dir, is_link, icon->preview, icon->icon_size );

			GdkPixbuf *pixbuf = ( pxbf ) ? gmf_icon_pixbuf_add_text ( fsize, icon->icon_size, pxbf ) : NULL;

			if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PIXBUF, pixbuf, COL_IS_PIXBUF, TRUE, -1 );

			if ( pxbf ) g_object_unref ( pxbf );
			if ( pixbuf ) g_object_unref ( pixbuf );
		}

		free ( path );
		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	icon->src_change = 0;

	return FALSE;
}

static void gmf_icon_selection_changed ( G_GNUC_UNUSED GtkIconView *icon_view, GmfIcon *icon )
{
	if ( icon->debug ) g_message ( "%s:: src_change %u ", __func__, icon->src_change );

	if ( !icon->src_change ) icon->src_change = g_timeout_add ( 1000, (GSourceFunc)gmf_icon_changed_timeout, icon );
}

static void gmf_icon_item_activated ( GtkIconView *icon_view, GtkTreePath *tree_path, GmfIcon *icon )
{
	g_autofree char *path = NULL;

	gboolean is_dir;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );
	
	gtk_tree_model_get_iter ( model, &iter, tree_path );
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1 );

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
		GtkWidget *window = gtk_widget_get_toplevel ( GTK_WIDGET ( icon->icon_view ) );

		GFile *file = g_file_new_for_path ( path );

		exec ( file, GTK_WINDOW ( window ) );

		g_object_unref ( file );
	}
}

static gboolean gmf_icon_press_event ( G_GNUC_UNUSED GtkDrawingArea *draw, G_GNUC_UNUSED GdkEventButton *event, GmfIcon *icon )
{
	if ( event->button == 1 ) return FALSE;

	if ( event->button == 2 ) { g_signal_emit_by_name ( icon, "icon-button-press", event->button ); return TRUE; }

	if ( event->button == 3 ) { g_signal_emit_by_name ( icon, "icon-button-press", event->button ); return TRUE; }

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

		copy_win_new ( CM_CP, icon->path, uris, NULL, 1.0 );

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

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
		ret = g_utf8_collate ( name_a, name_b );

	return ret;
}

static int gmf_icon_sort_func_z_a ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	int ret = 1;

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_b && is_dir_a )
		ret = 1;
	else if ( is_dir_b && !is_dir_a )
		ret = -1;
	else
		ret = g_utf8_collate ( name_b, name_a );

	return ret;
}

static int gmf_icon_sort_func_m_g ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	int ret = 1;

	uint64_t size_a = 0;
	uint64_t size_b = 0;

	gtk_tree_model_get ( model, a, COL_SIZE, &size_a, -1 );
	gtk_tree_model_get ( model, b, COL_SIZE, &size_b, -1 );

	ret = ( size_a >= size_b ) ? 1 : 0;

	return ret;
}

static int gmf_icon_sort_func_g_m ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, G_GNUC_UNUSED gpointer data )
{
	int ret = 1;

	uint64_t size_a = 0;
	uint64_t size_b = 0;

	gtk_tree_model_get ( model, a, COL_SIZE, &size_a, -1 );
	gtk_tree_model_get ( model, b, COL_SIZE, &size_b, -1 );

	ret = ( size_a <= size_b ) ? 1 : 0;

	return ret;
}

static void gmf_icon_set_sort_func ( uint num, GmfIcon *icon )
{
	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );

	if ( num == 0 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_icon_sort_func_a_z, NULL, NULL );
	if ( num == 1 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_icon_sort_func_z_a, NULL, NULL );
	if ( num == 2 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_icon_sort_func_m_g, NULL, NULL );
	if ( num == 3 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_icon_sort_func_g_m, NULL, NULL );
}

static void gmf_icon_handler_set_sort ( GmfIcon *icon, uint num )
{
	gmf_icon_set_sort_func ( num, icon );

	gmf_icon_open_location ( NULL, icon );
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

	g_signal_emit_by_name ( icon, "icon-add-tab", icon->path );

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

static void gmf_icon_handler_set_preview ( GmfIcon *icon, gboolean state )
{
	icon->preview = state;
	gmf_icon_open_location ( NULL, icon );
}

static void gmf_icon_handler_set_slow ( GmfIcon *icon, gboolean state )
{
	icon->slow = state;
}

static void gmf_icon_handler_set_props ( GmfIcon *icon, const char *path, uint size, gboolean state )
{
	icon->preview = state;
	icon->icon_size = (int)size;

	gmf_icon_handler_set_path ( icon, path );
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

	GtkTreeModel *model = gtk_icon_view_get_model ( icon->icon_view );
	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );
}

static void gmf_icon_init ( GmfIcon *icon )
{
	icon->exit = FALSE;
	icon->hidden = FALSE;
	icon->preview = TRUE;
	icon->icon_size  = 48;

	icon->src_update = 0;
	icon->src_reload = 0;
	icon->src_change = 0;

	icon->path = g_strdup ( g_get_home_dir () );

	icon->debug = ( g_getenv ( "GMF_DEBUG" ) ) ? TRUE : FALSE;

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

	gtk_icon_view_set_item_width    ( icon->icon_view, ITEM_WIDTH );
	gtk_icon_view_set_pixbuf_column ( icon->icon_view, COL_PIXBUF );
	gtk_icon_view_set_text_column   ( icon->icon_view, COL_NAME );

	gtk_icon_view_set_selection_mode ( icon->icon_view, GTK_SELECTION_MULTIPLE );

	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( icon->icon_view ), TRUE );

	gmf_icon_drag_drop ( icon );

	gtk_widget_set_events ( GTK_WIDGET ( icon->icon_view ), GDK_BUTTON_PRESS_MASK );
	g_signal_connect ( icon->icon_view, "item-activated",  G_CALLBACK ( gmf_icon_item_activated ), icon );
	g_signal_connect ( icon->icon_view, "selection-changed",  G_CALLBACK ( gmf_icon_selection_changed ), icon );
	g_signal_connect ( icon->icon_view, "button-press-event", G_CALLBACK ( gmf_icon_press_event ), icon );
	g_signal_connect ( scw, "scroll-event",  G_CALLBACK ( gmf_icon_scroll_event ), icon );

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( icon->icon_view ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	g_signal_connect ( icon, "icon-reload",   G_CALLBACK ( gmf_icon_handler_reload   ), NULL );
	g_signal_connect ( icon, "icon-up-path",  G_CALLBACK ( gmf_icon_handler_up_path  ), NULL );
	g_signal_connect ( icon, "icon-get-path", G_CALLBACK ( gmf_icon_handler_get_path ), NULL );
	g_signal_connect ( icon, "icon-set-path", G_CALLBACK ( gmf_icon_handler_set_path ), NULL );
	g_signal_connect ( icon, "icon-set-size", G_CALLBACK ( gmf_icon_handler_set_size ), NULL );
	g_signal_connect ( icon, "icon-set-search",  G_CALLBACK ( gmf_icon_handler_set_search  ), NULL );
	g_signal_connect ( icon, "icon-set-hidden",  G_CALLBACK ( gmf_icon_handler_set_hidden  ), NULL );
	g_signal_connect ( icon, "icon-set-preview", G_CALLBACK ( gmf_icon_handler_set_preview ), NULL );
	g_signal_connect ( icon, "icon-set-slow",    G_CALLBACK ( gmf_icon_handler_set_slow    ), NULL );
	g_signal_connect ( icon, "icon-set-props",   G_CALLBACK ( gmf_icon_handler_set_props   ), NULL );
	g_signal_connect ( icon, "icon-set-sort-func",     G_CALLBACK ( gmf_icon_handler_set_sort ), NULL );
	g_signal_connect ( icon, "icon-get-selected-num",  G_CALLBACK ( gmf_icon_handler_get_selected_num  ), NULL );
	g_signal_connect ( icon, "icon-get-selected-path", G_CALLBACK ( gmf_icon_handler_get_selected_path ), NULL );
}

static void gmf_icon_finalize ( GObject *object )
{
	GmfIcon *icon = GMF_ICON (object);

	if ( icon->monitor )
	{
		g_signal_handlers_disconnect_by_func ( icon->monitor, gmf_icon_changed_monitor, icon );

		g_file_monitor_cancel ( icon->monitor );

		g_object_unref ( icon->monitor );
	}

	if ( icon->src_update ) g_source_remove ( icon->src_update );
	if ( icon->src_reload ) g_source_remove ( icon->src_reload );
	if ( icon->src_change ) g_source_remove ( icon->src_change );

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

	g_signal_new ( "icon-set-sort-func", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );

	g_signal_new ( "icon-add-tab", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-get-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_STRING, 0 );

	g_signal_new ( "icon-set-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-set-size", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );

	g_signal_new ( "icon-set-search", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "icon-set-slow", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN );

	g_signal_new ( "icon-set-preview", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN );

	g_signal_new ( "icon-set-hidden", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "icon-set-props", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_BOOLEAN );

	g_signal_new ( "icon-button-press", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );

	g_signal_new ( "icon-get-selected-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_UINT, 0 );

	g_signal_new ( "icon-get-selected-path", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_POINTER, 0 );
}

GmfIcon * gmf_icon_new ( void )
{
	return g_object_new ( GMF_TYPE_ICON, NULL );
}

