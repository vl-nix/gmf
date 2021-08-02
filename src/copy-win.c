/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "copy-win.h"
#include "gmf-dialog.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

G_LOCK_DEFINE_STATIC ( copy_progress );

const char *cm_icons[CM_ALL][2] = 
{
	[CM_CP] = { "edit-copy",  "Copy - Gmf"  },
	[CM_MV] = { "edit-cut",   "Move - Gmf"  },
	[CM_PS] = { "edit-paste", "Paste - Gmf" }
};

enum f_num 
{
	F_BACKUP,
	F_OVERWRITE,
	F_NEXT,
	F_DEF
};

typedef struct _CopyData CopyData;

struct _CopyData
{
	enum cm_num cp_mv;

	GCancellable *cancellable;

	char **list;
	char **list_err;
	char *path_dest;

	uint16_t indx;
	uint16_t indx_old;
	uint64_t size_f_cur;

	uint8_t prg[UINT16_MAX];

	uint32_t indx_dir;
	uint32_t indx_dir_a;
	uint64_t size_dir;
	uint64_t size_dir_a;

	uint32_t indx_err_dir;

	gboolean total_plus;

	enum f_num bon;

	const char *ferr;

	gboolean copy_done;
	gboolean copy_stop;
	gboolean copy_debug;
};

struct _CopyWin
{
	GtkWindow parent_instance;

	GtkEntry *entry;
	GtkLabel *prg_dir;
	GtkTreeView *treeview;

	GtkExpander *expander;
	GtkTextView *text_view;

	enum cm_num cp_mv;

	GThread *thread;
	CopyData *job;

	uint src_update;

	gboolean exit;
	gboolean debug;
};

G_DEFINE_TYPE ( CopyWin, copy_win, GTK_TYPE_WINDOW )

static void copy_set_new_name ( char *name, char *path, GFile **file )
{
	GDateTime *date = g_date_time_new_now_local ();
	char *str_time = g_date_time_format ( date, "%j-%Y-%H.%M.%S" );
	char *new_name = g_strdup_printf ( "%s/copy-%s-%s", path, str_time, name );

	g_object_unref ( *file );
	*file = g_file_new_for_path ( new_name );

	free ( new_name );
	free ( str_time );
	g_date_time_unref ( date );
}

static void copy_get_count_size_dir ( gboolean recursion, gboolean *stop, char *path, uint32_t *indx, uint64_t *size, gboolean debug )
{
	uint32_t dirs = 0, files = 0;

	if ( g_file_test ( path, G_FILE_TEST_IS_DIR ) )
	{
		GDir *dir = g_dir_open ( path, 0, NULL );

		if ( dir )
		{
			const char *name = NULL;

			while ( ( name = g_dir_read_name (dir) ) != NULL )
			{
				if ( *stop ) break;

				char *path_new = g_strconcat ( path, "/", name, NULL );

				gboolean is_link = g_file_test ( path_new, G_FILE_TEST_IS_SYMLINK );

				if ( g_file_test ( path_new, G_FILE_TEST_IS_DIR ) )
					{ dirs++; if ( recursion && !is_link ) copy_get_count_size_dir ( recursion, stop, path_new, indx, size, debug ); } // Recursion!
				else
					{ files++; if ( !is_link ) *size += get_file_size ( path_new ); }

				if ( debug ) g_message ( "%s:: name %s ", __func__, name );

				free ( path_new );
			}

			g_dir_close (dir);

			*indx += ( dirs + files );

			if ( debug ) g_message ( "%s:: all indx %u, all size %lu ", __func__, *indx, *size );
		}
	}
}

static void copy_win_file_dir_progress ( int64_t current, int64_t total, gpointer data )
{
	CopyData *job = (CopyData *)data;

	uint8_t pr = ( total ) ? ( uint8_t )( current * 100 / total ) : 0;

	if ( job->copy_debug ) g_message ( "%s:: %u%%, current %ld, total %ld ", __func__, pr, current, total );

	if ( pr == 100 && !job->total_plus )
	{
		job->total_plus = TRUE;
		job->size_dir += (uint64_t)total;

		uint8_t prg = ( job->size_dir_a ) ? ( uint8_t )( (double)job->size_dir * 100 / (double)job->size_dir_a ) : 0;
		job->prg[job->indx] = prg;
	}
	else if ( pr != 100 )
	{
		uint8_t prg = ( job->size_dir_a ) ? ( uint8_t )( (double)(job->size_dir + (uint64_t)current) * 100 / (double)job->size_dir_a ) : 0;

		job->total_plus = FALSE;
		job->prg[job->indx] = prg;	
	}
}

static void copy_win_file_progress ( int64_t current, int64_t total, gpointer data )
{
	CopyData *job = (CopyData *)data;

	uint8_t prg = ( total ) ? ( uint8_t )( current * 100 / total ) : 0;

	if ( job->copy_debug ) g_message ( "%s:: %u%%, current %ld, total %ld ", __func__, prg, current, total );

	job->prg[job->indx] = prg;

	job->size_f_cur = (uint64_t)current;
}

static void copy_win_save_error ( const char *text, CopyData *job )
{
	FILE *file = fopen ( job->ferr, "a");

	if ( file == NULL ) return;

	fprintf ( file, "%s\n", text );

	fclose ( file );
}

static void copy_win_paste_thread_dir ( const char *path_src, const char *path_dest, GCancellable *cancellable, CopyData *job )
{
	if ( job->copy_debug ) g_message ( "%s:: Run paste-thread->dir Recursion ", __func__ );

	GError *error = NULL;

	GFile *dir_dest = g_file_new_for_path ( path_dest );
	g_file_make_directory ( dir_dest, NULL, &error );

	if ( error )
	{
		if ( !g_file_query_exists ( dir_dest, NULL ) )
		{
			job->indx_err_dir++;

			if ( job->list_err[job->indx] ) free ( job->list_err[job->indx] );
			job->list_err[job->indx] = g_strdup ( error->message );

			copy_win_save_error ( error->message, job );

			if ( job->copy_debug ) g_message ( "%s:: %s, error: %s ", __func__, path_dest, error->message );

			g_error_free ( error );
			g_object_unref ( dir_dest );

			return;
		}

		g_error_free ( error );
	}

	g_object_unref ( dir_dest );

	GDir *dir = g_dir_open ( path_src, 0, NULL );

	if ( dir )
	{
		const char *name = NULL;

		while ( ( name = g_dir_read_name ( dir ) ) != NULL )
		{
			if ( job->indx_dir >= UINT32_MAX ) break;

			if ( job->copy_stop ) break;

			GFileCopyFlags flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;

			if ( job->bon == F_OVERWRITE ) flags = G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS;

			char *path_new = g_strconcat ( path_src, "/", name, NULL );
			char *path_dst = g_strdup_printf ( "%s/%s", path_dest, name );

			if ( g_file_test ( path_new, G_FILE_TEST_IS_DIR ) )
			{
				copy_win_paste_thread_dir ( path_new, path_dst, cancellable, job ); // Recursion!
			}
			else
			{
				GError *error = NULL;
				GFile *file_src = g_file_new_for_path ( path_new );
				GFile *file_dst = g_file_new_for_path ( path_dst );

				job->total_plus = FALSE;
				gboolean ignore = FALSE;
				if ( g_file_test ( path_dst, G_FILE_TEST_EXISTS ) && job->bon == F_NEXT ) ignore = TRUE;

				if ( g_file_test ( path_dst, G_FILE_TEST_EXISTS ) && job->bon == F_BACKUP )
					copy_set_new_name ( name, path_dest, &file_dst );

				if ( !ignore ) g_file_copy ( file_src, file_dst, flags, cancellable, copy_win_file_dir_progress, job, &error );

				g_object_unref ( file_src );
				g_object_unref ( file_dst );

				if ( error )
				{
					job->indx_dir--;
					job->indx_err_dir++;

					if ( job->list_err[job->indx] ) free ( job->list_err[job->indx] );
					job->list_err[job->indx] = g_strdup ( error->message );

					copy_win_save_error ( error->message, job );

					if ( job->copy_debug ) g_message ( "%s:: %s, error: %s ", __func__, path_dst, error->message );

					g_error_free ( error );
				}
			}

			free ( path_new );
			free ( path_dst );

			job->indx_dir++;
		}

		g_dir_close ( dir );
	}

	if ( job->copy_debug ) g_message ( "%s:: Done paste-thread->dir Recursion ", __func__ );
}

static gpointer copy_win_paste_thread ( CopyData *job )
{
	if ( job->copy_debug ) g_message ( "%s:: Run paste-thread ", __func__ );

	while ( job->list[job->indx] != NULL )
	{
		if ( job->indx >= UINT16_MAX ) break;

		if ( job->copy_stop ) { free ( job->list[job->indx++] ); continue; }

		GFileCopyFlags flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;

		if ( job->bon == F_OVERWRITE ) flags = G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS;

		char *path = job->list[job->indx];
		char *name = g_path_get_basename ( path );

		GError *error = NULL;
		GFile *file_src = g_file_new_for_path ( path );
		GFile *file_dst = g_file_new_build_filename ( job->path_dest, name, NULL );

		if ( job->cp_mv == CM_MV )
		{
			if ( g_file_test ( path, G_FILE_TEST_IS_DIR ) && g_file_query_exists ( file_dst, NULL ) )
				job->list_err[job->indx] = g_strdup ( "Forbidden. Dir exists." );
			else
				g_file_move ( file_src, file_dst, flags, job->cancellable, copy_win_file_progress, job, &error );
		}
		else
		{
			if ( g_file_test ( path, G_FILE_TEST_IS_DIR ) )
			{
				job->indx_dir = 0;
				job->indx_dir_a = 0;

				job->size_dir = 0;
				job->size_dir_a = 0;

				job->total_plus = FALSE;
				job->copy_stop = FALSE;

				copy_get_count_size_dir ( TRUE, &job->copy_stop, path, &job->indx_dir_a, &job->size_dir_a, job->copy_debug );

				char *path_dest = g_file_get_path ( file_dst );
				copy_win_paste_thread_dir ( path, path_dest, job->cancellable, job );
				free ( path_dest );
			}
			else
			{
				gboolean ignore = FALSE;
				if ( g_file_query_exists ( file_dst, NULL ) && job->bon == F_NEXT ) ignore = TRUE;

				if ( g_file_query_exists ( file_dst, NULL ) && job->bon == F_BACKUP )
					copy_set_new_name ( name, job->path_dest, &file_dst );

				if ( !ignore ) g_file_copy ( file_src, file_dst, flags, job->cancellable, copy_win_file_progress, job, &error );
			}
		}

		g_object_unref ( file_dst );
		g_object_unref ( file_src );

		free ( name );
		free ( path );

		if ( error )
		{
			job->list_err[job->indx] = g_strdup ( error->message );

			if ( job->copy_debug ) g_message ( "%s:: %s/%s, error: %s ", __func__, job->path_dest, name, error->message );

			g_error_free ( error );
		}

		job->indx++;
	}

	job->copy_done = TRUE;

	free ( job->list );
	free ( job->path_dest );

	if ( job->copy_debug ) g_message ( "%s:: wait for error: 200 ms update-timeout ( 100 ms ) ", __func__ );

	usleep ( 200000 );

	uint16_t n = 0; for ( n = 0; n < job->indx; n++ ) if ( job->list_err[n] ) { free ( job->list_err[n] ); job->list_err[n] = NULL; }

	if ( job->copy_debug ) g_message ( "%s:: Done paste-thread ", __func__ );

	return NULL;
}

static void copy_win_update_timeout_prg ( uint16_t ind, uint8_t prg, CopyWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	GtkTreePath *tree_path = gtk_tree_path_new_from_indices ( ind, -1 );

	if ( gtk_tree_model_get_iter ( model, &iter, tree_path ) )
	{
		uint prc = 0;
		gtk_tree_model_get ( model, &iter, 3, &prc, -1 );

		if ( prc < 100 ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 3, prg, -1 );

		G_LOCK ( copy_progress );

		char *err = win->job->list_err[ind];

		G_UNLOCK ( copy_progress );

		if ( err ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 4, err, -1 );
	}

	if ( tree_path ) gtk_tree_path_free ( tree_path );
}

static void copy_win_update_label ( uint32_t indd, uint64_t size, uint64_t sizef, CopyWin *win )
{
	if ( size || sizef )
	{
		int ind = gtk_tree_model_iter_n_children ( gtk_tree_view_get_model ( win->treeview ), NULL );

		if ( ind != 1 ) { gtk_label_set_text ( win->prg_dir, " " ); return; }

		g_autofree char *size_a = g_format_size ( ( size ) ? size : sizef );
		g_autofree char *string = ( size ) ? g_strdup_printf ( "%u  /  %s", indd, size_a ) : g_strdup_printf ( "%s", size_a );

		gtk_label_set_text ( win->prg_dir, string );
	}
	else
		gtk_label_set_text ( win->prg_dir, " " );
}

static void copy_win_get_errors ( CopyWin *win )
{
	if ( !g_file_test ( win->job->ferr, G_FILE_TEST_EXISTS ) ) return;

	g_autofree char *label = g_strdup_printf ( "Errors %u:", win->job->indx_err_dir );
	gtk_expander_set_label ( win->expander, label );

	GtkTextIter iter;
	GtkTextBuffer *buffer = (GtkTextBuffer *)gtk_text_view_get_buffer ( win->text_view );
	gtk_text_buffer_get_start_iter ( buffer, &iter );

	FILE *file = fopen ( win->job->ferr, "r");

	if ( file == NULL ) return;

	char buf[1024];

	while ( fgets ( buf, 1024, file ) != NULL )
	{
		gtk_text_buffer_insert ( buffer, &iter, buf,  -1 );
		gtk_text_buffer_insert ( buffer, &iter, "\n", -1 );
	}

	fclose ( file );
}

static gboolean copy_win_update_timeout ( CopyWin *win )
{
	if ( win->exit ) { win->src_update = 0; return FALSE; }

	G_LOCK ( copy_progress );

	gboolean done = win->job->copy_done;

	uint8_t *prg_m = win->job->prg;
	uint16_t ind = win->job->indx;

	uint32_t indd = win->job->indx_dir;
	uint64_t size = win->job->size_dir;

	uint64_t sizef = win->job->size_f_cur;

	G_UNLOCK ( copy_progress );

	uint16_t set = (uint16_t)( ind - win->job->indx_old ), n = 0, s = (uint16_t)( ind - set );

	if ( done )
	{
		for ( n = 0; n < ind; n++ ) copy_win_update_timeout_prg ( n, prg_m[n], win );

		copy_win_update_label ( indd, size, sizef, win );

		if ( win->job->indx_err_dir > 1 ) copy_win_get_errors ( win );

		if ( win->debug ) g_message ( "%s:: Done update-timeout, src_update %u ", __func__, win->src_update );

		win->src_update = 0;

		return FALSE;
	}

	for ( n = s; n <= ind; n++ ) copy_win_update_timeout_prg ( n, prg_m[n], win );

	win->job->indx_old = ind;

	copy_win_update_label ( indd, size, sizef, win );

	return TRUE;
}

static void copy_win_paste ( CopyWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( win->treeview );

	uint16_t ind = (uint16_t)gtk_tree_model_iter_n_children ( model, NULL );

	uint16_t len = 0, indx = (uint16_t)( ind + 1 );

	win->job->list = g_malloc0 ( indx * sizeof ( char * ) );
	win->job->list_err = g_malloc0 ( indx * sizeof ( char * ) );

	gboolean valid = FALSE;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( len >= UINT16_MAX ) break;

		char *path = NULL;
		gtk_tree_model_get ( model, &iter, 5, &path, -1 );

		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 3, 0, -1 );
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, 4, NULL, -1 );

		win->job->list_err[len] = NULL;
		win->job->list[len] = g_strdup ( path );

		len++;
		free ( path );
	}

	win->job->list[len] = NULL;

	if ( win->thread ) g_thread_unref ( win->thread );
	win->thread = g_thread_new ( "paste-thread", (GThreadFunc)copy_win_paste_thread, win->job );

	if ( !win->src_update ) win->src_update = g_timeout_add ( 100, (GSourceFunc)copy_win_update_timeout, win );

	if ( win->debug ) g_message ( "%s:: indx %u, len %u ", __func__, indx, len );
}

static void copy_win_run ( G_GNUC_UNUSED GtkButton *button, CopyWin *win )
{
	if ( win->debug ) g_message ( "%s:: src_update %u ", __func__, win->src_update );

	if ( win->src_update ) { gmf_message_dialog ( " ", "It works ...", GTK_MESSAGE_INFO, NULL ); return; }

	win->job->cp_mv = win->cp_mv;

	win->job->indx = 0;
	win->job->indx_old = 0;
	win->job->indx_dir = 0;
	win->job->indx_err_dir = 0;

	win->job->size_dir = 0;
	win->job->size_f_cur = 0;

	win->job->copy_stop = FALSE;
	win->job->copy_done = FALSE;

	uint16_t n = 0; for ( n = 0; n < UINT16_MAX; n++ ) win->job->prg[n] = 0;

	g_cancellable_reset ( win->job->cancellable );

	win->job->path_dest = g_strdup ( gtk_entry_get_text ( win->entry ) );

	if ( g_file_test ( win->job->ferr, G_FILE_TEST_EXISTS ) ) remove ( win->job->ferr );

	gtk_expander_set_label ( win->expander, "Errors:" );

	GtkTextBuffer *buffer = (GtkTextBuffer *)gtk_text_view_get_buffer ( win->text_view );
	gtk_text_buffer_set_text ( buffer, "", -1 );

	copy_win_paste ( win );
}

static void copy_win_stop ( G_GNUC_UNUSED GtkButton *button, CopyWin *win )
{
	if ( win->debug ) g_message ( "%s:: src_update %u ", __func__, win->src_update );

	G_LOCK ( copy_progress );

	win->job->copy_stop = TRUE;

	g_cancellable_cancel ( win->job->cancellable );

	G_UNLOCK ( copy_progress );

	if ( win->thread )
	{
		if ( win->exit && win->debug ) g_message ( "%s:: wait paste-thread: ~ 200 ms ", __func__ );

		if ( win->exit ) g_thread_join ( win->thread );
		g_thread_unref ( win->thread );
	}

	win->thread = NULL;

	if ( win->src_update ) g_source_remove ( win->src_update );
	win->src_update = 0;
}

static void copy_win_clear ( G_GNUC_UNUSED GtkButton *button, CopyWin *win )
{
	gtk_expander_set_label ( win->expander, "Errors:" );

	GtkTextBuffer *buffer = (GtkTextBuffer *)gtk_text_view_get_buffer ( win->text_view );
	gtk_text_buffer_set_text ( buffer, "", -1 );

	gtk_label_set_text ( win->prg_dir, " " );

	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( win->treeview ) ) );
}

static void copy_win_icon_press_entry ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, G_GNUC_UNUSED gpointer *data )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

		g_autofree char *path = gmf_open_dir ( g_get_home_dir (), window );

		if ( path ) gtk_entry_set_text ( entry, path );
	}
}

static GtkEntry * copy_win_create_entry ( const char *text )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );

	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, "folder" );

	g_signal_connect ( entry, "icon-press", G_CALLBACK ( copy_win_icon_press_entry ), NULL );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

static void copy_win_treeview_add ( const char *name, const char *path, int ind, GtkListStore *store )
{
	gboolean is_dir  = g_file_test ( path, G_FILE_TEST_IS_DIR );
	gboolean is_link = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

	GdkPixbuf *pixbuf = file_get_pixbuf ( path, is_dir, is_link, FALSE, 24 );

	if ( !pixbuf ) pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), ( is_dir ) ? "folder" : "unknown", 24, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

	GtkTreeIter iter;
	gtk_list_store_append ( store, &iter );
	gtk_list_store_set    ( store, &iter, 0, ind, 1, pixbuf, 2, name, 3, 0, 4, " ", 5, path, -1 );

	if ( pixbuf ) g_object_unref ( pixbuf );
}

static void copy_win_add_file ( char **uris, GtkTreeView *treeview )
{
	GtkTreeModel *model = gtk_tree_view_get_model ( treeview );

	if ( uris )
	{
		uint32_t i = 0, c = 0;
		for ( i = 0; uris[i] != NULL; i++ )
		{
			if ( i >= UINT16_MAX ) { c++; continue; }

			int ind = gtk_tree_model_iter_n_children ( model, NULL );

			GFile *file = g_file_parse_name ( uris[i] );

			char *path = g_file_get_path ( file );
			char *name = g_file_get_basename ( file );

			if ( path && g_file_test ( path, G_FILE_TEST_EXISTS ) )
				copy_win_treeview_add ( name, path, ind+1, GTK_LIST_STORE ( model ) );

			free ( name );
			free ( path );

			g_object_unref ( file );
		}

		if ( i >= UINT16_MAX )
		{
			g_autofree char *add  = g_strdup_printf ( "Exceeded the limit %u", UINT16_MAX );
			g_autofree char *left = g_strdup_printf ( "Skipped %u", c );

			gmf_message_dialog ( left, add, GTK_MESSAGE_WARNING, NULL );
		}
	}
}

static void copy_win_drop_data ( GtkTreeView *treeview, GdkDragContext *context, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, 
	GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, G_GNUC_UNUSED gpointer *data )
{
	GdkAtom target = gtk_selection_data_get_data_type ( s_data );

	GdkAtom atom = gdk_atom_intern_static_string ( "text/uri-list" );

	if ( target == atom )
	{
		char **uris = gtk_selection_data_get_uris ( s_data );

		copy_win_add_file ( uris, treeview );

		g_strfreev ( uris );

		gtk_drag_finish ( context, TRUE, FALSE, time );
	}
}

static void copy_win_drag_drop ( GtkTreeView *treeview )
{
	GtkTargetEntry targets[] = { { "text/uri-list", GTK_TARGET_OTHER_WIDGET, 0 } };

	gtk_tree_view_enable_model_drag_dest ( treeview, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY );
	g_signal_connect ( treeview, "drag-data-received", G_CALLBACK ( copy_win_drop_data ), NULL );	
}

static GtkTreeView * copy_win_create_treeview ( void )
{
	GtkListStore *store = gtk_list_store_new ( 6, G_TYPE_UINT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING );

	GtkTreeView *treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );
	//gtk_tree_view_set_headers_visible ( treeview, FALSE );
	gtk_widget_set_visible ( GTK_WIDGET ( treeview ), TRUE );

	copy_win_drag_drop ( treeview );

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

static void copy_win_set_overwr ( GtkToggleButton *toggle, CopyWin *win )
{
	gboolean active = gtk_toggle_button_get_active ( toggle );

	G_LOCK ( copy_progress );
		if ( active ) win->job->bon = F_OVERWRITE;
	G_UNLOCK ( copy_progress );
}

static void copy_win_set_nname ( GtkToggleButton *toggle, CopyWin *win )
{
	gboolean active = gtk_toggle_button_get_active ( toggle );

	G_LOCK ( copy_progress );
		if ( active ) win->job->bon = F_BACKUP;
	G_UNLOCK ( copy_progress );
}

static void copy_win_set_next ( GtkToggleButton *toggle, CopyWin *win )
{
	gboolean active = gtk_toggle_button_get_active ( toggle );

	G_LOCK ( copy_progress );
		if ( active ) win->job->bon = F_NEXT;
	G_UNLOCK ( copy_progress );
}

static void copy_win_set_standard ( GtkToggleButton *toggle, CopyWin *win )
{
	gboolean active = gtk_toggle_button_get_active ( toggle );

	G_LOCK ( copy_progress );
		if ( active ) win->job->bon = F_DEF;
	G_UNLOCK ( copy_progress );
}

static GtkRadioButton * copy_win_create_radio ( const char *icon, const char *text, GtkRadioButton *radio, void ( *f ), CopyWin *win )
{
	GtkRadioButton *toggled = (GtkRadioButton *)( ( radio ) ? gtk_radio_button_new_from_widget ( radio ) : gtk_radio_button_new ( NULL ) );
	gtk_widget_set_visible ( GTK_WIDGET ( toggled ), TRUE );
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( toggled ), text );
	g_signal_connect ( toggled, "toggled", G_CALLBACK ( f ), win );

	GtkImage *image = create_image ( icon, 16 );
	gtk_button_set_image ( GTK_BUTTON ( toggled ), GTK_WIDGET ( image ) );

	return toggled;
}

static void copy_win_destroy ( G_GNUC_UNUSED GtkWindow *window, CopyWin *win )
{
	win->exit = TRUE;
	copy_win_stop ( NULL, win );

	if ( win->job->cancellable ) g_object_unref ( win->job->cancellable );

	if ( g_file_test ( win->job->ferr, G_FILE_TEST_EXISTS ) ) remove ( win->job->ferr );

	free ( win->job );
}

static void copy_win_create ( enum cm_num cp_mv, const char *path, char **uris, CopyWin *win )
{
	win->cp_mv = cp_mv;

	GtkWindow *window = GTK_WINDOW ( win );

	gtk_window_set_title ( window, cm_icons[cp_mv][1] );
	gtk_window_set_icon_name ( window, cm_icons[cp_mv][0] );
	gtk_window_set_default_size ( window, 600, 350 );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );
	g_signal_connect ( window, "destroy", G_CALLBACK ( copy_win_destroy ), win );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,  10 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	win->treeview = copy_win_create_treeview ();
	copy_win_add_file ( uris, win->treeview );

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( win->treeview ) );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( scw ), TRUE, TRUE, 0 );

	win->expander = (GtkExpander *)gtk_expander_new ( "Errors:" );
	gtk_widget_set_visible ( GTK_WIDGET ( win->expander ), TRUE );

	win->text_view = (GtkTextView *)gtk_text_view_new ();
	gtk_text_view_set_editable ( win->text_view, FALSE );
	gtk_text_view_set_wrap_mode ( win->text_view, GTK_WRAP_WORD );
	gtk_text_view_set_left_margin ( win->text_view, 10 );
	gtk_text_view_set_right_margin ( win->text_view, 10 );
	gtk_widget_set_visible ( GTK_WIDGET ( win->text_view ), TRUE );

	GtkScrolledWindow *sw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_min_content_height ( sw, 100 );
	gtk_scrolled_window_set_shadow_type ( sw, GTK_SHADOW_IN );
	gtk_scrolled_window_set_policy ( sw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( GTK_CONTAINER ( sw ), GTK_WIDGET ( win->text_view ) );
	gtk_widget_set_visible ( GTK_WIDGET ( sw ), TRUE );

	gtk_container_add ( GTK_CONTAINER ( win->expander ), GTK_WIDGET ( sw ) );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( win->expander ), FALSE, FALSE, 0 );

	const char *labels[] = { "window-close", "edit-clear", "media-playback-stop", cm_icons[cp_mv][0] };
	const void *funcs[] = { gtk_widget_destroy, copy_win_clear, copy_win_stop, copy_win_run };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( labels ); c++ )
	{
		GtkButton *button = button_set_image ( labels[c], 16 );

		( c == 0 ) ? g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( funcs[c] ), window )
			   : g_signal_connect ( button, "clicked", G_CALLBACK ( funcs[c] ), win );

		gtk_widget_set_visible (  GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( button  ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	win->entry = copy_win_create_entry ( path );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( win->entry ), TRUE, TRUE, 0 );
	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	win->prg_dir = (GtkLabel *)gtk_label_new ( " " );
	gtk_widget_set_visible ( GTK_WIDGET ( win->prg_dir ), TRUE );
	gtk_widget_set_halign ( GTK_WIDGET ( win->prg_dir ), GTK_ALIGN_START );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( win->prg_dir ), TRUE, TRUE, 0 );

	GtkRadioButton *toggled = copy_win_create_radio ( "edit-paste", "Default", NULL, copy_win_set_standard, win );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( toggled ), FALSE, FALSE, 0 );

	toggled = copy_win_create_radio ( "next", "Ignore ( if the file exists )", toggled, copy_win_set_next, win );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( toggled ), FALSE, FALSE, 0 );

	toggled = copy_win_create_radio ( "gtk-edit", "New-name ( if the file exists )", toggled, copy_win_set_nname, win );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( toggled ), FALSE, FALSE, 0 );

	toggled = copy_win_create_radio ( "revert", "Overwrite ( if the file exists )", toggled, copy_win_set_overwr, win );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( toggled ), FALSE, FALSE, 0 );

	gtk_box_pack_end ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( v_box ), 10 );
	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );
}

static void copy_win_init ( CopyWin *win )
{
	win->thread = NULL;
	win->exit   = FALSE;
	win->cp_mv  = CM_CP;
	win->src_update = 0;

	win->job = g_new0 ( CopyData, 1 );
	win->job->bon = F_DEF;
	win->job->cp_mv = CM_CP;
	win->job->copy_done = FALSE;
	win->job->cancellable = g_cancellable_new ();

	win->job->ferr = "/tmp/gmf-copy-err";
	win->debug = win->job->copy_debug = ( g_getenv ( "GMF_DEBUG" ) ) ? TRUE : FALSE;
}

static void copy_win_finalize ( GObject *object )
{
	G_OBJECT_CLASS (copy_win_parent_class)->finalize (object);
}

static void copy_win_class_init ( CopyWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = copy_win_finalize;
}

CopyWin * copy_win_new ( enum cm_num cp_mv, const char *path, char **uris, GApplication *app, double opacity )
{
	CopyWin *win = ( app ) ? g_object_new ( COPY_TYPE_WIN, "application", app, NULL ) : g_object_new ( COPY_TYPE_WIN, NULL );

	copy_win_create ( cp_mv, path, uris, win );

	gtk_window_present ( GTK_WINDOW ( win ) );
	gtk_widget_set_opacity ( GTK_WIDGET ( win ), opacity );

	return win;
}
