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

#include "copy-win.h"
#include "info-win.h"

#include <glib/gstdio.h>

struct _GmfWin
{
	GtkWindow parent_instance;

	GmfBar *bar;
	GmfIcon *icon_view;
	GtkPlacesSidebar *psb;

	GtkPopover *popover_pref;

	enum cm_num cp_mv;

	uint16_t width;
	uint16_t height;

	uint8_t opacity;
	uint16_t icon_size;

	gboolean dark;
	gboolean preview;
	gboolean unmount_set_home;

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

static void gmf_win_psb_popup ( G_GNUC_UNUSED GtkPlacesSidebar *sb, G_GNUC_UNUSED GtkWidget *container, G_GNUC_UNUSED GFile *s_item, GVolume *s_vol, GmfWin *win )
{
	win->unmount_set_home = FALSE;

	if ( s_vol )
	{
		GMount *mount = g_volume_get_mount ( s_vol );

		if ( mount )
		{
			GFile *mount_root = g_mount_get_root ( mount );
			g_autofree char *path_mr = g_file_get_path ( mount_root );

			g_autofree char *path = NULL;
			g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

			if ( g_str_has_prefix ( path, path_mr ) ) win->unmount_set_home = TRUE;

			if ( mount_root ) g_object_unref ( mount_root );
			g_object_unref ( mount );
		}
	}
}

static void gmf_win_psb_unmount ( G_GNUC_UNUSED GtkPlacesSidebar *sb, G_GNUC_UNUSED GMountOperation *mount_op, GmfWin *win )
{
	if ( win->unmount_set_home ) g_signal_emit_by_name ( win->icon_view, "icon-set-path", g_get_home_dir () );

	win->unmount_set_home = FALSE;
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

	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_autofree char *path_set = g_path_get_dirname ( path );

	GFile *location = g_file_new_for_path ( path_set );

	gtk_places_sidebar_set_location ( win->psb, location );

	if ( location ) g_object_unref ( location );
}

static void gmf_win_clic_act ( GmfWin *win )
{
	uint num = 0;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-num", &num );

	g_signal_emit_by_name ( win->bar, "bar-set-act", num );
}

static void gmf_win_mount_acces ( GObject *obj, GAsyncResult * res, gpointer data )
{
	GmfWin *win = (GmfWin *)data;

	GError *err = NULL;
	gboolean ret = g_file_mount_enclosing_volume_finish ( G_FILE ( obj ), res, &err );

	if ( ret )
	{
		g_autofree char *fpath = g_file_get_path ( G_FILE ( obj ) );

		g_signal_emit_by_name ( win->icon_view, "icon-set-path", fpath );
	}
}

static void gmf_win_clic_jmp ( const char *path, GmfWin *win )
{
	GFile *file = g_file_parse_name ( path );

	g_autofree char *fpath = g_file_get_path ( file );
	g_autofree char *furi  = g_file_get_uri  ( file );

	if ( !fpath && furi )
	{
		GMountOperation *mop = gtk_mount_operation_new ( GTK_WINDOW ( win ) );
		gtk_mount_operation_set_screen ( GTK_MOUNT_OPERATION ( mop ), gdk_screen_get_default () );

		g_file_mount_enclosing_volume ( file, G_MOUNT_MOUNT_NONE, mop, NULL, gmf_win_mount_acces, win ); // launch cmd: gio mount uri

		g_object_unref ( mop );
	}
	else
	{
		if ( fpath ) g_signal_emit_by_name ( win->icon_view, "icon-set-path", fpath );
	}

	if ( file ) gtk_places_sidebar_set_location ( win->psb, file );

	if ( file ) g_object_unref ( file );
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

	if ( !list ) return;

	while ( list != NULL )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		GError *error = NULL;
		g_file_trash ( file, NULL, &error );

		if ( error ) { gmf_message_dialog ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

		list = list->next;
		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_clic_arc ( GmfWin *win )
{
	g_autofree char *prog_roller = g_find_program_in_path ( "file-roller" );
	g_autofree char *prog_engrampa = g_find_program_in_path ( "engrampa" );

	if ( !prog_roller && !prog_engrampa )
	{
		gmf_message_dialog ( "Not Found:", "File-roller or Engrampa", GTK_MESSAGE_WARNING, GTK_WINDOW ( win ) );

		return;
	}

	const char *cmd = ( !prog_roller && prog_engrampa ) ? "engrampa --add " : "file-roller --add ";

	GString *gstring = g_string_new ( cmd );

	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( !list ) return;

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

	if ( !list ) return;

	if ( g_list_length ( list ) == 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		gmf_app_chooser_dialog ( file, NULL, GTK_WINDOW ( win ) );

		g_object_unref ( file );
	}
	else if ( g_list_length ( list ) > 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		gmf_app_chooser_dialog ( file, list, GTK_WINDOW ( win ) );

		g_object_unref ( file );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_clic_inf ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	double opacity = gtk_widget_get_opacity ( GTK_WIDGET ( win ) );

	if ( list )
	{
		if ( g_list_length ( list ) == 1 )
			info_win_new ( I_SEL_1, path, list, NULL, opacity );
		else
			info_win_new ( I_SEL_N, path, list, NULL, opacity );
	}
	else
	{
		info_win_new ( I_SEL_0, path, NULL, NULL, opacity );
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
		double opacity = gtk_widget_get_opacity ( GTK_WIDGET ( win ) );

		char **fields = g_strsplit ( clipboard_text, "\n", 0 );
		uint j = 0, numfields = g_strv_length ( fields );

		char *uris[numfields+1];

		for ( j = 0; j < numfields; j++ ) uris[j] = fields[j];

		uris[j] = NULL;

		if ( numfields ) copy_win_new ( win->cp_mv, path, uris, NULL, opacity );

		g_strfreev ( fields );
		free ( clipboard_text );
	}
}

static void gmf_win_set_clipboard ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( !list ) return;

	GString *gstring = g_string_new ( (char *)list->data );
	list = list->next;

	while ( list != NULL )
	{
		g_string_prepend ( gstring, "\n" );
		g_string_prepend ( gstring, (char *)list->data );

		list = list->next;
	}

	GtkClipboard *clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD );

	gtk_clipboard_set_text ( clipboard, gstring->str, -1 );

	g_string_free ( gstring, TRUE );
	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_clic_cpy ( GmfWin *win )
{
	win->cp_mv = CM_CP;
	gmf_win_set_clipboard ( win );
}

static void gmf_win_clic_cut ( GmfWin *win )
{
	win->cp_mv = CM_MV;
	gmf_win_set_clipboard ( win );
}

static void gmf_win_clic_pst ( GmfWin *win )
{
	gmf_win_copy ( win );
}

static void gmf_win_clic_ndr ( GmfWin *win )
{
	gmf_dfe_dialog ( NULL, D_DR, win );
}

static void gmf_win_clic_nfl ( GmfWin *win )
{
	gmf_dfe_dialog ( NULL, D_FL, win );
}

static void gmf_win_clic_edt ( GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 1 )
	{
		g_autofree char *name = g_path_get_basename ( (char *)list->data );

		gmf_dfe_dialog ( name, D_ED, win );
	}

	g_list_free_full ( list, (GDestroyNotify) g_free );
}

static void gmf_win_dir_new ( const char *name, GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_autofree char *path_new = g_strdup_printf ( "%s/%s", path, name );

	GFile *file_new = g_file_new_for_path ( path_new );

	GError *error = NULL;
	g_file_make_directory ( file_new, NULL, &error );

	if ( error ) { gmf_message_dialog ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

	g_object_unref ( file_new );
}

static void gmf_win_file_new ( const char *name, GmfWin *win )
{
	g_autofree char *path = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-path", &path );

	g_autofree char *path_new = g_strdup_printf ( "%s/%s", path, name );

	GFile *file_new = g_file_new_for_path ( path_new );

	GError *error = NULL;
	g_file_create ( file_new, G_FILE_CREATE_NONE, NULL, &error );

	if ( error ) { gmf_message_dialog ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

	g_object_unref ( file_new );
}

static void gmf_win_edit_name ( const char *name, GmfWin *win )
{
	GList *list = NULL;
	g_signal_emit_by_name ( win->icon_view, "icon-get-selected-path", &list );

	if ( g_list_length ( list ) == 1 )
	{
		GFile *file = g_file_new_for_path ( (char *)list->data );

		GError *error = NULL;
		g_file_set_display_name ( file, name, NULL, &error );

		if ( error ) { gmf_message_dialog ( " ", error->message, GTK_MESSAGE_ERROR, GTK_WINDOW ( win ) ); g_error_free ( error ); }

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

	GFile *location = g_file_new_for_path ( path );

	gtk_places_sidebar_set_location ( win->psb, location );

	if ( location ) g_object_unref ( location );
}

static void gmf_win_handler_button_press ( G_GNUC_UNUSED GmfIcon *icon_view, uint button_n, GmfWin *win )
{
	if ( button_n == 3 ) g_signal_emit_by_name ( win->bar, "bar-run-act" );

	if ( button_n == 2 ) gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), !gtk_widget_get_visible ( GTK_WIDGET ( win->psb ) ) );
}

static void gmf_win_handler_add_tab ( G_GNUC_UNUSED GmfIcon *icon_view, const char *path, GmfWin *win )
{
	g_signal_emit_by_name ( win->bar, "bar-set-tab", path );

	g_autofree char *name = g_path_get_basename ( path );
	g_autofree char *string = g_strdup_printf ( "%s - Gmf", name );

	gtk_window_set_title ( GTK_WINDOW ( win ), string );
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
	uint8_t opacity = (uint8_t)gtk_spin_button_get_value_as_int ( button );

	win->opacity = opacity;

	gtk_widget_set_opacity ( GTK_WIDGET ( win ), ( double )opacity / 100 );
}

static void gmf_spinbutton_changed_size ( GtkSpinButton *button, GmfWin *win )
{
	uint16_t icon_size = (uint16_t)gtk_spin_button_get_value_as_int ( button );

	win->icon_size = icon_size;

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

	gmf_about ( GTK_WINDOW ( win ) );
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

static void gmf_win_active_switch_slow ( GObject *gobject, G_GNUC_UNUSED GParamSpec *pspec, GmfWin *win )
{
	gboolean state = gtk_switch_get_state ( GTK_SWITCH ( gobject ) );

	win->preview = state;
	g_signal_emit_by_name ( win->icon_view, "icon-set-slow", state );
}

static void gmf_win_active_switch_prw ( GObject *gobject, G_GNUC_UNUSED GParamSpec *pspec, GmfWin *win )
{
	gboolean state = gtk_switch_get_state ( GTK_SWITCH ( gobject ) );

	win->preview = state;
	g_signal_emit_by_name ( win->icon_view, "icon-set-preview", state );
}

static GtkSwitch * gmf_win_create_switch ( gboolean state, void (*f), GmfWin *win )
{
	GtkSwitch *gswitch = (GtkSwitch *)gtk_switch_new ();
	gtk_switch_set_state ( gswitch, state );
	gtk_widget_set_visible (  GTK_WIDGET ( gswitch ), TRUE );

	g_signal_connect ( gswitch, "notify::active", G_CALLBACK ( f ), win );

	return gswitch;
}

static void gmf_win_changed_sort ( GtkComboBoxText *combo_box, GmfWin *win )
{
	uint num = (uint)gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo_box ) );

	g_signal_emit_by_name ( win->icon_view, "icon-set-sort-func", num );
}

static GtkComboBoxText * gmf_win_create_combo ( GmfWin *win )
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

static GtkPopover * gmf_popover_pref ( GmfWin *win )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( vbox ), TRUE );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_create_spinbutton ( win->icon_size,  16, 512, 1, "Icon-size", gmf_spinbutton_changed_size, win    ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_create_spinbutton ( win->opacity,    40, 100, 1, "Opacity",   gmf_spinbutton_changed_opacity, win ) ), FALSE, FALSE, 0 );

	GtkBox *hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	GtkLabel *label = (GtkLabel *)gtk_label_new ( " Slow " );
	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	GtkSwitch *gswitch = gmf_win_create_switch ( FALSE, gmf_win_active_switch_slow, win );
	gtk_box_pack_end ( hbox, GTK_WIDGET ( gswitch ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox , GTK_WIDGET ( hbox ), FALSE, FALSE, 0 );

	hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( hbox, 5 );
	gtk_widget_set_visible ( GTK_WIDGET ( hbox ), TRUE );

	label = (GtkLabel *)gtk_label_new ( " Preview " );
	gtk_widget_set_halign ( GTK_WIDGET ( label ), GTK_ALIGN_START );
	gtk_widget_set_visible ( GTK_WIDGET ( label ), TRUE );
	gtk_box_pack_start ( hbox, GTK_WIDGET ( label ), FALSE, FALSE, 0 );

	gswitch = gmf_win_create_switch ( win->preview, gmf_win_active_switch_prw, win );
	gtk_box_pack_end ( hbox, GTK_WIDGET ( gswitch ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox , GTK_WIDGET ( hbox ), FALSE, FALSE, 0 );

	GtkComboBoxText *combo = gmf_win_create_combo ( win );
	gtk_box_pack_start ( vbox , GTK_WIDGET ( combo ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Icons", "/usr/share/icons/",  gmf_changed_icons, win ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( gmf_win_create_theme ( "Theme", "/usr/share/themes/", gmf_changed_theme, win ) ), FALSE, FALSE, 0 );

	hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
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
	gtk_window_set_default_size ( window, win->width, win->height );
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
	g_object_set ( win->psb, "populate-all", TRUE, NULL );
	gtk_places_sidebar_set_open_flags ( win->psb, GTK_PLACES_OPEN_NORMAL );
	g_signal_connect ( win->psb, "unmount", G_CALLBACK ( gmf_win_psb_unmount ), win );
	g_signal_connect ( win->psb, "populate-popup", G_CALLBACK ( gmf_win_psb_popup ), win );
	g_signal_connect ( win->psb, "open-location", G_CALLBACK ( gmf_win_psb_open_location ), win );

	win->icon_view = gmf_icon_new ();
	g_signal_connect ( win->icon_view, "icon-add-tab", G_CALLBACK ( gmf_win_handler_add_tab ), win );
	g_signal_connect ( win->icon_view, "icon-button-press", G_CALLBACK ( gmf_win_handler_button_press ), win );

	gtk_widget_set_visible ( GTK_WIDGET ( win->psb ), TRUE );
	gtk_widget_set_visible ( GTK_WIDGET ( win->icon_view ), TRUE );

	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( win->psb ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( win->icon_view ) );
	gtk_widget_set_visible ( GTK_WIDGET ( paned ), TRUE );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( v_box ) );

	gtk_window_present ( GTK_WINDOW ( win ) );

	g_signal_connect ( win, "win-apply-dfe", G_CALLBACK ( gmf_win_apply_dfe_handler ), NULL );
}

static gboolean gmf_win_get_settings ( const char *schema_id )
{
	GSettingsSchemaSource *schemasrc = g_settings_schema_source_get_default ();
	GSettingsSchema *schema = g_settings_schema_source_lookup ( schemasrc, schema_id, FALSE );

	if ( schema == NULL ) g_critical ( "%s:: schema: %s - not installed.", __func__, schema_id );

	if ( schema == NULL ) return FALSE;

	return TRUE;
}

static void gmf_save_pref ( GmfWin *win )
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
	g_settings_set_boolean ( settings, "preview", win->preview );

	g_settings_set_string  ( settings, "theme", theme );
	g_settings_set_string  ( settings, "icon-theme", icon_theme );

	g_settings_set_uint    ( settings, "opacity",    win->opacity );
	g_settings_set_uint    ( settings, "icon-size",  win->icon_size  );

	g_object_unref ( settings );
}

static void gmf_win_load_pref ( GmfWin *win )
{
	const char *schema_id = "org.gtk.gmf";
	if ( !gmf_win_get_settings ( schema_id ) ) return;

	GSettings *settings = g_settings_new ( schema_id );

	win->dark    = g_settings_get_boolean ( settings, "dark" );
	win->preview = g_settings_get_boolean ( settings, "preview" );

	win->opacity    = (uint8_t)g_settings_get_uint ( settings, "opacity" );
	win->icon_size  = (uint16_t)g_settings_get_uint ( settings, "icon-size" );

	win->width  = (uint16_t)g_settings_get_uint ( settings, "width"  );
	win->height = (uint16_t)g_settings_get_uint ( settings, "height" );

	g_object_unref ( settings );
}

static void gmf_win_set_pref ( const char *path, GmfWin *win )
{
	const char *schema_id = "org.gtk.gmf";
	if ( !gmf_win_get_settings ( schema_id ) ) return;

	GSettings *settings = g_settings_new ( schema_id );

	g_autofree char *theme      = g_settings_get_string  ( settings, "theme" );
	g_autofree char *icon_theme = g_settings_get_string  ( settings, "icon-theme" );

	if ( theme && !g_str_has_prefix ( theme, "none" ) ) g_object_set ( gtk_settings_get_default (), "gtk-theme-name", theme, NULL );
	if ( icon_theme && !g_str_has_prefix ( icon_theme, "none" ) ) g_object_set ( gtk_settings_get_default (), "gtk-icon-theme-name", icon_theme, NULL );

	g_object_set ( gtk_settings_get_default (), "gtk-application-prefer-dark-theme", win->dark, NULL );

	gtk_widget_set_opacity ( GTK_WIDGET ( win ), (double)win->opacity / 100 );

	g_signal_emit_by_name ( win->icon_view, "icon-set-props", path, win->icon_size, win->preview );

	g_object_unref ( settings );
}

typedef struct _FuncAction FuncAction;

struct _FuncAction
{
	void (*f)();
	const char *func_name;

	uint mod_key;
	uint gdk_key;
};

static void gmf_action_all ( GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, gpointer data )
{
	GmfWin *act_win = GMF_WIN ( gtk_application_get_active_window ( (GtkApplication *)data ) );

	if ( !act_win ) return;

	g_autofree char *name = NULL;
	g_object_get ( sl, "name", &name, NULL );

	if ( g_str_equal ( "cut",    name ) ) gmf_win_clic_cut ( act_win );
	if ( g_str_equal ( "copy",   name ) ) gmf_win_clic_cpy ( act_win );
	if ( g_str_equal ( "paste",  name ) ) gmf_win_clic_pst ( act_win );
	if ( g_str_equal ( "hidden", name ) ) gmf_win_clic_hdn ( act_win );
	if ( g_str_equal ( "delete", name ) ) gmf_win_clic_rmv ( act_win );
}

static void _app_add_accelerator ( const char *func_name, uint mod_key, uint gdk_key, GtkApplication *app )
{
	g_autofree char *accel_name = gtk_accelerator_name ( gdk_key, mod_key );

	const char *accel_str[] = { accel_name, NULL };

	g_autofree char *text = g_strconcat ( "app.", func_name, NULL );

	gtk_application_set_accels_for_action ( app, text, accel_str );
}

static void gmf_app_action ( GmfApp *_app )
{
	GtkApplication *app = GTK_APPLICATION ( _app );

	static FuncAction func_action_n[] =
        {
                { gmf_action_all, "cut",    GDK_CONTROL_MASK, GDK_KEY_X },
                { gmf_action_all, "copy",   GDK_CONTROL_MASK, GDK_KEY_C },
                { gmf_action_all, "paste",  GDK_CONTROL_MASK, GDK_KEY_V },
                { gmf_action_all, "hidden", GDK_CONTROL_MASK, GDK_KEY_H },
                { gmf_action_all, "delete", 0, GDK_KEY_Delete }
        };

        GActionEntry entries[ G_N_ELEMENTS ( func_action_n ) ];

	uint8_t i = 0; for ( i = 0; i < G_N_ELEMENTS ( func_action_n ); i++ )
	{
		entries[i].name           = func_action_n[i].func_name;
		entries[i].activate       = func_action_n[i].f;
		entries[i].parameter_type = NULL;
		entries[i].state          = NULL;

		_app_add_accelerator ( func_action_n[i].func_name, func_action_n[i].mod_key, func_action_n[i].gdk_key, app );
	}

	g_action_map_add_action_entries ( G_ACTION_MAP ( app ), entries, G_N_ELEMENTS ( entries ), app );
}

static void gmf_win_init ( GmfWin *win )
{
	win->cp_mv = CM_CP;

	win->width  = 700;
	win->height = 350;

	win->opacity = 100;
	win->icon_size = 48;

	win->preview = TRUE;
	win->unmount_set_home = FALSE;

	win->debug = ( g_getenv ( "GMF_DEBUG" ) ) ? TRUE : FALSE;

	gmf_win_load_pref ( win );
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

		const char *schema_id = "org.gtk.gmf";
		if ( !gmf_win_get_settings ( schema_id ) ) return;

		GSettings *settings = g_settings_new ( schema_id );

		if ( settings ) g_settings_set_uint ( settings, "width",  set_width  );
		if ( settings ) g_settings_set_uint ( settings, "height", set_height );

		g_object_unref ( settings );
	}

	G_OBJECT_CLASS ( gmf_win_parent_class )->dispose ( object );
}

static void gmf_win_finalize ( GObject *object )
{
	GmfWin *win = GMF_WIN ( object );

	gmf_save_pref ( win );

	G_OBJECT_CLASS (gmf_win_parent_class)->finalize (object);
}

static void gmf_win_class_init ( GmfWinClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->dispose  = gmf_win_dispose;
	oclass->finalize = gmf_win_finalize;

	g_signal_new ( "win-apply-dfe", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );
}

GmfWin * gmf_win_new ( const char *path, GmfApp *app )
{
	GmfWin *win = g_object_new ( GMF_TYPE_WIN, "application", app, NULL );

	gmf_app_action ( app );

	gmf_win_set_pref ( path, win );

	return win;
}
