/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-win.h"
#include "gmf-bar.h"
#include "gmf-dialog.h"
#include "gmf-icon-view.h"

#include <glib/gstdio.h>

struct _GmfWin
{
	GtkWindow parent_instance;

	GtkPlacesSidebar *psb;

	GmfBar *bar;
	GmfIcon *icon_view;
};

G_DEFINE_TYPE ( GmfWin, gmf_win, GTK_TYPE_WINDOW )

static void gmf_win_psb_open_location ( G_GNUC_UNUSED GtkPlacesSidebar *sb, GObject *location, G_GNUC_UNUSED GtkPlacesOpenFlags flags, GmfWin *win )
{
	GFile *file = (GFile *)location;

	g_autofree char *uri  = g_file_get_uri  ( file );
	g_autofree char *path = g_file_get_path ( file );

	if ( path )
	{
		g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );
	}
	else
	{
		// if ( g_str_has_prefix ( uri, "trash"  ) ) gmf_trash_dialog  ( GTK_WINDOW ( win ) );
		// if ( g_str_has_prefix ( uri, "recent" ) ) gmf_recent_dialog ( GTK_WINDOW ( win ) );
	}
}

static void gmf_win_psb_unmount ( G_GNUC_UNUSED GtkPlacesSidebar *sb, G_GNUC_UNUSED GMountOperation *mount_op, GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-path", g_get_home_dir () );
}

static void gmf_win_clic_up ( GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-up-path" );
}

static void gmf_win_clic_act ( GmfWin *win )
{
	uint num = 0;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-num", &num );

	g_signal_emit_by_name ( win->bar, "bar-set-act", num );
}

static void gmf_win_clic_reload ( GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-reload" );
}

static void gmf_win_clic_hdn ( GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-hidden" );
}

static void gmf_win_clic_trm ( GmfWin *win )
{
	g_autofree char *pwd = g_get_current_dir ();

	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_chdir ( path );

	launch_cmd ( "x-terminal-emulator", GTK_WINDOW ( win ) );

	g_chdir ( pwd );
}

static void gmf_win_clic_arc ( GmfWin *win )
{
	GString *gstring = g_string_new ( "file-roller --add " );

	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 0 ) { g_list_free ( list ); return; }

	while ( list != NULL )
	{
		char *path = (char *)list->data;

		g_string_append_printf ( gstring, "'%s' ", path );

		list = list->next;
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );

	launch_cmd ( gstring->str, GTK_WINDOW ( win ) );

	g_string_free ( gstring, TRUE );
}

static void gmf_win_clicked_handler_num ( G_GNUC_UNUSED GmfBar *bar, uint num, G_GNUC_UNUSED const char *str, GmfWin *win )
{
	if ( num == BT_GUP ) gmf_win_clic_up ( win );
	if ( num == BT_ACT ) gmf_win_clic_act ( win );
	if ( num == BT_RLD ) gmf_win_clic_reload ( win );

	// g_message ( "%s:: num: %u, name-icon: %s ", __func__, num, str );
}

static void gmf_win_clicked_pp_handler_num ( G_GNUC_UNUSED GmfBar *bar, uint num, G_GNUC_UNUSED const char *str, GmfWin *win )
{
	if ( num == BTP_HDN ) gmf_win_clic_hdn ( win );
	if ( num == BTP_TRM ) gmf_win_clic_trm ( win );
	if ( num == BTP_ARC ) gmf_win_clic_arc ( win );

	// g_message ( "%s:: num: %u, name-icon: %s ", __func__, num, str );
}

static void gmf_win_handler_button_press ( G_GNUC_UNUSED GmfIcon *icon_view, GmfWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-run-act" );
}

static void gmf_win_create ( GmfWin *win )
{
	GtkWindow *window = GTK_WINDOW ( win );

	gtk_window_set_default_size ( window, 700, 350 );
	gtk_window_set_icon_name ( window, "system-file-manager" );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	win->bar = gmf_bar_new ();
	gtk_box_pack_start ( v_box, GTK_WIDGET ( win->bar ), FALSE, FALSE, 0 );
	g_signal_connect ( win->bar, "bar-click-num", G_CALLBACK ( gmf_win_clicked_handler_num ), win );
	g_signal_connect ( win->bar, "bar-pp-click-num", G_CALLBACK ( gmf_win_clicked_pp_handler_num ), win );

	win->psb = (GtkPlacesSidebar *)gtk_places_sidebar_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), TRUE );
	g_signal_connect ( win->psb, "unmount", G_CALLBACK ( gmf_win_psb_unmount ), win );
	g_signal_connect ( win->psb, "open-location", G_CALLBACK ( gmf_win_psb_open_location ), win );

	win->icon_view = gmf_icon_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( win->icon_view ), TRUE );
	g_signal_connect ( win->icon_view, "icon-button-press", G_CALLBACK ( gmf_win_handler_button_press ), win );

	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( win->psb ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( win->icon_view ) );
	gtk_widget_set_visible ( GTK_WIDGET ( paned ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( GTK_WINDOW ( win ) );
}

static void gmf_win_init ( GmfWin *win )
{
	gmf_win_create ( win );
}

static void gmf_win_finalize ( GObject *object )
{
	G_OBJECT_CLASS (gmf_win_parent_class)->finalize (object);
}

static void gmf_win_class_init ( GmfWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = gmf_win_finalize;
}

GmfWin * gmf_win_new ( const char *path, GmfApp *app )
{
	GmfWin *win = g_object_new ( GMF_TYPE_WIN, "application", app, NULL );

	g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );

	return win;
}
