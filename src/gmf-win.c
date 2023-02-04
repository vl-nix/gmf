/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-win.h"
#include "info-win.h"
#include "gmf-dialog.h"

#include <errno.h>
#include <glib/gstdio.h>

#define MAX_TAB 8
#define PREVIEW 100
#define ITEM_WIDTH 80
#define UNUSED G_GNUC_UNUSED

G_LOCK_DEFINE_STATIC ( progress_copy );

typedef unsigned int uint;

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

enum copy_cut_enm
{
	COPY,
	MOVE
};

enum act_enm 
{
	ACT_NDR,
	ACT_NFL,
	ACT_EDT
};

enum bt_enm 
{
	BT_WIN,
	BT_TAB,
	BT_GUP,
	BT_JMP,
	BT_ACT,
	BT_SRH,
	BT_RLD,
	BT_INF,
	BT_PRF,
	BT_ALL
};

const char *bt_icon_n[BT_ALL] = 
{
	[BT_WIN] = "window-new",
	[BT_TAB] = "tab-new",
	[BT_GUP] = "go-up",
	[BT_JMP] = "go-jump",
	[BT_ACT] = "system-run",
	[BT_SRH] = "system-search",
	[BT_RLD] = "reload",
	[BT_INF] = "dialog-information",
	[BT_PRF] = "preferences-system"
};

enum bt_ppa_enm
{
	BTP_NDR,
	BTP_NFL,
	BTP_PST,
	BTP_HDN,
	BTP_TRM,
	BTP_AAL
};

enum bt_ppb_enm
{
	BTP_EDT,
	BTP_CPY,
	BTP_CUT,
	BTP_RMV,
	BTP_ARC,
	BTP_RUN,
	BTP_BAL
};

const char *bpa_icon_n[BTP_AAL] = 
{
	[BTP_NDR] = "folder-new",
	[BTP_NFL] = "document-new",
	[BTP_PST] = "edit-paste", 
	[BTP_HDN] = "䷽",
	[BTP_TRM] = "terminal"
};

const char *bpb_icon_n[BTP_BAL] = 
{
	[BTP_EDT] = "gtk-edit",
	[BTP_CPY] = "edit-copy", 
	[BTP_CUT] = "edit-cut",
	[BTP_RMV] = "remove",
	[BTP_ARC] = "archive-insert-directory",
	[BTP_RUN] = "system-run"
};

struct _GmfWin
{
	GtkWindow parent_instance;

	GtkPlacesSidebar *psb;
	GtkIconView *icon_view;

	GtkScrolledWindow *scw;

	GtkButton  *button_prf;
	GtkPopover *popover_pref;

	GtkButton  *button_act;
	GtkPopover *popover_act_a;
	GtkPopover *popover_act_b;

	GtkPopover *popover_tab;
	GtkButton  *button_tabs;
	GtkButton  *button_tab[MAX_TAB];

	GtkEntry   *entry_jump;
	GtkButton  *button_jump;
	GtkButton  *button_search;
	GtkPopover *popover_jump;
	GtkPopover *popover_search;

	GFile *file;
	GFileMonitor *monitor;

	uint16_t   mod_t;
	uint16_t   end_t;
	uint16_t limit_t;

	uint16_t width;
	uint16_t height;

	uint8_t opacity;
	uint16_t icon_size;

	gboolean dark;
	gboolean hidden;
	gboolean preview;
	gboolean unmount_set_home;

	gboolean debug;

	// Copy

	GFile *file_copy;
	GFile *file_paste;

	GtkLabel *label_prg_indx;
	GtkLabel *label_prg_size;
	GtkPopover *popover_act_copy;
	GtkProgressBar *bar_prg_dir_copy;
	GtkProgressBar *bar_prg_file_copy;

	GCancellable *cancellable_copy;

	char **uris_copy;
	GList *list_err_copy;

	uint8_t prg_copy;
	uint16_t indx_all;
	uint16_t indx_copy;

	uint64_t size_file;
	uint64_t size_file_cur;
	uint64_t size_file_all;
	uint64_t size_file_copy;

	enum copy_cut_enm cm_num;

	gboolean done_copy;
};

G_DEFINE_TYPE ( GmfWin, gmf_win, GTK_TYPE_WINDOW )

typedef void ( *fp ) ( GmfWin * );

static void gmf_win_set_file ( const char *, GmfWin * );
static void gmf_win_icon_press_act ( GmfWin * );
static void gmf_win_icon_open_location ( const char *, const char *, GmfWin * );

// ***** Copy *****

static void copy_file_progress ( int64_t current, int64_t total, gpointer data )
{
	GmfWin *win = (GmfWin *)data;

	uint8_t prg = ( total ) ? ( uint8_t )( current * 100 / total ) : 0;

	G_LOCK ( progress_copy );

		win->prg_copy = prg;
		win->size_file_cur = (uint64_t)current;

		if ( current == total )
		{
			if ( win->size_file == (uint64_t)total ) { win->size_file_copy += total; win->size_file = 0; } else win->size_file = total;
		}

	G_UNLOCK ( progress_copy );
}

static void copy_error ( GError *error, GmfWin *win )
{
	G_LOCK ( progress_copy );
		win->indx_copy--;
		win->list_err_copy = g_list_append ( win->list_err_copy, g_strdup ( error->message ) );
	G_UNLOCK ( progress_copy );

	g_debug ( "%s:: %s ", __func__, error->message );

	g_error_free ( error );
}

static void move ( GFile *file_copy, GFile *file_paste, GmfWin *win )
{
	G_LOCK ( progress_copy );
		win->indx_copy++;
		win->size_file = 0;
	G_UNLOCK ( progress_copy );

	GError *error = NULL;
	g_file_move ( file_copy, file_paste, G_FILE_COPY_NOFOLLOW_SYMLINKS, win->cancellable_copy, copy_file_progress, win, &error );

	if ( error ) copy_error ( error, win );
}

static void copy_file ( GFile *file_copy, GFile *file_paste, GmfWin *win )
{
	G_LOCK ( progress_copy );
		win->indx_copy++;
		win->size_file = 0;
	G_UNLOCK ( progress_copy );

	GError *error = NULL;
	g_file_copy ( file_copy, file_paste, G_FILE_COPY_NOFOLLOW_SYMLINKS, win->cancellable_copy, copy_file_progress, win, &error );

	if ( error ) copy_error ( error, win );
}

static void copy_dir ( GFile *file_copy, GFile *file_paste, GmfWin *win )
{
	gboolean break_f = FALSE;

	G_LOCK ( progress_copy );
		win->indx_copy++;
		if ( win->indx_copy >= UINT16_MAX || g_cancellable_is_cancelled ( win->cancellable_copy ) ) break_f = TRUE;
	G_UNLOCK ( progress_copy );

	if ( break_f ) return;

	GError *error = NULL;
	g_file_make_directory ( file_paste, NULL, &error );

	if ( error )
	{
		copy_error ( error, win );

		if ( !g_file_query_exists ( file_paste, NULL ) ) return;
	}

	g_autofree char *dir_src = g_file_get_path ( file_copy  );
	g_autofree char *dir_dst = g_file_get_path ( file_paste );

	GDir *dir = g_dir_open ( dir_src, 0, NULL );

	if ( dir )
	{
		const char *name = NULL;

		while ( ( name = g_dir_read_name ( dir ) ) != NULL )
		{
			G_LOCK ( progress_copy );
				if ( win->indx_copy >= UINT16_MAX || g_cancellable_is_cancelled ( win->cancellable_copy ) ) break_f = TRUE;
			G_UNLOCK ( progress_copy );

			if ( break_f ) break;

			GFile *new_file_copy  = g_file_new_build_filename ( dir_src, name, NULL );
			GFile *new_file_paste = g_file_new_build_filename ( dir_dst, name, NULL );

			GFileType ftype = g_file_query_file_type ( new_file_copy, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL );

			if ( ftype == G_FILE_TYPE_DIRECTORY )
			{
				copy_dir ( new_file_copy, new_file_paste, win ); // Recursion!
			}
			else
			{
				copy_file ( new_file_copy, new_file_paste, win );
			}

			g_object_unref ( new_file_copy  );
			g_object_unref ( new_file_paste );
		}

		g_dir_close ( dir );
	}
}

static void copy_dir_file ( const char *uri, GmfWin *win )
{
	enum copy_cut_enm cm_num;
	g_autofree char *dir = NULL;

	G_LOCK ( progress_copy );
		cm_num = win->cm_num;
		dir = g_file_get_path ( win->file );
	G_UNLOCK ( progress_copy );

	win->file_copy = g_file_parse_name ( uri );

	g_autofree char *name = g_file_get_basename ( win->file_copy );

	win->file_paste = g_file_new_build_filename ( dir, name, NULL );

	GFileType ftype = g_file_query_file_type ( win->file_copy, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL );

	if ( cm_num == MOVE )
	{
		move ( win->file_copy, win->file_paste, win );
	}
	else
	{
		if ( ftype == G_FILE_TYPE_DIRECTORY )
			copy_dir ( win->file_copy, win->file_paste, win );
		else
			copy_file ( win->file_copy, win->file_paste, win );
	}

	g_object_unref ( win->file_copy  );
	g_object_unref ( win->file_paste );
}

static void copy_set_all_size ( const char *path, GmfWin *win )
{
	ulong size = get_file_size ( path );

	G_LOCK ( progress_copy );
		win->size_file_all += size;
	G_UNLOCK ( progress_copy );
}

static uint16_t copy_dir_n ( const char *path, GmfWin *win )
{
	uint16_t nums = 0;
	const char *name = NULL;
	gboolean break_f = FALSE;

	GDir *dir = g_dir_open ( path, 0, NULL );

	while ( ( name = g_dir_read_name (dir) ) )
	{
		G_LOCK ( progress_copy );
			if ( nums >= UINT16_MAX || g_cancellable_is_cancelled ( win->cancellable_copy ) ) break_f = TRUE;
		G_UNLOCK ( progress_copy );

		if ( break_f ) break;

		char *path_new = g_strconcat ( path, "/", name, NULL );

		gboolean is_dir  = g_file_test ( path_new, G_FILE_TEST_IS_DIR );
		gboolean is_link = g_file_test ( path_new, G_FILE_TEST_IS_SYMLINK );

		if ( !is_link )
		{
			if ( is_dir ) { nums += copy_dir_n ( path_new, win ); } else { copy_set_all_size ( path_new, win ); }
		}

		nums++;
		free ( path_new );
	}

	g_dir_close ( dir );

	return nums;
}

static void copy_get_all_indx ( GmfWin *win )
{
	uint16_t nums = 0;
	enum copy_cut_enm cm_num;
	gboolean break_f = FALSE;

	G_LOCK ( progress_copy );
		cm_num = win->cm_num;
	G_UNLOCK ( progress_copy );

	uint16_t i = 0; for ( i = 0; win->uris_copy[i] != NULL; i++ )
	{
		G_LOCK ( progress_copy );
			if ( nums >= UINT16_MAX || g_cancellable_is_cancelled ( win->cancellable_copy ) ) break_f = TRUE;
		G_UNLOCK ( progress_copy );

		if ( break_f ) break;

		if ( cm_num == COPY )
		{
			char *path = g_filename_from_uri ( win->uris_copy[i], NULL, NULL );

			gboolean is_dir  = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_link = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			if ( !is_link )
			{
				if ( is_dir ) { nums += copy_dir_n ( path, win ); } else { copy_set_all_size ( path, win ); }
			}

			free ( path );
		}

		nums++;
	}

	G_LOCK ( progress_copy );
		win->indx_all = nums;
	G_UNLOCK ( progress_copy );
}

static gpointer copy_dir_file_thread ( GmfWin *win )
{
	uint i = 0;
	gboolean break_f = FALSE;

	copy_get_all_indx ( win );

	for ( i = 0; win->uris_copy[i] != NULL; i++ )
	{
		G_LOCK ( progress_copy );
			if ( win->indx_copy >= UINT16_MAX || g_cancellable_is_cancelled ( win->cancellable_copy ) ) break_f = TRUE;
		G_UNLOCK ( progress_copy );

		if ( break_f ) break;

		copy_dir_file ( win->uris_copy[i], win );

		g_debug ( "%s:: uri: %s ", __func__, win->uris_copy[i] );
	}

	G_LOCK ( progress_copy );
		win->done_copy = TRUE;
	G_UNLOCK ( progress_copy );

	g_strfreev ( win->uris_copy );

	return NULL;
}

static void copy_file_clear ( GmfWin *win )
{
	if ( win->list_err_copy ) { g_list_free_full ( win->list_err_copy, (GDestroyNotify) free ); win->list_err_copy = NULL; }
}

static gboolean copy_update_timeout ( GmfWin *win )
{
	if ( !GTK_IS_WIDGET ( win->icon_view ) ) return FALSE;

	uint8_t prg_copy = 0;
	uint16_t indx_all = 0;
	uint16_t indx_copy = 0;

	uint64_t size_file_cur = 0;
	uint64_t size_file_all = 0;
	uint64_t size_file_copy = 0;

	enum copy_cut_enm cm_num;

	G_LOCK ( progress_copy );

	prg_copy = win->prg_copy;
	indx_all  = win->indx_all;
	indx_copy = win->indx_copy;

	size_file_cur = win->size_file_cur;
	size_file_all = win->size_file_all;
	size_file_copy = win->size_file_copy;

	cm_num = win->cm_num;

	G_UNLOCK ( progress_copy );

	char text[100];
	sprintf ( text, " %u / %u ", indx_copy, indx_all );

	gtk_label_set_text ( win->label_prg_indx, text );

	uint8_t prg = 0;

	if ( cm_num == MOVE )
		prg = ( indx_all ) ? ( uint8_t )( indx_copy * 100 / indx_all ) : 0;
	else
		prg = ( size_file_all ) ? ( uint8_t )( size_file_copy * 100 / size_file_all ) : 0;

	gtk_progress_bar_set_fraction ( win->bar_prg_dir_copy, (double)prg / 100 );

	if ( size_file_all )
	{
		g_autofree char *str_sz_all  = g_format_size ( size_file_all );
		g_autofree char *str_sz_copy = g_format_size ( ( size_file_copy ) ? size_file_copy : size_file_cur );

		char text_sz[256];
		sprintf ( text_sz, " %s / %s ", str_sz_copy, str_sz_all );

		gtk_label_set_text ( win->label_prg_size, text_sz );
	}

	gtk_progress_bar_set_fraction ( win->bar_prg_file_copy, (double)prg_copy / 100 );

	if ( win->done_copy )
	{
		if ( win->list_err_copy == NULL )
			gtk_progress_bar_set_fraction ( win->bar_prg_file_copy, 1.0 );
		else
			gmf_dialog_copy_dir_err ( win->list_err_copy, GTK_WINDOW ( win ) );

		copy_file_clear ( win );

		return FALSE;
	}

	return TRUE;
}

static void copy_dir_file_run_thread ( GmfWin *win )
{
	win->done_copy = FALSE;

	win->prg_copy  = 0;
	win->indx_all  = 0;
	win->indx_copy = 0;

	win->size_file_all  = 0;
	win->size_file_copy = 0;

	g_cancellable_reset ( win->cancellable_copy );

	gtk_button_clicked ( win->button_act );
	gtk_label_set_text ( win->label_prg_indx, " 0 / 0 " );
	gtk_label_set_text ( win->label_prg_size, " 0 / 0 " );
	gtk_progress_bar_set_fraction ( win->bar_prg_dir_copy,  0 );
	gtk_progress_bar_set_fraction ( win->bar_prg_file_copy, 0 );

	GThread *thread = g_thread_new ( NULL, (GThreadFunc)copy_dir_file_thread, win );
	if ( thread ) g_thread_unref ( thread );

	g_timeout_add ( 100, (GSourceFunc)copy_update_timeout, win );
}

static void gmf_win_signal_ppa_copy_cancel ( UNUSED GtkButton *button, GmfWin *win )
{
	G_LOCK ( progress_copy );
		g_cancellable_cancel ( win->cancellable_copy );
	G_UNLOCK ( progress_copy );
}

static GtkPopover * gmf_win_pp_act_copy ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkBox *h_box_a = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box_a, 50 );

	win->label_prg_indx = (GtkLabel *)gtk_label_new ( " 0 / 0 " );
	gtk_widget_set_halign ( GTK_WIDGET ( win->label_prg_indx ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( win->label_prg_indx ), TRUE );
	gtk_box_pack_start ( h_box_a, GTK_WIDGET ( win->label_prg_indx ), TRUE, TRUE, 0 );

	win->label_prg_size = (GtkLabel *)gtk_label_new ( " 0 / 0 " );
	gtk_widget_set_halign ( GTK_WIDGET ( win->label_prg_indx ), GTK_ALIGN_END );
	gtk_widget_set_visible ( GTK_WIDGET ( win->label_prg_size ), TRUE );
	gtk_box_pack_end ( h_box_a, GTK_WIDGET ( win->label_prg_size ), TRUE, TRUE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box_a ), TRUE );

	GtkBox *h_box_b = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box_b, 5 );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box_a ), FALSE, FALSE, 0 );

	win->bar_prg_dir_copy = (GtkProgressBar *)gtk_progress_bar_new ();
	gtk_progress_bar_set_show_text ( win->bar_prg_dir_copy, TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->bar_prg_dir_copy ), TRUE );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( win->bar_prg_dir_copy ), TRUE, TRUE, 0 );

	win->bar_prg_file_copy = (GtkProgressBar *)gtk_progress_bar_new ();
	gtk_progress_bar_set_show_text ( win->bar_prg_file_copy, TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->bar_prg_file_copy ), TRUE );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( win->bar_prg_file_copy ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_box_pack_start ( h_box_b, GTK_WIDGET ( v_box ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box_b ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( h_box_b ), FALSE, FALSE, 0 );

	GtkBox *h_box_c = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box_c, 5 );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "gtk-cancel", GTK_ICON_SIZE_MENU );
	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box_c, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	g_signal_connect ( button, "clicked", G_CALLBACK ( gmf_win_signal_ppa_copy_cancel ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box_c ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( h_box_c ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );
	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );

	return popover;
}

// ***** Places-Sidebar *****

static void gmf_win_psb_open_location ( UNUSED GtkPlacesSidebar *sb, GObject *location, UNUSED GtkPlacesOpenFlags flags, GmfWin *win )
{
	GFile *file = (GFile *)location;

	g_autofree char *path = g_file_get_path ( file );

	if ( path )
	{
		gmf_win_set_file ( path, win );
	}
	else
	{
		g_autofree char *uri = g_file_get_uri ( file );

		if ( g_str_has_prefix ( uri, "trash"  ) ) gmf_dialog_trash  ( GTK_WINDOW ( win ) );
		if ( g_str_has_prefix ( uri, "recent" ) ) gmf_dialog_recent ( GTK_WINDOW ( win ) );
	}
}

static void gmf_win_psb_popup ( UNUSED GtkPlacesSidebar *sb, UNUSED GtkWidget *container, UNUSED GFile *s_item, GVolume *s_vol, GmfWin *win )
{
	win->unmount_set_home = FALSE;

	if ( s_vol )
	{
		GMount *mount = g_volume_get_mount ( s_vol );

		if ( mount )
		{
			GFile *mount_root = g_mount_get_root ( mount );

			if ( g_file_has_prefix ( win->file, mount_root ) ) win->unmount_set_home = TRUE;

			if ( mount_root ) g_object_unref ( mount_root );
			g_object_unref ( mount );
		}
	}
}

static void gmf_win_psb_unmount ( UNUSED GtkPlacesSidebar *sb, UNUSED GMountOperation *op, GmfWin *win )
{
	if ( win->unmount_set_home ) gmf_win_set_file ( g_get_home_dir (), win );

	win->unmount_set_home = FALSE;
}

// ***** Tool-Bar *****

static void gmf_win_wnw ( GmfWin *win )
{
	g_autofree char *prog_gmf = g_find_program_in_path ( "gmf" );

	if ( !prog_gmf )
	{
		gmf_dialog_message ( "Not Found:", "Gmf", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );

		return;
	}

	g_autofree char *path = g_file_get_path ( win->file );

	char cmd[PATH_MAX];
	sprintf ( cmd, "%s %s", prog_gmf, path );

	gmf_launch_cmd ( cmd, GTK_WINDOW ( win ) );
}

static void gmf_win_tab ( GmfWin *win )
{
	gtk_popover_set_relative_to ( win->popover_tab, GTK_WIDGET ( win->button_tabs ) );
	gtk_popover_popup ( win->popover_tab );
}

static void gmf_win_gup ( GmfWin *win )
{
	GFile *dir = g_file_get_parent ( win->file );

	if ( dir == NULL ) return;

	g_autofree char *path = g_file_get_path ( dir );

	gmf_win_set_file ( path, win );

	g_object_unref ( dir );
}

static void gmf_win_gjm ( GmfWin *win )
{
	gtk_popover_set_relative_to ( win->popover_jump, GTK_WIDGET ( win->button_jump ) );
	gtk_popover_popup ( win->popover_jump );

	g_autofree char *path = g_file_get_path ( win->file );
	gtk_entry_set_text ( win->entry_jump, path );
}

static void gmf_win_act ( GmfWin *win )
{
	gmf_win_icon_press_act ( win );
}

static void gmf_win_srh ( GmfWin *win )
{
	gtk_popover_set_relative_to ( win->popover_search, GTK_WIDGET ( win->button_search ) );
	gtk_popover_popup ( win->popover_search );
}

static void gmf_win_rld ( GmfWin *win )
{
	g_autofree char *path = g_file_get_path ( win->file );

	gmf_win_icon_open_location ( path, NULL, win );
}

static void gmf_win_inf ( GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	g_autofree char *dir = g_file_get_path ( win->file );
	double opacity = gtk_widget_get_opacity ( GTK_WIDGET ( win ) );

	if ( !list ) { info_win_new ( I_SEL_0, dir, NULL, opacity ); return; }

	if ( g_list_length ( list ) == 1 )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		info_win_new ( I_SEL_1, dir, path, opacity );

		free ( path );
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_prf ( GmfWin *win )
{
	gtk_popover_set_relative_to ( win->popover_pref, GTK_WIDGET ( win->button_prf ) );
	gtk_popover_popup ( win->popover_pref );
}

static void gmf_win_bar_signal_all_buttons ( GtkButton *button, GmfWin *win )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	fp funcs[] =  { gmf_win_wnw, gmf_win_tab, gmf_win_gup, gmf_win_gjm, gmf_win_act, gmf_win_srh, gmf_win_rld, gmf_win_inf, gmf_win_prf };

	if ( funcs[num] ) funcs[num] ( win );
}

static GtkBox * gmf_win_bar_create ( GmfWin *win )
{
	GtkBox *box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	enum bt_enm n = BT_WIN;

	for ( n = BT_WIN; n < BT_ALL; n++ )
	{
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( bt_icon_n[n], GTK_ICON_SIZE_MENU );

		gtk_button_set_relief ( button, GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
		gtk_box_pack_start ( box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

		char buf[20];
		sprintf ( buf, "%d", n );
		gtk_widget_set_name ( GTK_WIDGET ( button ), buf );

		if ( n == BT_TAB ) win->button_tabs = button;
		if ( n == BT_ACT ) win->button_act  = button;
		if ( n == BT_JMP ) win->button_jump   = button;
		if ( n == BT_SRH ) win->button_search = button;
		if ( n == BT_PRF ) win->button_prf    = button;

		g_signal_connect ( button, "clicked", G_CALLBACK ( gmf_win_bar_signal_all_buttons ), win );
	}

	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	return box;
}

static char * gmf_win_open_dir ( const char *dir, GtkWindow *window )
{
	return gmf_dialog_open_dir ( dir, "gtk-open", "document-open", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, window );
}

static void gmf_win_popover_jump ( GtkEntry *entry, GtkEntryIconPosition icon_pos, UNUSED GdkEventButton *event, GmfWin *win )
{
	const char *text = gtk_entry_get_text ( entry );

	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY )
	{
		g_autofree char *path = gmf_win_open_dir ( text, GTK_WINDOW ( win ) );

		if ( path == NULL ) return;

		gtk_entry_set_text ( entry, path );
	}

	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		gmf_win_set_file ( text, win );
	}
}

static void gmf_win_popover_search ( GtkEntry *entry, UNUSED GtkEntryIconPosition icon_pos, UNUSED GdkEventButton *event, GmfWin *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY )
	{
		gtk_entry_set_text ( entry, "" );

		g_autofree char *path = g_file_get_path ( win->file );

		gmf_win_icon_open_location ( path, NULL, win );
	}

	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		const char *text = gtk_entry_get_text ( entry );
		g_autofree char *text_down = g_utf8_strdown ( text, -1 );

		g_autofree char *path = g_file_get_path ( win->file );

		gmf_win_icon_open_location ( path, text_down, win );
	}
}

static GtkPopover * gmf_win_popover_js ( const char *icon_a, const char *icon_b, const char *text, enum bt_enm be, GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, icon_a );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, icon_b );
	g_signal_connect ( entry, "icon-press", G_CALLBACK ( ( be == BT_JMP ) ? gmf_win_popover_jump : gmf_win_popover_search ), win );

	if ( be == BT_JMP ) gtk_widget_set_size_request ( GTK_WIDGET ( entry ), 500, -1 );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	if ( be == BT_JMP ) win->entry_jump = entry;

	return popover;
}

static void gmf_win_dir_new ( GtkEntry *entry, GmfWin *win )
{
	const char *text = gtk_entry_get_text ( entry );
	g_autofree char *path = g_file_get_path ( win->file );

	GFile *file_new = g_file_new_build_filename ( path, text, NULL );

	GError *error = NULL;
	g_file_make_directory ( file_new, NULL, &error );

	if ( error ) { gmf_dialog_message ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

	g_object_unref ( file_new );

	GtkWidget *popover = gtk_widget_get_parent ( GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( popover, FALSE );
}

static void gmf_win_popover_dir_new ( GtkEntry *entry, GtkEntryIconPosition icon_pos, UNUSED GdkEventButton *event, GmfWin *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY ) gmf_win_dir_new ( entry, win );
}

static void gmf_win_file_new ( GtkEntry *entry, GmfWin *win )
{
	const char *text = gtk_entry_get_text ( entry );
	g_autofree char *path = g_file_get_path ( win->file );

	GFile *file_new = g_file_new_build_filename ( path, text, NULL );

	GError *error = NULL;
	g_file_create ( file_new, G_FILE_CREATE_NONE, NULL, &error );

	if ( error ) { gmf_dialog_message ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

	g_object_unref ( file_new );

	GtkWidget *popover = gtk_widget_get_parent ( GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( popover, FALSE );
}

static void gmf_win_popover_file_new ( GtkEntry *entry, GtkEntryIconPosition icon_pos, UNUSED GdkEventButton *event, GmfWin *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY ) gmf_win_file_new ( entry, win );
}

static void gmf_win_name_new ( GtkEntry *entry, GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( !list ) return;

	if ( g_list_length ( list ) == 1 )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		const char *text = gtk_entry_get_text ( entry );

		GFile *file = g_file_new_for_path ( path );

		GError *error = NULL;
		g_file_set_display_name ( file, text, NULL, &error );

		if ( error ) { gmf_dialog_message ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

		free ( path );
		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	GtkWidget *popover = gtk_widget_get_parent ( GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( popover, FALSE );
}

static void gmf_win_popover_name_new ( GtkEntry *entry, GtkEntryIconPosition icon_pos, UNUSED GdkEventButton *event, GmfWin *win )
{
	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY ) gmf_win_name_new ( entry, win );
}

static void gmf_win_activate_dir ( GtkEntry *entry, GmfWin *win )
{
	gmf_win_dir_new ( entry, win );
}

static void gmf_win_activate_file ( GtkEntry *entry, GmfWin *win )
{
	gmf_win_file_new ( entry, win );
}

static void gmf_win_activate_name ( GtkEntry *entry, GmfWin *win )
{
	gmf_win_name_new ( entry, win );
}

static GtkPopover * gmf_win_popover_df_new ( const char *icon_a, const char *icon_b, const char *text, uint8_t bpe, GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );

	if ( icon_a ) gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, icon_a );
	if ( icon_b ) gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, icon_b );

	if ( ( bpe == ACT_NDR ) ) g_signal_connect ( entry, "activate", G_CALLBACK ( gmf_win_activate_dir  ), win );
	if ( ( bpe == ACT_NFL ) ) g_signal_connect ( entry, "activate", G_CALLBACK ( gmf_win_activate_file ), win );
	if ( ( bpe == ACT_EDT ) ) g_signal_connect ( entry, "activate", G_CALLBACK ( gmf_win_activate_name ), win );

	if ( ( bpe == ACT_NDR ) ) g_signal_connect ( entry, "icon-press", G_CALLBACK ( gmf_win_popover_dir_new  ), win );
	if ( ( bpe == ACT_NFL ) ) g_signal_connect ( entry, "icon-press", G_CALLBACK ( gmf_win_popover_file_new ), win );
	if ( ( bpe == ACT_EDT ) ) g_signal_connect ( entry, "icon-press", G_CALLBACK ( gmf_win_popover_name_new ), win );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return popover;
}

static void gmf_win_get_clipboard ( GmfWin *win )
{
	GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

	char *text = gtk_clipboard_wait_for_text ( clipboard );

	if ( text )
	{
		char **split = g_strsplit ( text, "\n", 0 );
		uint j = 0, len = g_strv_length ( split );

		char **uris = g_malloc ( ( len + 1 ) * sizeof (char *) );

		for ( j = 0; j < len; j++ ) uris[j] = g_filename_to_uri ( g_strstrip ( split[j] ), NULL, NULL );

		uris[j] = NULL;

		win->uris_copy = uris;
		copy_dir_file_run_thread ( win );

		g_strfreev ( split );
		free ( text );

		gtk_clipboard_clear ( clipboard );
	}
}

static void gmf_win_set_clipboard ( GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( !list ) return;

	uint16_t j = 0;
	GString *gstring = g_string_new ( NULL );

	while ( list != NULL )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		if ( j ) g_string_append ( gstring, "\n" );
		g_string_append ( gstring, path );

		j++;
		free ( path );
		list = list->next;
	}

	GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

	gtk_clipboard_set_text ( clipboard, gstring->str, -1 );

	g_string_free ( gstring, TRUE );
	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_drn ( GmfWin *win )
{
	GtkPopover *popover = gmf_win_popover_df_new ( NULL, "folder-new", "New-folder", ACT_NDR, win );

	gtk_popover_set_relative_to ( popover, GTK_WIDGET ( win->button_act ) );
	gtk_popover_popup ( popover );
}

static void gmf_win_fln ( GmfWin *win )
{
	GtkPopover *popover = gmf_win_popover_df_new ( NULL, "document-new", "New-document", ACT_NFL, win );

	gtk_popover_set_relative_to ( popover, GTK_WIDGET ( win->button_act ) );
	gtk_popover_popup ( popover );
}

static void gmf_win_pst ( GmfWin *win )
{
	gmf_win_get_clipboard ( win );
}

static void gmf_win_hdn ( GmfWin *win )
{
	win->hidden = !win->hidden;

	gmf_win_rld ( win );
}

static void gmf_win_tmn ( GmfWin *win )
{
	g_autofree char *pwd = g_get_current_dir ();

	g_autofree char *path = g_file_get_path ( win->file );

	g_chdir ( path );

	gmf_launch_cmd ( "x-terminal-emulator", GTK_WINDOW ( win ) );

	g_chdir ( pwd );
}

static void gmf_win_edt ( GmfWin *win )
{
	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );
	uint length = g_list_length ( list );

	if ( !list ) return;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	g_autofree char *name = NULL;
	gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
	gtk_tree_model_get ( model, &iter, COL_NAME, &name, -1 );

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	if ( length != 1 ) return;

	GtkPopover *popover = gmf_win_popover_df_new ( NULL, "gtk-edit", name, ACT_EDT, win );

	gtk_popover_set_relative_to ( popover, GTK_WIDGET ( win->button_act ) );
	gtk_popover_popup ( popover );
}

static void gmf_win_cpy ( GmfWin *win )
{
	win->cm_num = COPY;

	gmf_win_set_clipboard ( win );
}

static void gmf_win_cut ( GmfWin *win )
{
	win->cm_num = MOVE;

	gmf_win_set_clipboard ( win );
}

static void gmf_win_rmv ( GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( !list ) return;

	while ( list != NULL )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		GFile *file = g_file_new_for_path ( path );

		GError *error = NULL;
		g_file_trash ( file, NULL, &error );

		if ( error ) { gmf_dialog_message ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

		list = list->next;

		free ( path );
		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_arc ( GmfWin *win )
{
	g_autofree char *prog_roller = g_find_program_in_path ( "file-roller" );
	g_autofree char *prog_engrampa = g_find_program_in_path ( "engrampa" );

	if ( !prog_roller && !prog_engrampa )
	{
		gmf_dialog_message ( "Not Found:", "File-roller or Engrampa", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );

		return;
	}

	const char *cmd = ( !prog_roller && prog_engrampa ) ? "engrampa --add " : "file-roller --add ";

	GString *gstring = g_string_new ( cmd );

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( !list ) return;

	while ( list != NULL )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		g_string_append_printf ( gstring, "'%s' ", path );

		free ( path );
		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	gmf_launch_cmd ( gstring->str, GTK_WINDOW ( win ) );

	g_string_free ( gstring, TRUE );
}

static void gmf_win_run ( GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( !list ) return;

	if ( g_list_length ( list ) == 1 )
	{
		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		GFile *file = g_file_new_for_path ( path );

		gmf_dialog_app_chooser ( file, GTK_WINDOW ( win ) );

		free ( path );
		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_signal_all_ppa_num ( GtkButton *button, GmfWin *win )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	fp funcs[] =  { gmf_win_drn, gmf_win_fln, gmf_win_pst, gmf_win_hdn, gmf_win_tmn };

	if ( funcs[num] ) funcs[num] ( win );

	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_act_a ), FALSE );
}

static void gmf_win_signal_all_ppb_num ( GtkButton *button, GmfWin *win )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	fp funcs[] =  { gmf_win_edt, gmf_win_cpy, gmf_win_cut, gmf_win_rmv, gmf_win_arc, gmf_win_run };

	if ( funcs[num] ) funcs[num] ( win );

	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_act_b ), FALSE );
}

static GtkPopover * gmf_win_pp_act_a ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	enum bt_ppa_enm n = BTP_NDR;

	for ( n = BTP_NDR; n < BTP_AAL; n++ )
	{
		GtkButton *button = ( n == BTP_HDN ) ? (GtkButton *)gtk_button_new_with_label ( bpa_icon_n[n] ) : (GtkButton *)gtk_button_new_from_icon_name ( bpa_icon_n[n], GTK_ICON_SIZE_MENU );
		g_signal_connect ( button, "clicked", G_CALLBACK ( gmf_win_signal_all_ppa_num ), win );

		char buf[10];
		sprintf ( buf, "%u", n );
		gtk_widget_set_name ( GTK_WIDGET ( button ), buf );

		gtk_button_set_relief ( button, GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

		gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

static GtkPopover * gmf_win_pp_act_b ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	enum bt_ppb_enm n = BTP_EDT;

	for ( n = BTP_EDT; n < BTP_BAL; n++ )
	{
		GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( bpb_icon_n[n], GTK_ICON_SIZE_MENU );
		g_signal_connect ( button, "clicked", G_CALLBACK ( gmf_win_signal_all_ppb_num ), win );

		char buf[10];
		sprintf ( buf, "%u", n );
		gtk_widget_set_name ( GTK_WIDGET ( button ), buf );

		gtk_button_set_relief ( button, GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

		gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

static void gmf_win_icon_press_act ( GmfWin *win )
{
	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	GtkPopover *popover = ( list ) ? win->popover_act_b : win->popover_act_a;

	if ( !win->done_copy ) popover = win->popover_act_copy;

	gtk_popover_set_relative_to ( popover, GTK_WIDGET ( win->button_act ) );
	gtk_popover_popup ( popover );

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_button_set_tab ( const char *path, uint8_t n, GmfWin *win )
{
	g_autofree char *name = g_path_get_basename ( path );

	gtk_button_set_label ( win->button_tab[n], name );
	gtk_widget_set_name ( GTK_WIDGET ( win->button_tab[n] ), path );
}

static void gmf_win_button_tab_set_tab ( const char *path, GmfWin *win )
{
	GList *l = NULL;

	uint8_t n = 0; for ( n = 0; n < MAX_TAB; n++ )
	{
		const char *label = gtk_button_get_label ( win->button_tab[n] );
		const char *name  = gtk_widget_get_name ( GTK_WIDGET ( win->button_tab[n] ) );

		if ( g_str_has_prefix ( label, "none" ) ) break;

		if ( g_str_equal ( path, name ) ) continue;

		l = g_list_append ( l, g_strdup ( name ) );
	}

	n = 0;
	gmf_win_button_set_tab ( path, n, win );

	while ( l != NULL )
	{
		if ( ++n < MAX_TAB ) gmf_win_button_set_tab ( (char *)l->data, n, win );

		l = l->next;
	}

	g_list_free_full ( l, (GDestroyNotify) g_free );
}

static void gmf_win_clicked_button_tab ( GtkButton *button, GmfWin *win )
{
	const char *path = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	if ( !g_str_has_prefix ( path, "none" ) ) gmf_win_set_file ( path, win );

	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_tab ), FALSE );
}

static GtkPopover * gmf_win_pp_tab ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkScrolledWindow *scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_size_request ( GTK_WIDGET ( scw ), 500, -1 );
	gtk_widget_set_visible ( GTK_WIDGET ( scw ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	uint8_t n = 0; for ( n = 0; n < MAX_TAB; n++ )
	{
		win->button_tab[n] = (GtkButton *)gtk_button_new_with_label ( "none" );
		g_signal_connect ( win->button_tab[n], "clicked", G_CALLBACK ( gmf_win_clicked_button_tab ), win );

		gtk_button_set_relief ( win->button_tab[n], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( win->button_tab[n] ), TRUE );

		gtk_box_pack_start ( h_box, GTK_WIDGET ( win->button_tab[n] ), TRUE, TRUE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( scw ), GTK_WIDGET ( h_box ) );
	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( scw ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

// ***** Icon-View *****

static inline gboolean gmf_win_link_exists ( const char *path, const char *link )
{
	gboolean link_exist = FALSE;

	if ( !g_path_is_absolute ( link ) )
	{
		g_autofree char *dir_name = g_path_get_dirname ( path );
		g_autofree char *new_name = g_build_filename ( dir_name, link, NULL );

		link_exist = g_file_test ( new_name, G_FILE_TEST_EXISTS );
	}
	else
		link_exist = g_file_test ( link, G_FILE_TEST_EXISTS );

	return link_exist;
}

static inline GIcon * gmf_win_emblemed_icon ( const char *name, GIcon *gicon )
{
	GIcon *e_icon = g_themed_icon_new ( name );
	GEmblem *emblem  = g_emblem_new ( e_icon );

	GIcon *emblemed  = g_emblemed_icon_new ( gicon, emblem );

	g_object_unref ( e_icon );
	g_object_unref ( emblem );

	return emblemed;
}

static GdkPixbuf * gmf_win_icon_get_pixbuf ( const char *path, gboolean is_link, uint16_t icon_size )
{
	GdkPixbuf *pixbuf = NULL;

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	const char *content_type = ( finfo ) ? g_file_info_get_content_type ( finfo ) : NULL;

	if ( content_type && g_str_has_prefix ( content_type, "image" ) ) pixbuf = gdk_pixbuf_new_from_file_at_size ( path, icon_size, icon_size, NULL );

	if ( finfo && pixbuf == NULL )
	{
		GtkIconInfo *icon_info = NULL;
		GIcon *emblemed = NULL, *unknown = NULL, *gicon = g_file_info_get_icon ( finfo );

		if ( is_link )
		{
			gboolean is_exist = FALSE;
			const char *target = g_file_info_get_symlink_target ( finfo );

			if ( target ) is_exist = gmf_win_link_exists ( path, target );

			if ( !is_exist ) unknown = g_themed_icon_new ( "unknown" );

			emblemed = ( is_exist ) ? gmf_win_emblemed_icon ( "emblem-symbolic-link", gicon ) : gmf_win_emblemed_icon ( "error", unknown );

			icon_info = gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), emblemed, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );
		}
		else
			icon_info = gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), gicon, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );

		// if ( !icon_info ) icon_info = gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), unknown, icon_size, GTK_ICON_LOOKUP_FORCE_REGULAR );

		if ( icon_info ) pixbuf = gtk_icon_info_load_icon ( icon_info, NULL );

		if ( unknown ) g_object_unref ( unknown );
		if ( emblemed ) g_object_unref ( emblemed );
		if ( icon_info ) g_object_unref ( icon_info );
	}

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	return pixbuf;
}

static void gmf_icon_set_pixbuf ( GtkTreeIter iter, GtkTreeModel *model, uint16_t icon_size )
{
	gboolean is_link = FALSE;
	g_autofree char *path = NULL;
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_LINK, &is_link, -1 );

	if ( g_file_test ( path, G_FILE_TEST_EXISTS ) )
	{
		GdkPixbuf *pixbuf = gmf_win_icon_get_pixbuf ( path, is_link, icon_size );

		if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_IS_PIXBUF, TRUE, COL_PIXBUF, pixbuf, -1 );

		if ( pixbuf ) g_object_unref ( pixbuf );
	}
}

static gpointer gmf_icon_update_pixbuf_thread ( GmfWin *win )
{
	uint16_t num = 0, end = win->end_t, mod = win->mod_t, limit = win->limit_t, icon_size = win->icon_size;

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	gboolean valid = FALSE;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		if ( ( !limit && num > end ) || ( limit && num < end ) ) { num++; continue; }

		if (  mod && !( num % 2 ) ) { num++; continue; }
		if ( !mod &&  ( num % 2 ) ) { num++; continue; }

		gboolean is_pb = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_PIXBUF, &is_pb, -1 );

		if ( !is_pb ) gmf_icon_set_pixbuf ( iter, model, icon_size );

		num++;
	}

	return NULL;
}

static void gmf_icon_update_pixbuf_all ( uint16_t nums, GmfWin *win )
{
	win->mod_t = 0; win->limit_t = 0; win->end_t = nums / 2;
	GThread *thread_0_0 = g_thread_new ( NULL, (GThreadFunc)gmf_icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 1; win->limit_t = 0; win->end_t = nums / 2;
	GThread *thread_1_0 = g_thread_new ( NULL, (GThreadFunc)gmf_icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 0; win->limit_t = 1; win->end_t = nums / 2;
	GThread *thread_0_1 = g_thread_new ( NULL, (GThreadFunc)gmf_icon_update_pixbuf_thread, win );

	g_usleep ( 20000 );
	win->mod_t = 1; win->limit_t = 1; win->end_t = nums / 2;
	GThread *thread_1_1 = g_thread_new ( NULL, (GThreadFunc)gmf_icon_update_pixbuf_thread, win );

	g_thread_join  ( thread_0_0 );
	g_thread_join  ( thread_1_0 );
	g_thread_join  ( thread_0_1 );
	g_thread_join  ( thread_1_1 );

	g_thread_unref ( thread_0_0 );
	g_thread_unref ( thread_1_0 );
	g_thread_unref ( thread_0_1 );
	g_thread_unref ( thread_1_1 );
}

static uint16_t gmf_win_icon_dir_n ( const char *path_dir )
{
	uint16_t nums = 0;
	const char *name = NULL;

	GDir *dir = g_dir_open ( path_dir, 0, NULL );

	while ( ( name = g_dir_read_name (dir) ) ) nums++;

	g_dir_close ( dir );

	return nums;
}

static void gmf_win_icon_open_location ( const char *path_dir, const char *search, GmfWin *win )
{
	g_return_if_fail ( path_dir != NULL );

	GDir *dir = g_dir_open ( path_dir, 0, NULL );

	if ( !dir ) { gmf_dialog_message ( "", g_strerror ( errno ), GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	const char *name = NULL;
	const char *icon_name_d = "folder";
	const char *icon_name_f = "text-x-preview"; // text-x-generic

	GtkIconTheme *itheme = gtk_icon_theme_get_default ();

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	gtk_list_store_clear ( GTK_LIST_STORE ( model ) );

	uint16_t nums = gmf_win_icon_dir_n ( path_dir );

	while ( ( name = g_dir_read_name (dir) ) )
	{
		char *display_name = g_filename_to_utf8 ( name, -1, NULL, NULL, NULL );

		char *display_name_down = g_utf8_strdown ( display_name, -1 );

		if ( name[0] != '.' || win->hidden )
		if ( !search || ( search && g_strrstr ( display_name_down, search ) ) )
		{
			char *path = g_build_filename ( path_dir, name, NULL );

			gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );
			gboolean is_slk = g_file_test ( path, G_FILE_TEST_IS_SYMLINK );

			ulong size = get_file_size ( path );

			GdkPixbuf *pixbuf = ( win->preview && nums < PREVIEW ) ? pixbuf = gmf_win_icon_get_pixbuf ( path, is_slk, win->icon_size ) 
				: gtk_icon_theme_load_icon ( itheme, ( is_dir ) ? icon_name_d : icon_name_f, win->icon_size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL );

			gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );

			gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter,
					COL_PATH, path,
					COL_NAME, display_name,
					COL_IS_DIR, is_dir,
					COL_IS_LINK, is_slk,
					COL_IS_PIXBUF, ( win->preview && nums < PREVIEW ) ? TRUE : FALSE,
					COL_PIXBUF, pixbuf,
					COL_SIZE, size,
					-1 );

			free ( path );
			if ( pixbuf ) g_object_unref ( pixbuf );
		}

		free ( display_name );
		free ( display_name_down );
	}

	g_dir_close ( dir );

	if ( win->preview && nums >= PREVIEW ) gmf_icon_update_pixbuf_all ( nums, win );

	gtk_icon_view_scroll_to_path ( win->icon_view, gtk_tree_path_new_first (), FALSE, 0, 0 );
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

static void gmf_win_icon_changed ( const char *path_f, GmfWin *win )
{
	GtkTreeIter iter;
	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	while ( list != NULL )
	{
		if ( !GTK_IS_WIDGET ( win->icon_view ) ) break;

		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );

		gboolean is_dir = FALSE, is_link = FALSE, path_f_change = FALSE;
		gtk_tree_model_get ( model, &iter, COL_IS_DIR, &is_dir, COL_IS_LINK, &is_link, COL_PATH, &path, -1 );

		if ( path_f && g_str_equal ( path, path_f ) ) path_f_change = TRUE;

		if ( !is_dir || path_f_change )
		{
			ulong size = get_file_size ( path );
			g_autofree char *fsize = g_format_size ( size );

			GdkPixbuf *pxbf = NULL;
			gtk_tree_model_get ( model, &iter, COL_PIXBUF, &pxbf, -1 );

			GdkPixbuf *pixbuf = ( pxbf ) ? gmf_icon_pixbuf_add_text ( fsize, win->icon_size, pxbf ) : NULL;

			if ( pixbuf ) gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_PIXBUF, pixbuf, COL_IS_PIXBUF, TRUE, -1 );

			if ( pxbf ) g_object_unref ( pxbf );
			if ( pixbuf ) g_object_unref ( pixbuf );
		}

		free ( path );
		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_icon_select_path ( const char *path_f, GmfWin *win )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	gboolean found = FALSE, valid = FALSE;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid; valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		g_autofree char *path = NULL;
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		if ( path_f && g_str_equal ( path, path_f ) ) found = TRUE;

		if ( found )
		{
			GtkTreePath *treepath = gtk_tree_model_get_path ( model, &iter);

			gtk_icon_view_select_path ( win->icon_view, treepath );

			gtk_tree_path_free ( treepath );

			break;
		}
	}
}

static void gmf_win_icon_changed_monitor ( UNUSED GFileMonitor *monitor, UNUSED GFile *file, UNUSED GFile *other, GFileMonitorEvent evtype, GmfWin *win )
{
	if ( !GTK_IS_WIDGET ( win->icon_view ) ) return;

	gboolean reload = FALSE, changed = FALSE;

	switch ( evtype )
	{
		case G_FILE_MONITOR_EVENT_CHANGED:           changed = TRUE;  break;
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_DELETED:           reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_CREATED:           reload  = TRUE; if ( !win->done_copy ) changed = TRUE; break;
		case G_FILE_MONITOR_EVENT_RENAMED:           reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_IN:          reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_MOVED_OUT:         reload  = TRUE;  break;
		case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: reload  = FALSE; break;

		default: break;
	}

	g_autofree char *path = g_file_get_path ( file );

	GList *list = gtk_icon_view_get_selected_items ( win->icon_view );

	if ( reload ) gmf_win_rld ( win );

	if ( list &&  win->done_copy )
		gtk_icon_view_select_path ( win->icon_view, (GtkTreePath *)list->data );
	else
		gmf_win_icon_select_path ( path, win );

	if ( changed ) gmf_win_icon_changed ( path, win );

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );
}

static void gmf_win_icon_free_monitor ( GmfWin *win )
{
	if ( win->monitor )
	{
		g_signal_handlers_disconnect_by_func ( win->monitor, gmf_win_icon_changed_monitor, win );

		g_file_monitor_cancel ( win->monitor );

		g_object_unref ( win->monitor );
	}
}

static void gmf_win_icon_run_monitor ( GmfWin *win )
{
	gmf_win_icon_free_monitor ( win );

	win->monitor = g_file_monitor ( win->file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL );

	if ( win->monitor == NULL ) return;

	g_signal_connect ( win->monitor, "changed", G_CALLBACK ( gmf_win_icon_changed_monitor ), win );
}

static gboolean gmf_win_icon_changed_timeout ( GmfWin *win )
{
	if ( !GTK_IS_WIDGET ( win->icon_view ) ) return FALSE;

	gmf_win_icon_changed ( NULL, win );

	return FALSE;
}

static void gmf_win_icon_selection_changed ( UNUSED GtkIconView *icon_view, GmfWin *win )
{
	g_timeout_add ( 1000, (GSourceFunc)gmf_win_icon_changed_timeout, win );
}

static int gmf_win_icon_sort_func_a_z ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, UNUSED gpointer data )
{
	int ret = 1;
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_a && is_dir_b )
		ret = 1;
	else if ( is_dir_a && !is_dir_b )
		ret = -1;
	else
	{
		g_autofree char *na = g_utf8_collate_key_for_filename ( name_a, -1 );
		g_autofree char *nb = g_utf8_collate_key_for_filename ( name_b, -1 );

		ret = g_strcmp0 ( na, nb );

		// ret = g_utf8_collate ( name_a, name_b );
	}

	return ret;
}

static int gmf_win_icon_sort_func_z_a ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, UNUSED gpointer data )
{
	int ret = 1;
	gboolean is_dir_a, is_dir_b;

	g_autofree char *name_a = NULL;
	g_autofree char *name_b = NULL;

	gtk_tree_model_get ( model, a, COL_IS_DIR, &is_dir_a, COL_NAME, &name_a, -1 );
	gtk_tree_model_get ( model, b, COL_IS_DIR, &is_dir_b, COL_NAME, &name_b, -1 );

	if ( !is_dir_b && is_dir_a )
		ret = 1;
	else if ( is_dir_b && !is_dir_a )
		ret = -1;
	else
	{
		g_autofree char *na = g_utf8_collate_key_for_filename ( name_a, -1 );
		g_autofree char *nb = g_utf8_collate_key_for_filename ( name_b, -1 );

		ret = g_strcmp0 ( nb, na );

		// ret = g_utf8_collate ( name_b, name_a );
	}

	return ret;
}

static int gmf_win_icon_sort_func_m_g ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, UNUSED gpointer data )
{
	int ret = 1;

	uint64_t size_a = 0;
	uint64_t size_b = 0;

	gtk_tree_model_get ( model, a, COL_SIZE, &size_a, -1 );
	gtk_tree_model_get ( model, b, COL_SIZE, &size_b, -1 );

	ret = ( size_a >= size_b ) ? 1 : 0;

	return ret;
}

static int gmf_win_icon_sort_func_g_m ( GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, UNUSED gpointer data )
{
	int ret = 1;

	uint64_t size_a = 0;
	uint64_t size_b = 0;

	gtk_tree_model_get ( model, a, COL_SIZE, &size_a, -1 );
	gtk_tree_model_get ( model, b, COL_SIZE, &size_b, -1 );

	ret = ( size_a <= size_b ) ? 1 : 0;

	return ret;
}

static void gmf_win_icon_set_sort_func ( uint num, GmfWin *win )
{
	GtkTreeModel *model = gtk_icon_view_get_model ( win->icon_view );

	if ( num == 0 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_win_icon_sort_func_a_z, NULL, NULL );
	if ( num == 1 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_win_icon_sort_func_z_a, NULL, NULL );
	if ( num == 2 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_win_icon_sort_func_m_g, NULL, NULL );
	if ( num == 3 ) gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE ( model ), gmf_win_icon_sort_func_g_m, NULL, NULL );
}

static GtkTreeModel * gmf_win_icon_create_model ( void )
{
	GtkListStore *store = gtk_list_store_new ( NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF, G_TYPE_UINT64 );

	gtk_tree_sortable_set_default_sort_func ( GTK_TREE_SORTABLE (store), gmf_win_icon_sort_func_a_z, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING );

	return GTK_TREE_MODEL ( store );
}

static GtkIconView * gmf_win_icon_view_create ( void )
{
	GtkIconView *icon_view = (GtkIconView *)gtk_icon_view_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( icon_view ), TRUE );

	GtkTreeModel *model = gmf_win_icon_create_model ();
	gtk_icon_view_set_model ( icon_view, model );

	g_object_unref ( model );

	gtk_icon_view_set_item_width    ( icon_view, ITEM_WIDTH );
	gtk_icon_view_set_text_column   ( icon_view, COL_NAME   );
	gtk_icon_view_set_pixbuf_column ( icon_view, COL_PIXBUF );

	gtk_icon_view_set_selection_mode ( icon_view, GTK_SELECTION_MULTIPLE );

	return icon_view;
}

static void gmf_win_icon_item_activated ( GtkIconView *icon_view, GtkTreePath *tree_path, GmfWin *win )
{
	g_autofree char *path = NULL;

	gboolean is_dir;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );
	
	gtk_tree_model_get_iter ( model, &iter, tree_path );
	gtk_tree_model_get ( model, &iter, COL_PATH, &path, COL_IS_DIR, &is_dir, -1 );

	if ( is_dir )
	{
		gmf_win_set_file ( path, win );
	}
	else
	{
		GFile *file = g_file_parse_name ( path );

		gmf_activated_file ( file, GTK_WINDOW ( win ) );

		g_object_unref ( file );
	}
}

static gboolean gmf_win_icon_press_event ( UNUSED GtkDrawingArea *draw, GdkEventButton *event, GmfWin *win )
{
	if ( event->button == GDK_BUTTON_PRIMARY ) return GDK_EVENT_PROPAGATE;

	if ( event->button == GDK_BUTTON_MIDDLE ) { gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), !gtk_widget_get_visible ( GTK_WIDGET ( win->psb ) ) ); return GDK_EVENT_PROPAGATE; }

	if ( event->button == GDK_BUTTON_SECONDARY ) { gmf_win_icon_press_act ( win ); return GDK_EVENT_STOP; }

	return GDK_EVENT_PROPAGATE;
}

static void gmf_win_set_file ( const char *path_dir, GmfWin *win )
{
	g_return_if_fail ( path_dir != NULL );

	if ( !g_file_test ( path_dir, G_FILE_TEST_EXISTS ) ) return;

	if ( win->file ) g_object_unref ( win->file );
	win->file = g_file_parse_name ( path_dir );

	gtk_places_sidebar_set_location ( win->psb, win->file );

	gmf_win_icon_open_location ( path_dir, NULL, win );

	gmf_win_button_tab_set_tab ( path_dir, win );

	gmf_win_icon_run_monitor ( win );
}

static void gmf_win_start_arg ( GFile *file, GmfWin *win )
{
	if ( file )
	{
		g_autofree char *path = g_file_get_path ( file );

		gboolean is_dir = g_file_test ( path, G_FILE_TEST_IS_DIR );

		if ( is_dir ) gmf_win_set_file ( path, win ); else gmf_win_set_file ( g_get_home_dir (), win );
	}
	else
		gmf_win_set_file ( g_get_home_dir (), win );
}

// ***** Pref *****

static void gmf_changed_theme ( GtkFileChooserButton *button, GmfWin *win )
{
	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_pref ), FALSE );

	g_autofree char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	g_autofree char *name = g_path_get_basename ( path );

	g_object_set ( gtk_settings_get_default (), "gtk-theme-name", name, NULL );
}

static void gmf_changed_icons ( GtkFileChooserButton *button, GmfWin *win )
{
	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_pref ), FALSE );

	g_autofree char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	g_autofree char *name = g_path_get_basename ( path );

	g_object_set ( gtk_settings_get_default (), "gtk-icon-theme-name", name, NULL );

	gmf_win_rld ( win );
}

static GtkFileChooserButton * gmf_win_create_theme ( const char *title, const char *path, void ( *f )( GtkFileChooserButton *, GmfWin * ), GmfWin *win )
{
	GtkFileChooserButton *button = (GtkFileChooserButton *)gtk_file_chooser_button_new ( title, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( button ), path );
	g_signal_connect ( button, "file-set", G_CALLBACK ( f ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static void gmf_spinbutton_changed_opacity ( GtkSpinButton *button, GmfWin *win )
{
	uint8_t opacity = (uint8_t)gtk_spin_button_get_value_as_int ( button );

	win->opacity = opacity;

	gtk_widget_set_opacity ( GTK_WIDGET ( win ), ( double )opacity / 100 );
}

static GtkSpinButton * gmf_create_spinbutton ( uint val, uint8_t min, uint32_t max, uint8_t step, void ( *f )( GtkSpinButton *, GmfWin * ), GmfWin *win )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );
	g_signal_connect ( spinbutton, "changed", G_CALLBACK ( f ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );

	return spinbutton;
}

static void gmf_clicked_dark ( UNUSED GtkButton *button, UNUSED GmfWin *win )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );
}

static void gmf_clicked_about ( UNUSED GtkButton *button, GmfWin *win )
{
	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_pref ), FALSE );

	gmf_dialog_about ( GTK_WINDOW ( win ) );
}

static void gmf_clicked_quit ( UNUSED GtkButton *button, GmfWin *win )
{
	gtk_widget_destroy ( GTK_WIDGET ( win ) );
}

static GtkButton * gmf_win_create_button ( const char *icon_name, void ( *f )( GtkButton *, GmfWin * ), GmfWin *win )
{
	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( icon_name, GTK_ICON_SIZE_MENU );
	g_signal_connect ( button, "clicked", G_CALLBACK ( f ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static void gmf_win_changed_sort ( GtkComboBoxText *combo_box, GmfWin *win )
{
	uint num = (uint)gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	gmf_win_icon_set_sort_func ( num, win );

	gmf_win_rld ( win );
}

static GtkComboBoxText * gmf_win_create_combo_sort ( GmfWin *win )
{
	GtkComboBoxText *combo = (GtkComboBoxText *)gtk_combo_box_text_new ();

	gtk_combo_box_text_append ( combo, NULL, "A - Z" );
	gtk_combo_box_text_append ( combo, NULL, "Z - A" );
	gtk_combo_box_text_append ( combo, NULL, "MB - GB" );
	gtk_combo_box_text_append ( combo, NULL, "GB - MB" );

	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo ), 0 );
	g_signal_connect ( combo, "changed", G_CALLBACK ( gmf_win_changed_sort ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( combo ), TRUE );

	return combo;
}

static void gmf_win_changed_size ( GtkComboBoxText *combo_box, GmfWin *win )
{
	g_autofree char *text = gtk_combo_box_text_get_active_text ( combo_box );

	win->icon_size = ( uint16_t )atoi ( text );

	gmf_win_rld ( win );
}

static uint8_t gmf_win_get_size_num ( GmfWin *win )
{
	uint8_t i = 0, num = 2;

	uint16_t icon_size_n[] = { 24, 32, 48, 64, 96, 128, 256 };

	for ( i = 0; i < G_N_ELEMENTS ( icon_size_n ); i++ ) { if ( win->icon_size == icon_size_n[i] ) { num = i; break; } }

	return num;
}

static GtkComboBoxText * gmf_win_create_combo_size ( GmfWin *win )
{
	GtkComboBoxText *combo = (GtkComboBoxText *)gtk_combo_box_text_new ();

	const char *icon_size_n[] = { "24", "32", "48", "64", "96", "128", "256" };

	uint8_t i = 0; for ( i = 0; i < G_N_ELEMENTS ( icon_size_n ); i++ ) { gtk_combo_box_text_append ( combo, NULL, icon_size_n[i] ); }
 
	gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo ), gmf_win_get_size_num ( win ) );
	g_signal_connect ( combo, "changed", G_CALLBACK ( gmf_win_changed_size ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( combo ), TRUE );

	return combo;
}

static void gmf_clicked_preview ( GtkButton *button, GmfWin *win )
{
	win->preview = !win->preview;

	GtkImage *image = (GtkImage *)gtk_button_get_image ( button );
	gtk_image_set_from_icon_name ( image, ( win->preview ) ? "desktop" : "folder", GTK_ICON_SIZE_MENU );

	gmf_win_rld ( win );
}

static GtkPopover * gmf_win_popover_pref ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( vbox ), TRUE );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_create_spinbutton ( win->opacity,  40, 100, 1, gmf_spinbutton_changed_opacity, win ) ), FALSE, FALSE, 0 );

	GtkBox *hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	GtkComboBoxText *combo_size = gmf_win_create_combo_size ( win );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( ( win->preview ) ? "desktop" : "folder", GTK_ICON_SIZE_MENU );
	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( gmf_clicked_preview ), win );

	gtk_box_pack_start ( hbox , GTK_WIDGET ( button     ), FALSE, FALSE, 0 );
	gtk_box_pack_end   ( hbox , GTK_WIDGET ( combo_size ), TRUE,  TRUE,  0 );
	gtk_box_pack_start ( vbox , GTK_WIDGET ( hbox       ), FALSE, FALSE, 0 );

	GtkComboBoxText *combo_sort = gmf_win_create_combo_sort ( win );
	gtk_box_pack_start ( vbox , GTK_WIDGET ( combo_sort ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Icons", "/usr/share/icons/",  gmf_changed_icons, win ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Theme", "/usr/share/themes/", gmf_changed_theme, win ) ), FALSE, FALSE, 0 );

	hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "weather-clear-night", gmf_clicked_dark,  win ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "dialog-information",  gmf_clicked_about, win ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "window-close",        gmf_clicked_quit,  win ) ), TRUE, TRUE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( hbox ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( vbox ) );

	return popover;
}

static gboolean gmf_win_get_settings ( const char *schema_id )
{
	GSettingsSchemaSource *schemasrc = g_settings_schema_source_get_default ();
	GSettingsSchema *schema = g_settings_schema_source_lookup ( schemasrc, schema_id, FALSE );

	if ( schema == NULL ) g_warning ( "%s:: schema: %s - not installed.", __func__, schema_id );

	if ( schema == NULL ) return FALSE;

	return TRUE;
}

static void gmf_win_save_pref ( GmfWin *win )
{
	const char *schema_id = "org.gtk.gmf";
	if ( !gmf_win_get_settings ( schema_id ) ) return;

	GSettings *settings = g_settings_new ( schema_id );

	gboolean dark = FALSE;
	g_autofree char *theme = NULL;
	g_autofree char *icon_theme = NULL;

	g_object_get ( gtk_settings_get_default (), "gtk-theme-name", &theme, NULL );
	g_object_get ( gtk_settings_get_default (), "gtk-icon-theme-name", &icon_theme, NULL );
	g_object_get ( gtk_settings_get_default (), "gtk-application-prefer-dark-theme", &dark, NULL );

	g_settings_set_boolean ( settings, "dark",  dark  );

	g_settings_set_string  ( settings, "theme", theme );
	g_settings_set_string  ( settings, "icon-theme", icon_theme );

	g_settings_set_uint    ( settings, "opacity",    win->opacity );
	g_settings_set_uint    ( settings, "icon-size",  win->icon_size );

	g_settings_set_uint ( settings, "width",  win->width  );
	g_settings_set_uint ( settings, "height", win->height );

	g_object_unref ( settings );
}

static void gmf_win_set_pref ( GmfWin *win )
{
	const char *schema_id = "org.gtk.gmf";
	if ( !gmf_win_get_settings ( schema_id ) ) return;

	GSettings *settings = g_settings_new ( schema_id );

	win->dark       = g_settings_get_boolean ( settings, "dark" );
	win->opacity    = (uint8_t)g_settings_get_uint ( settings, "opacity" );
	win->icon_size  = (uint16_t)g_settings_get_uint ( settings, "icon-size" );

	win->width  = (uint16_t)g_settings_get_uint ( settings, "width"  );
	win->height = (uint16_t)g_settings_get_uint ( settings, "height" );

	g_autofree char *theme      = g_settings_get_string  ( settings, "theme" );
	g_autofree char *icon_theme = g_settings_get_string  ( settings, "icon-theme" );

	if ( theme && !g_str_has_prefix ( theme, "none" ) ) g_object_set ( gtk_settings_get_default (), "gtk-theme-name", theme, NULL );
	if ( icon_theme && !g_str_has_prefix ( icon_theme, "none" ) ) g_object_set ( gtk_settings_get_default (), "gtk-icon-theme-name", icon_theme, NULL );

	g_object_set ( gtk_settings_get_default (), "gtk-application-prefer-dark-theme", win->dark, NULL );

	g_object_unref ( settings );
}

// ***** Drag and Drop *****

static void gmf_win_icon_drop ( UNUSED GtkIconView *iv, GdkDragContext *context, UNUSED int x, UNUSED int y, GtkSelectionData *sd, UNUSED uint info, guint32 time, GmfWin *win )
{
	if ( !win->done_copy ) { gmf_dialog_message ( " ", "It works ...", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) ); return; }

	GdkAtom target = gtk_selection_data_get_data_type ( sd );

	GdkAtom atom = gdk_atom_intern_static_string ( "text/uri-list" );

	if ( target == atom )
	{
		win->uris_copy = gtk_selection_data_get_uris ( sd );

		win->cm_num = COPY;
		copy_dir_file_run_thread ( win );

		gtk_drag_finish ( context, TRUE, FALSE, time );
	}
}

static void gmf_win_icon_drag ( GtkIconView *icon_view, UNUSED GdkDragContext *c, GtkSelectionData *sd, UNUSED uint info, UNUSED uint time, UNUSED GmfWin *win )
{
	GList *list = gtk_icon_view_get_selected_items ( icon_view );
	uint i = 0, len = g_list_length ( list );

	char **uris = g_malloc ( ( len + 1 ) * sizeof (char *) );

	while ( list != NULL )
	{
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_icon_view_get_model ( icon_view );

		char *path = NULL;
		gtk_tree_model_get_iter ( model, &iter, (GtkTreePath *)list->data );
		gtk_tree_model_get ( model, &iter, COL_PATH, &path, -1 );

		uris[i++] = g_filename_to_uri ( path, NULL, NULL );

		free ( path );
		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) gtk_tree_path_free );

	uris[i] = NULL;

	gtk_selection_data_set_uris ( sd, uris );
	
	g_strfreev ( uris );
}

static void gmf_win_icon_drag_drop ( GmfWin *win )
{
	GtkTargetEntry targets[] = { { "text/uri-list", GTK_TARGET_OTHER_WIDGET, 0 } };

	gtk_icon_view_enable_model_drag_source ( win->icon_view, GDK_BUTTON1_MASK, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY );
	g_signal_connect ( win->icon_view, "drag-data-get", G_CALLBACK ( gmf_win_icon_drag ), win );

	gtk_icon_view_enable_model_drag_dest ( win->icon_view, targets, G_N_ELEMENTS (targets), GDK_ACTION_COPY );
	g_signal_connect ( win->icon_view, "drag-data-received", G_CALLBACK ( gmf_win_icon_drop ), win );
}

// ***** Base *****

static void gmf_win_destroy ( UNUSED GtkWindow *window, GmfWin *win )
{
	G_LOCK ( progress_copy );
		g_cancellable_cancel ( win->cancellable_copy );
	G_UNLOCK ( progress_copy );

	gtk_icon_view_unselect_all ( win->icon_view );
}

static void gmf_win_create ( GmfWin *win )
{
	GtkWindow *window = GTK_WINDOW ( win );
	gtk_window_set_title ( window, "Gmf" );
	gtk_window_set_default_size ( window, win->width, win->height );
	gtk_window_set_icon_name ( window, "system-file-manager" );
	g_signal_connect ( window, "destroy", G_CALLBACK ( gmf_win_destroy ), win );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkBox *bar_box = gmf_win_bar_create ( win );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( bar_box ), FALSE, FALSE, 0 );

	win->psb = (GtkPlacesSidebar *)gtk_places_sidebar_new ();
	g_object_set ( win->psb, "populate-all", TRUE, NULL );
	gtk_places_sidebar_set_show_recent ( win->psb, TRUE );
	g_signal_connect ( win->psb, "unmount", G_CALLBACK ( gmf_win_psb_unmount ), win );
	g_signal_connect ( win->psb, "populate-popup", G_CALLBACK ( gmf_win_psb_popup ), win );
	g_signal_connect ( win->psb, "open-location",  G_CALLBACK ( gmf_win_psb_open_location ), win );

	win->scw = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( win->scw, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

	win->icon_view = gmf_win_icon_view_create ();
	g_signal_connect ( win->icon_view, "item-activated",     G_CALLBACK ( gmf_win_icon_item_activated    ), win );
	g_signal_connect ( win->icon_view, "button-press-event", G_CALLBACK ( gmf_win_icon_press_event       ), win );
	g_signal_connect ( win->icon_view, "selection-changed",  G_CALLBACK ( gmf_win_icon_selection_changed ), win );

	gtk_container_add ( GTK_CONTAINER ( win->scw ), GTK_WIDGET ( win->icon_view ) );

	gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->scw ), TRUE );

	win->popover_tab   = gmf_win_pp_tab   ( win );
	win->popover_act_a = gmf_win_pp_act_a ( win );
	win->popover_act_b = gmf_win_pp_act_b ( win );
	win->popover_pref  = gmf_win_popover_pref ( win );

	win->popover_act_copy = gmf_win_pp_act_copy ( win );

	const char *home = g_get_home_dir ();
	win->popover_jump   = gmf_win_popover_js ( "folder",     "go-jump",       home, BT_JMP, win );
	win->popover_search = gmf_win_popover_js ( "edit-clear", "system-search", NULL, BT_SRH, win );

	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( win->psb ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( win->scw ) );
	gtk_widget_set_visible ( GTK_WIDGET ( paned ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( GTK_WINDOW ( win ) );

	gtk_widget_set_opacity ( GTK_WIDGET ( win ), ( double ) win->opacity / 100 );

	gmf_win_icon_drag_drop ( win );
}

static void gmf_win_init ( GmfWin *win )
{
	win->file = NULL;

	win->width  = 700;
	win->height = 350;

	win->opacity = 100;
	win->icon_size = 48;

	win->hidden = FALSE;
	win->preview = TRUE;
	win->unmount_set_home = FALSE;

	win->monitor = NULL;

	win->done_copy = TRUE;
	win->list_err_copy = NULL;
	win->cancellable_copy = g_cancellable_new ();

	win->debug = ( g_getenv ( "GMF_DEBUG" ) ) ? TRUE : FALSE;

	gmf_win_set_pref ( win );

	gmf_win_create ( win );
}

static void gmf_win_dispose ( GObject *object )
{
	GmfWin *win = GMF_WIN ( object );

	GdkWindow *rwin = gtk_widget_get_window ( GTK_WIDGET ( win ) );

	if ( GDK_IS_WINDOW ( rwin ) )
	{
  		int rwidth  = gdk_window_get_width  ( rwin );
  		int rheight = gdk_window_get_height ( rwin );

		uint16_t set_width  = ( rwidth  > 0 ) ? (uint16_t)rwidth  : 700;
		uint16_t set_height = ( rheight > 0 ) ? (uint16_t)rheight : 350;

		win->width  = set_width;
		win->height = set_height;
	}

	G_OBJECT_CLASS ( gmf_win_parent_class )->dispose ( object );
}

static void gmf_win_finalize ( GObject *object )
{
	GmfWin *win = GMF_WIN ( object );

	gmf_win_save_pref ( win );

	gmf_win_icon_free_monitor ( win );

	if ( win->file ) g_object_unref ( win->file );

	g_object_unref ( win->cancellable_copy );

	G_OBJECT_CLASS (gmf_win_parent_class)->finalize (object);
}

static void gmf_win_class_init ( GmfWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->dispose  = gmf_win_dispose;
	oclass->finalize = gmf_win_finalize;
}

GmfWin * gmf_win_new ( GFile *file, GmfApp *app )
{
	GmfWin *win = g_object_new ( GMF_TYPE_WIN, "application", app, NULL );

	gmf_win_start_arg ( file, win );

	return win;
}
