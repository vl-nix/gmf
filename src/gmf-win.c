/*
* Copyright 2021 Stepan Perun
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

	GmfBar *bar;
	GmfIcon *icon_view;
	GtkPlacesSidebar *psb;

	GtkPopover *popover_pref;

	enum c_num cp_mv;

	gboolean debug;
};

G_DEFINE_TYPE ( GmfWin, gmf_win, GTK_TYPE_WINDOW )

typedef void ( *fp ) ( GmfWin * );

static void gmf_win_psb_open_location ( G_GNUC_UNUSED GtkPlacesSidebar *sb, GObject *location, G_GNUC_UNUSED GtkPlacesOpenFlags flags, GmfWin *win )
{
	GFile *file = (GFile *)location;

	g_autofree char *uri  = g_file_get_uri  ( file );
	g_autofree char *path = g_file_get_path ( file );

	if ( path )
	{
		g_signal_emit_by_name ( win->bar, "bar-set-tab", path );
		g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );
	}
	else
	{
		if ( g_str_has_prefix ( uri, "trash"  ) ) gmf_trash_dialog  ( GTK_WINDOW ( win ) );
		if ( g_str_has_prefix ( uri, "recent" ) ) gmf_recent_dialog ( GTK_WINDOW ( win ) );
	}
}

static void gmf_win_psb_unmount ( G_GNUC_UNUSED GtkPlacesSidebar *sb, G_GNUC_UNUSED GMountOperation *mount_op, GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-path", g_get_home_dir () );
}

static void gmf_win_clic_win ( GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	char cmd[PATH_MAX];
	sprintf ( cmd, "gmf %s", path );

	launch_cmd ( cmd, GTK_WINDOW ( win ) );
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

static void gmf_win_clic_jmp ( const char *path, GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );
}

static void gmf_win_clic_srh ( const char *str, GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-search", str );
}

static void gmf_win_clic_rld ( GmfWin *win )
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

static void gmf_win_clic_rmv ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	while ( list != NULL )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		GError *err = NULL;
		g_file_trash ( file, NULL, &err );

		if ( err != NULL )
		{
			message_dialog ( "", err->message, GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );
			g_error_free ( err );
		}

		list = list->next;
		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
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

static void gmf_win_clic_run ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		gmf_app_chooser_dialog ( file, GTK_WINDOW ( win ) );

		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_clic_inf ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		gmf_info_dialog ( file, win );

		g_object_unref ( file );
	}
	else
	{
		g_autofree char *path = NULL;
		g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

		GFile *file = g_file_new_for_path ( path );

		gmf_info_dialog ( file, win );

		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_copy ( GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

	char *clipboard_text = gtk_clipboard_wait_for_text ( clipboard );

	if ( clipboard_text )
	{
		char **fields = g_strsplit ( clipboard_text, "\n", 0 );
		uint j = 0, numfields = g_strv_length ( fields );

		char *uris[numfields+1];

		for ( j = 0; j < numfields; j++ ) uris[j] = fields[j];

		uris[j] = NULL;

		if ( numfields ) gmf_copy_dialog ( path, uris, win->cp_mv, NULL );

		g_strfreev ( fields );
		free ( clipboard_text );
	}
}

static void gmf_win_set_clipboard ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	GString *gstring = g_string_new ( (char *)list->data );
	list = list->next;

	while ( list != NULL )
	{
		g_string_append_printf ( gstring, "\n%s", (char *)list->data );
		list = list->next;
	}

	GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

	gtk_clipboard_set_text ( clipboard, gstring->str, -1 );

	g_string_free ( gstring, TRUE );
	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_clic_cpy ( GmfWin *win )
{
	win->cp_mv = C_CP;
	gmf_win_set_clipboard ( win );
}

static void gmf_win_clic_cut ( GmfWin *win )
{
	win->cp_mv = C_MV;
	gmf_win_set_clipboard ( win );
}

static void gmf_win_clic_pst ( GmfWin *win )
{
	gmf_win_copy ( win );
}

static void gmf_win_clic_ndr ( GmfWin *win )
{
	gmf_dfe_dialog ( D_DR, win );
}

static void gmf_win_clic_nfl ( GmfWin *win )
{
	gmf_dfe_dialog ( D_FL, win );
}

static void gmf_win_clic_edt ( GmfWin *win )
{
	gmf_dfe_dialog ( D_ED, win );
}

static void gmf_win_dir_new ( const char *name, GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_autofree char *path_new = g_strdup_printf ( "%s/%s", path, name );

	GFile *file_new = g_file_new_for_path ( path_new );

	g_file_make_directory ( file_new, NULL, NULL );

	g_object_unref ( file_new );
}

static void gmf_win_file_new ( const char *name, GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_autofree char *path_new = g_strdup_printf ( "%s/%s", path, name );

	GFile *file_new = g_file_new_for_path ( path_new );

	g_file_create ( file_new, G_FILE_CREATE_NONE, NULL, NULL );

	g_object_unref ( file_new );
}

static void gmf_win_edit_name ( const char *name, GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		g_file_set_display_name ( file, name, NULL, NULL );

		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_apply_dfe_handler ( GmfWin *win, uint8_t num, const char *string )
{
	if ( num == D_DR ) gmf_win_dir_new ( string, win );
	if ( num == D_FL ) gmf_win_file_new ( string, win );
	if ( num == D_ED ) gmf_win_edit_name ( string, win );

	if ( win->debug ) g_message ( "%s:: num: %u, string: %s ", __func__, num, string );
}

static void gmf_win_clicked_handler_num ( G_GNUC_UNUSED GmfBar *bar, uint8_t num, const char *string, GmfWin *win )
{
	if ( num == BT_JMP ) gmf_win_clic_jmp ( string, win );
	if ( num == BT_SRH ) gmf_win_clic_srh ( string, win );

	if ( num == BT_TAB || num == BT_JMP || num == BT_SRH || num == BT_PRF ) return;

	fp funcs[] = { gmf_win_clic_win, NULL, gmf_win_clic_up, NULL, gmf_win_clic_act, NULL, gmf_win_clic_rld, gmf_win_clic_inf, NULL };

	if ( funcs[num] ) funcs[num] ( win );

	if ( win->debug ) g_message ( "%s:: num: %u, name-act-data: %s ", __func__, num, string );
}

static void gmf_win_clicked_pp_handler_num ( G_GNUC_UNUSED GmfBar *bar, uint8_t num, const char *string, GmfWin *win )
{
	fp funcs[] =  { gmf_win_clic_ndr, gmf_win_clic_nfl, gmf_win_clic_pst, gmf_win_clic_hdn, gmf_win_clic_trm, 
			gmf_win_clic_edt, gmf_win_clic_cpy, gmf_win_clic_cut, gmf_win_clic_rmv, gmf_win_clic_arc, gmf_win_clic_run };

	funcs[num] ( win );

	if ( win->debug ) g_message ( "%s:: num: %u, name-act-data: %s ", __func__, num, string );
}

static void gmf_win_clicked_handler_act_tab ( G_GNUC_UNUSED GmfBar *bar, const char *path, GmfWin *win )
{
	g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );
}

static void gmf_win_handler_button_press ( G_GNUC_UNUSED GmfIcon *icon_view, GmfWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-run-act" );
}

static void gmf_win_handler_add_tab ( G_GNUC_UNUSED GmfIcon *icon_view, const char *path, GmfWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-set-tab", path );
}

static void gmf_win_clicked_handler_act_prf ( G_GNUC_UNUSED GmfBar *bar, GObject *obj, GmfWin *win )
{
	gtk_popover_set_relative_to ( win->popover_pref, GTK_WIDGET ( obj ) );
	gtk_popover_popup ( win->popover_pref );
}

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

	gmf_win_clic_rld ( win );
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
	uint opacity = (uint)gtk_spin_button_get_value_as_int ( button );

	gtk_widget_set_opacity ( GTK_WIDGET ( win ), ( double )opacity / 100 );
}

static void gmf_spinbutton_changed_size ( GtkSpinButton *button, GmfWin *win )
{
	uint icon_size = (uint)gtk_spin_button_get_value_as_int ( button );

	g_signal_emit_by_name ( win->icon_view, "icon-set-size", icon_size );
}

static GtkSpinButton * gmf_create_spinbutton ( uint val, uint8_t min, uint32_t max, uint8_t step, const char *text, void ( *f )( GtkSpinButton *, GmfWin * ), GmfWin *win )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );
	g_signal_connect ( spinbutton, "changed", G_CALLBACK ( f ), win );

	gtk_entry_set_icon_from_icon_name ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, "info" );
	gtk_entry_set_icon_tooltip_text   ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, text );

	gtk_widget_set_visible ( GTK_WIDGET ( spinbutton ), TRUE );

	return spinbutton;
}

static void gmf_clicked_dark ( G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED GmfWin *win )
{
	gboolean dark = FALSE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );
}

static void gmf_clicked_about ( G_GNUC_UNUSED GtkButton *button, GmfWin *win )
{
	gtk_widget_set_visible ( GTK_WIDGET ( win->popover_pref ), FALSE );

	about ( GTK_WINDOW ( win ) );
}

static void gmf_clicked_quit ( G_GNUC_UNUSED GtkButton *button, GmfWin *win )
{
	gtk_widget_destroy ( GTK_WIDGET ( win ) );
}

static GtkButton * gmf_win_create_button ( const char *icon_name, void ( *f )( GtkButton *, GmfWin * ), GmfWin *win )
{
	GtkButton *button = button_set_image ( icon_name, 16 );
	g_signal_connect ( button, "clicked", G_CALLBACK ( f ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( button ), TRUE );

	return button;
}

static GtkPopover * gmf_popover_pref ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( vbox ), TRUE );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_create_spinbutton ( 48,  16, 1024, 1, "Icon-size", gmf_spinbutton_changed_size, win ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_create_spinbutton ( 100, 40, 100,  1, "Opacity",   gmf_spinbutton_changed_opacity, win ) ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Icons", "/usr/share/icons/",  gmf_changed_icons, win ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Theme", "/usr/share/themes/", gmf_changed_theme, win ) ), FALSE, FALSE, 0 );

	GtkBox *hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "weather-clear-night", gmf_clicked_dark,  win ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "dialog-info",         gmf_clicked_about, win ) ), TRUE, TRUE, 0 );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( gmf_win_create_button ( "window-close",        gmf_clicked_quit,  win ) ), TRUE, TRUE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( hbox ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( vbox ) );

	return popover;
}

static void gmf_win_create ( GmfWin *win )
{
	GtkWindow *window = GTK_WINDOW ( win );
	gtk_window_set_title ( window, "Gmf" );
	gtk_window_set_default_size ( window, 700, 350 );
	gtk_window_set_icon_name ( window, "system-file-manager" );

	win->popover_pref = gmf_popover_pref ( win );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	gtk_widget_set_visible ( GTK_WIDGET ( v_box ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	win->bar = gmf_bar_new ();
	gtk_box_pack_start ( v_box, GTK_WIDGET ( win->bar ), FALSE, FALSE, 0 );
	g_signal_connect ( win->bar, "bar-act-tab", G_CALLBACK ( gmf_win_clicked_handler_act_tab ), win );
	g_signal_connect ( win->bar, "bar-act-prf", G_CALLBACK ( gmf_win_clicked_handler_act_prf ), win );
	g_signal_connect ( win->bar, "bar-click-num", G_CALLBACK ( gmf_win_clicked_handler_num ), win );
	g_signal_connect ( win->bar, "bar-pp-click-num", G_CALLBACK ( gmf_win_clicked_pp_handler_num ), win );

	win->psb = (GtkPlacesSidebar *)gtk_places_sidebar_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), TRUE );
	g_signal_connect ( win->psb, "unmount", G_CALLBACK ( gmf_win_psb_unmount ), win );
	g_signal_connect ( win->psb, "open-location", G_CALLBACK ( gmf_win_psb_open_location ), win );

	win->icon_view = gmf_icon_new ();
	gtk_widget_set_visible ( GTK_WIDGET ( win->icon_view ), TRUE );
	g_signal_connect ( win->icon_view, "icon-add-tab", G_CALLBACK ( gmf_win_handler_add_tab ), win );
	g_signal_connect ( win->icon_view, "icon-button-press", G_CALLBACK ( gmf_win_handler_button_press ), win );

	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( win->psb ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( win->icon_view ) );
	gtk_widget_set_visible ( GTK_WIDGET ( paned ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( GTK_WINDOW ( win ) );

	g_signal_connect ( win, "win-apply-dfe", G_CALLBACK ( gmf_win_apply_dfe_handler ), NULL );
}

static void gmf_win_init ( GmfWin *win )
{
	win->cp_mv = C_CP;
	win->debug = ( g_getenv ( "GMF_DEBUG" ) ) ? TRUE : FALSE;

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

	g_signal_new ( "win-apply-dfe", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );
}

GmfWin * gmf_win_new ( const char *path, GmfApp *app )
{
	GmfWin *win = g_object_new ( GMF_TYPE_WIN, "application", app, NULL );

	g_signal_emit_by_name ( win->icon_view, "icon-set-path", path );

	return win;
}
