/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-dialog.h"
#include "gmf-info-win.h"

#include <errno.h>
#include <sys/stat.h>

G_LOCK_DEFINE_STATIC ( info_progress );

typedef struct _Info Info;

struct _Info
{
	char *path;

	uint32_t indx;
	uint64_t size;

	gboolean done;
};

struct _InfoWin
{
	GtkWindow parent_instance;

	GtkLabel *label_size;

	char *odir;
	char *path;

	Info *info;
	GThread *thread;

	uint src_update;

	gboolean exit;
};

G_DEFINE_TYPE ( InfoWin, info_win, GTK_TYPE_WINDOW )

static uint info_get_file_mode ( const char *file )
{
	uint mode = 0;

	struct stat sb;

	if ( stat ( file, &sb ) == 0 )
		mode = sb.st_mode;
	else
		g_warning ( "%s:: %s ", __func__, g_strerror (errno) );

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

static void info_set_file_mode_acces ( const char *file, uint mode_unset, uint mode_set )
{
	mode_t mode = info_get_file_mode ( file );

	mode &= ~mode_unset;
	mode |= mode_set;

	int res = chmod ( file, mode );

	if ( res == -1 ) gmf_dialog_message ( "", g_strerror (errno), GTK_MESSAGE_ERROR, NULL );
}

static char * info_file_time_to_str ( uint64_t time )
{
	GDateTime *date = g_date_time_new_from_unix_local ( (int64_t)time );

	char *str_time = g_date_time_format ( date, "%a; %e-%b-%Y %T %z" );

	g_date_time_unref ( date );

	return str_time;
}

static void info_dialog_image_add_emblem ( const char *name, const char *name_em, uint8_t sz, GtkImage *image )
{
	GIcon *gicon = g_themed_icon_new ( name );
	GIcon *emblemed = emblemed_icon ( name_em, NULL, gicon );

	gtk_image_set_from_gicon ( image, emblemed, sz );

	g_object_unref ( gicon );
	g_object_unref ( emblemed );
}

static GdkPixbuf * info_dialog_pixbuf_add_emblem ( const char *name, const char *name_em, uint8_t sz )
{
	GdkPixbuf *pixbuf = NULL;

	GIcon *gicon = g_themed_icon_new ( name );
	GIcon *emblemed = emblemed_icon ( name_em, NULL, gicon );

	GtkIconInfo *iinfo = gtk_icon_theme_lookup_by_gicon ( gtk_icon_theme_get_default (), emblemed, sz, GTK_ICON_LOOKUP_FORCE_REGULAR );

	g_object_unref ( gicon );
	g_object_unref ( emblemed );

	pixbuf = gtk_icon_info_load_icon ( iinfo, NULL );

	g_object_unref ( iinfo );

	return pixbuf;
}

static GtkEntry * info_dialog_create_entry ( const char *text )
{
	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );
	gtk_editable_set_editable ( GTK_EDITABLE (entry), FALSE );

	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return entry;
}

static GtkImage * info_dialog_create_image ( GdkPixbuf *pixbuf, const char *icon, uint8_t size )
{
	GtkImage *image = ( pixbuf ) ? (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf ) : (GtkImage *)gtk_image_new_from_icon_name ( icon, size );

	gtk_widget_set_halign ( GTK_WIDGET ( image ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( image ), TRUE );

	return image;
}

static GtkLabel * info_dialog_create_label ( const char *text, uint8_t alg )
{
	GtkLabel *label = (GtkLabel *)gtk_label_new ( text );
	gtk_widget_set_halign ( GTK_WIDGET ( label ), alg );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );

	return label;
}

static void info_dialog_create_hdd ( const char *path, GtkBox *m_box )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkImage *disk = (GtkImage *)gtk_image_new_from_icon_name ( "drive-harddisk", GTK_ICON_SIZE_DND );
	gtk_widget_set_visible ( GTK_WIDGET ( disk ), TRUE );

	gtk_widget_set_halign ( GTK_WIDGET ( disk ), GTK_ALIGN_START );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( disk ), FALSE, FALSE, 0 );

	GFile *file = g_file_new_for_path ( path );
	GFileInfo *fsinfo = g_file_query_filesystem_info ( file, "filesystem::*", NULL, NULL );

	uint64_t volume_free = g_file_info_get_attribute_uint64 ( fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_FREE );
	uint64_t volume_used = g_file_info_get_attribute_uint64 ( fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_USED );
	uint64_t volume_size = g_file_info_get_attribute_uint64 ( fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE );

	if ( volume_used == 0 ) volume_used = volume_size - volume_free;

	uint8_t percent = ( volume_size ) ? (uint8_t)( volume_used * 100 / volume_size ) : 0;

	g_autofree char *free = g_format_size ( volume_free );
	g_autofree char *used = g_format_size ( volume_used );
	g_autofree char *size = g_format_size ( volume_size );

	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );

	g_autofree char *markup = NULL;

	if ( dark )
		markup = g_markup_printf_escaped ( "<span foreground=\"#7AFF7A\">%s  </span>/<span foreground=\"#FFD27E\">  %s  </span>/  %s  ( %u%% )", free, used, size, percent );
	else
		markup = g_markup_printf_escaped ( "<span foreground=\"#008000\">%s  </span>/<span foreground=\"#805300\">  %s  </span>/  %s  ( %u%% )", free, used, size, percent );

	GtkLabel *label = (GtkLabel *)gtk_label_new ( "" );
	gtk_label_set_markup ( label, markup );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );

	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	GtkProgressBar *prg_bar = (GtkProgressBar *)gtk_progress_bar_new ();
	gtk_progress_bar_set_fraction ( prg_bar, (double)percent / 100 );
	gtk_widget_set_visible ( GTK_WIDGET ( prg_bar ), TRUE );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( prg_bar ), FALSE, FALSE, 0 );

	if ( fsinfo ) g_object_unref ( fsinfo );
	if ( file   ) g_object_unref ( file   );
}

static void info_changed_mode_rw ( GtkComboBox *combo_box, InfoWin *win )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( combo_box ) );

	uint num = (uint)gtk_combo_box_get_active ( combo_box );

	uint ugo = 0;
	if ( g_str_has_suffix ( name, "owner" ) ) ugo = 0;
	if ( g_str_has_suffix ( name, "group" ) ) ugo = 1;
	if ( g_str_has_suffix ( name, "other" ) ) ugo = 2;

	uint unset[] = { S_IRUSR | S_IWUSR, S_IRGRP | S_IWGRP, S_IROTH | S_IWOTH };

	uint set_ugo[3][3] = 
	{
		{ S_IRUSR, S_IRUSR | S_IWUSR, 0 },
		{ S_IRGRP, S_IRGRP | S_IWGRP, 0 },
		{ S_IROTH, S_IROTH | S_IWOTH, 0 } 
	};

	info_set_file_mode_acces ( win->path, unset[ugo], set_ugo[ugo][num] );
}

static void info_changed_mode_x ( GtkComboBox *combo_box, InfoWin *win )
{
	uint num = (uint)gtk_combo_box_get_active ( combo_box );

	uint unset = { S_IXUSR | S_IXGRP | S_IXOTH };

	uint set_exec[] = { S_IXUSR, S_IXUSR | S_IXGRP, S_IXUSR | S_IXGRP | S_IXOTH, 0 };

	info_set_file_mode_acces ( win->path, unset, set_exec[num] );
}

static GtkTreeModel * gmf_info_create_combo_icon_store ( uint8_t len, const char *icon_names[] )
{
	GtkListStore *store = gtk_list_store_new ( 2, GDK_TYPE_PIXBUF, G_TYPE_STRING );
	GtkTreeIter iter;

	uint8_t i = 0; for ( i = 0; i < len; i++ )
	{
		GdkPixbuf *pixbuf = ( len == 4 && i == 2 ) ? info_dialog_pixbuf_add_emblem ( "system-users", "add", 24 )
			: gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), icon_names[i], 24, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL );

		gtk_list_store_append ( store, &iter );
		gtk_list_store_set    ( store, &iter, 0, pixbuf, -1 );

		if ( pixbuf ) g_object_unref ( pixbuf );
	}

	return GTK_TREE_MODEL (store);
}

static GtkComboBox * info_combo_create ( uint8_t len, const char *icon_names[] )
{
	GtkTreeModel *model = gmf_info_create_combo_icon_store ( len, icon_names );
	GtkComboBox *combo = (GtkComboBox *)gtk_combo_box_new_with_model (model);
	g_object_unref (model);

	GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "pixbuf", 0, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", 1, NULL);

	return combo;
}

static void info_dialog_create_ugo ( GtkBox *m_box, InfoWin *win )
{
	const char *icon_name_a[] = { "text-x-preview", "gtk-edit", "locked" };
	const char *icon_name_b[] = { "owner", "group", "other", "exec" };
	const char *icon_name_c[] = { "avatar-default", "system-users", "system-users", "system-run" };
	const char *icon_name_d[] = { "avatar-default", "system-users", "system-users", "locked" };

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( icon_name_c ); c++ )
	{
		GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
		gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

		GtkImage *image = info_dialog_create_image ( NULL, icon_name_c[c], GTK_ICON_SIZE_LARGE_TOOLBAR );
		if ( c == 2 ) info_dialog_image_add_emblem ( "system-users", "add", GTK_ICON_SIZE_LARGE_TOOLBAR, image );
		gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

		uint m = info_get_file_mode_acces ( win->path, icon_name_b[c] );

		GtkComboBox *combo = info_combo_create ( ( c == 3 ) ? G_N_ELEMENTS ( icon_name_d ) : G_N_ELEMENTS ( icon_name_a ), ( c == 3 ) ? icon_name_d : icon_name_a );
		gtk_widget_set_visible ( GTK_WIDGET ( combo ), TRUE );
		gtk_widget_set_name ( GTK_WIDGET ( combo ), icon_name_b[c] );

		gtk_combo_box_set_active ( combo, (int)m );

		g_signal_connect ( combo, "changed", G_CALLBACK ( ( c == 3 ) ? info_changed_mode_x : info_changed_mode_rw ), win );

		gtk_widget_set_size_request ( GTK_WIDGET ( combo ), 200, -1 );
		gtk_box_pack_end ( h_box, GTK_WIDGET ( combo ), FALSE, FALSE, 0 );
		gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
	}
}

static void info_dialog_create_time ( GtkBox *m_box, GFileInfo *finfo )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	uint64_t atime = g_file_info_get_attribute_uint64 ( finfo, "time::access" );
	uint64_t mtime = g_file_info_get_attribute_uint64 ( finfo, "time::modified" );

	g_autofree char *str_atime = info_file_time_to_str ( atime );
	g_autofree char *str_mtime = info_file_time_to_str ( mtime );

	GtkImage *image = info_dialog_create_image ( NULL, "text-x-preview", GTK_ICON_SIZE_LARGE_TOOLBAR );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	GtkLabel *label = info_dialog_create_label ( str_atime, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	image = info_dialog_create_image ( NULL, "gtk-edit", GTK_ICON_SIZE_LARGE_TOOLBAR );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	label = info_dialog_create_label ( str_mtime, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
}

static char * info_win_get_name ( gboolean is_link, GFileInfo *finfo, uint32_t *indx, uint64_t *size_a, InfoWin *win )
{
	char *name = NULL;

	*indx += 1;
	*size_a = get_file_size ( win->path );

	name = g_path_get_basename ( win->path );

	if ( is_link )
	{
		const char *target = ( finfo ) ? g_file_info_get_symlink_target ( finfo ) : "unknown";
		char *name_link = g_strconcat ( name, " -> ", target, NULL );

		free ( name );
		name = name_link;
	}

	return name;
}

static void info_get_count_size_dir ( gboolean recursion, gboolean *stop, char *path, uint32_t *indx, uint64_t *size )
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
					{ dirs++; if ( recursion && !is_link ) info_get_count_size_dir ( recursion, stop, path_new, indx, size ); } // Recursion!
				else
					{ files++; if ( !is_link ) *size += get_file_size ( path_new ); }

				free ( path_new );
			}

			g_dir_close (dir);

			*indx += ( dirs + files );
		}
	}
}

static gpointer info_thread ( Info *info )
{
	usleep ( 500000 );

	info_get_count_size_dir ( TRUE, &info->done, info->path, &info->indx, &info->size );

	info->done = TRUE;

	usleep ( 500000 );

	free ( info->path );

	return NULL;
}

static gboolean info_update_timeout ( InfoWin *win )
{
	if ( win->exit ) { win->src_update = 0; return FALSE; }

	G_LOCK ( info_progress );

	gboolean done = win->info->done;
	uint32_t indx = win->info->indx;
	uint64_t size = win->info->size;

	G_UNLOCK ( info_progress );

	g_autofree char *fsize = g_format_size ( size );
	g_autofree char *ssize = g_strdup_printf ( "%u  /  %s", indx, fsize );

	gtk_label_set_text ( win->label_size, ssize );

	if ( done ) { win->src_update = 0; return FALSE; }

	return TRUE;
}

static void info_win_destroy ( G_GNUC_UNUSED GtkWindow *window, InfoWin *win )
{
	win->exit = TRUE;

	if ( win->info )
	{
		G_LOCK ( info_progress );

		win->info->done = TRUE;

		G_UNLOCK ( info_progress );

		g_thread_join  ( win->thread );
		g_thread_unref ( win->thread );

		free ( win->info );
	}

	if ( win->src_update ) g_source_remove ( win->src_update );

	free ( win->path );
	free ( win->odir );
}

static void info_win_create ( enum info_sel_enm num, const char *odir, const char *path, InfoWin *win )
{
	if ( num == I_SEL_0 ) { win->path = g_strdup ( odir ); win->odir = g_path_get_dirname  ( odir ); }
	if ( num == I_SEL_1 ) { win->path = g_strdup ( path ); win->odir = g_strdup ( odir ); }

	GtkWindow *window = GTK_WINDOW ( win );

	gtk_window_set_title ( window, " " );
	gtk_window_set_default_size ( window, 400, -1 );
	gtk_window_set_icon_name ( window, "dialog-information" );
	gtk_window_set_position ( window, GTK_WIN_POS_CENTER );
	g_signal_connect ( window, "destroy", G_CALLBACK ( info_win_destroy ), win );

	GtkBox *m_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_set_border_width ( GTK_CONTAINER ( m_box ), 10 );

	gtk_box_set_spacing ( m_box, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( m_box ), TRUE );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	gboolean is_dir  = g_file_test ( win->path, G_FILE_TEST_IS_DIR );
	gboolean is_link = g_file_test ( win->path, G_FILE_TEST_IS_SYMLINK );

	GFile *file = g_file_new_for_path ( win->path );
	GFileInfo *finfo = g_file_query_info ( file, "*", 0, NULL, NULL );

	g_autofree char *owner = g_file_info_get_attribute_as_string ( finfo, G_FILE_ATTRIBUTE_OWNER_USER  );
	g_autofree char *group = g_file_info_get_attribute_as_string ( finfo, G_FILE_ATTRIBUTE_OWNER_GROUP );

	uint32_t indx = 0;
	uint64_t size = 0;

	g_autofree char *uog = g_strdup_printf ( "%s : %s", owner, group );
	g_autofree char *name = info_win_get_name ( is_link, finfo, &indx, &size, win );

	GtkEntry *entry = info_dialog_create_entry ( name );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, TRUE, 0 );

	entry = info_dialog_create_entry ( win->odir );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( entry ), FALSE, TRUE, 0 );

	const char *content_type = g_file_info_get_content_type ( finfo );
	g_autofree char *description = ( content_type ) ? g_content_type_get_description ( content_type ) : NULL;

	GdkPixbuf *pixbuf = get_pixbuf ( win->path, is_link, 48 );

	GtkImage *image = info_dialog_create_image ( pixbuf, "unknown", GTK_ICON_SIZE_DIALOG );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );

	if ( pixbuf ) g_object_unref ( pixbuf );

	g_autofree char *str_sz = g_format_size ( size );
	g_autofree char *string = g_strdup_printf ( "%u  /  %s", indx, str_sz );

	win->label_size = info_dialog_create_label ( string, GTK_ALIGN_END );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( win->label_size ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	GtkLabel *label = info_dialog_create_label ( ( description ) ? description : "None", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	info_dialog_create_hdd ( win->odir, m_box );

	label = info_dialog_create_label ( uog, GTK_ALIGN_CENTER );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 5 );

	info_dialog_create_ugo ( m_box, win );

	label = info_dialog_create_label ( " ", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	info_dialog_create_time ( m_box, finfo );

	label = info_dialog_create_label ( " ", GTK_ALIGN_END );
	gtk_box_pack_start ( m_box, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	GtkButton *button = (GtkButton *)gtk_button_new_from_icon_name ( "window-close", GTK_ICON_SIZE_MENU );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( button ), TRUE, TRUE, 0 );

	gtk_box_pack_end ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( m_box ) );

	if ( finfo ) g_object_unref ( finfo );
	if ( file  ) g_object_unref ( file  );

	if ( is_dir )
	{
		win->info = g_new0 ( Info, 1 );
		win->info->size = 0;
		win->info->indx = 0;
		win->info->done = FALSE;

		win->info->path = g_strdup ( win->path );
		win->thread = g_thread_new ( "info-thread", (GThreadFunc)info_thread, win->info );

		win->src_update = g_timeout_add ( 100, (GSourceFunc)info_update_timeout, win );
	}
}

static void info_win_init ( InfoWin *win )
{
	win->info = NULL;

	win->exit  = FALSE;
	win->src_update = 0;
}

static void info_win_finalize ( GObject *object )
{
	G_OBJECT_CLASS (info_win_parent_class)->finalize (object);
}

static void info_win_class_init ( InfoWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = info_win_finalize;
}

InfoWin * info_win_new ( enum info_sel_enm num, const char *dir, const char *path, double opacity )
{
	InfoWin *win = g_object_new ( INFO_TYPE_WIN, NULL );

	info_win_create ( num, dir, path, win );

	gtk_window_present ( GTK_WINDOW ( win ) );
	gtk_widget_set_opacity ( GTK_WIDGET ( win ), opacity );

	return win;
}
