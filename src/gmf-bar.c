/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-bar.h"
#include "gmf-dialog.h"

struct _GmfBar
{
	GtkBox parent_instance;

	GtkButton *button[BT_ALL];
	GtkButton *button_pp[BTP_ALL];

	GtkPopover *popover_tab;
	GtkButton *button_tab[MAX_TAB];

	GtkPopover *popover_act;
	GtkPopover *popover_jump;
	GtkPopover *popover_search;
};

G_DEFINE_TYPE ( GmfBar, gmf_bar, GTK_TYPE_BOX )

const char *b_icons[BT_ALL] = 
{
	[BT_WIN] = "window-new",
	[BT_TAB] = "tab-new",
	[BT_GUP] = "go-up",
	[BT_JMP] = "go-jump",
	[BT_ACT] = "gnome-panel",
	[BT_SRH] = "system-search",
	[BT_RLD] = "reload",
	[BT_INF] = "dialog-info",
	[BT_PRF] = "preferences-system"
};

const char *bp_icons[BTP_ALL] = 
{
	[BTP_NDR] = "folder-new",
	[BTP_NFL] = "document-new",
	[BTP_PST] = "edit-paste", 
	[BTP_HDN] = "䷽",
	[BTP_TRM] = "terminal",
	[BTP_EDT] = "gtk-edit",
	[BTP_CPY] = "edit-copy", 
	[BTP_CUT] = "edit-cut",
	[BTP_RMV] = "remove",
	[BTP_ARC] = "archive-insert",
	[BTP_RUN] = "system-run"
};

static void gmf_bar_button_handler_run_act ( GmfBar *bar )
{
	gtk_button_clicked ( bar->button[BT_ACT] );
}

static void gmf_bar_button_handler_set_act ( GmfBar *bar, uint num )
{
	enum b_pp_num n = BTP_NDR, c = ( !num ) ? BTP_NDR : BTP_EDT, a = ( !num ) ? BTP_EDT : BTP_ALL;

	for ( n = BTP_NDR; n < BTP_ALL; n++ ) gtk_widget_set_visible ( GTK_WIDGET ( bar->button_pp[n] ), FALSE );

	for ( n = c; n < a; n++ ) gtk_widget_set_visible ( GTK_WIDGET ( bar->button_pp[n] ), TRUE );
}

static void gmf_bar_signal_handler_pp_num ( GtkButton *button, GmfBar *bar )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	g_signal_emit_by_name ( bar, "bar-pp-click-num", num, bp_icons[num] );

	gtk_widget_set_visible ( GTK_WIDGET ( bar->popover_act ), FALSE );
}

static GtkPopover * gmf_bar_pp_act ( GmfBar *bar )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	enum b_pp_num n = BTP_NDR;

	for ( n = BTP_NDR; n < BTP_ALL; n++ )
	{
		bar->button_pp[n] = ( n == BTP_HDN ) 
			? (GtkButton *)gtk_button_new_with_label ( bp_icons[n] ) 
			: button_set_image ( bp_icons[n], 20 );

		g_signal_connect ( bar->button_pp[n], "clicked", G_CALLBACK ( gmf_bar_signal_handler_pp_num ), bar );

		char buf[10];
		sprintf ( buf, "%u", n );
		gtk_widget_set_name ( GTK_WIDGET ( bar->button_pp[n] ), buf );

		gtk_button_set_relief ( bar->button_pp[n], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( bar->button_pp[n] ), TRUE );

		gtk_box_pack_start ( h_box, GTK_WIDGET ( bar->button_pp[n] ), TRUE, TRUE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

static void gmf_bar_button_set_tab ( const char *path, uint8_t n, GmfBar *bar )
{
	g_autofree char *name = g_path_get_basename ( path );

	gtk_button_set_label ( bar->button_tab[n], name );
	gtk_widget_set_name ( GTK_WIDGET ( bar->button_tab[n] ), path );
}

static void gmf_bar_button_handler_set_tab ( GmfBar *bar, const char *path )
{
	if ( path == NULL ) return;

	GList *l = NULL;
	gboolean found = FALSE;

	uint8_t n = 0; for ( n = 0; n < MAX_TAB; n++ )
	{
		const char *label = gtk_button_get_label ( bar->button_tab[n] );
		const char *name = gtk_widget_get_name ( GTK_WIDGET ( bar->button_tab[n] ) );

		if ( g_str_has_prefix ( label, "none" ) ) break;

		if ( g_str_equal ( path, name ) ) continue;

		l = g_list_append ( l, g_strdup ( name ) );
	}

	if ( found ) { g_list_free_full ( l, (GDestroyNotify) g_free ); return; }

	n = 0;
	gmf_bar_button_set_tab ( path, n, bar );

	while ( l != NULL )
	{
		if ( ++n < MAX_TAB ) gmf_bar_button_set_tab ( (char *)l->data, n, bar );

		l = l->next;
	}

	g_list_free_full ( l, (GDestroyNotify) g_free );
}

static void gmf_bar_clicked_button_tab ( GtkButton *button, GmfBar *bar )
{
	const char *name  = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	g_signal_emit_by_name ( bar, "bar-act-tab", name );

	gtk_widget_set_visible ( GTK_WIDGET ( bar->popover_tab ), FALSE );
}

static GtkPopover * gmf_bar_pp_tab ( GmfBar *bar )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	uint8_t n = 0; for ( n = 0; n < MAX_TAB; n++ )
	{
		bar->button_tab[n] = (GtkButton *)gtk_button_new_with_label ( "none" );
		g_signal_connect ( bar->button_tab[n], "clicked", G_CALLBACK ( gmf_bar_clicked_button_tab ), bar );

		gtk_button_set_relief ( bar->button_tab[n], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( bar->button_tab[n] ), TRUE );

		gtk_box_pack_start ( h_box, GTK_WIDGET ( bar->button_tab[n] ), TRUE, TRUE, 0 );
	}

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( h_box ) );
	gtk_widget_set_visible ( GTK_WIDGET ( h_box ), TRUE );

	return popover;
}

static void gmf_bar_clic_tab ( GtkButton *button, GmfBar *bar )
{
	gtk_popover_set_relative_to ( bar->popover_tab, GTK_WIDGET ( button ) );
	gtk_popover_popup ( bar->popover_tab );
}

static void gmf_bar_clic_act ( GtkButton *button, GmfBar *bar )
{
	gtk_popover_set_relative_to ( bar->popover_act, GTK_WIDGET ( button ) );
	gtk_popover_popup ( bar->popover_act );
}

static void gmf_bar_popover_jump ( GtkEntry *entry, GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, GmfBar *bar )
{
	const char *text = gtk_entry_get_text ( entry );

	if ( icon_pos == GTK_ENTRY_ICON_PRIMARY )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( entry ) ) );

		g_autofree char *path = gmf_open_dir ( text, window );

		if ( path == NULL ) return;

		gtk_entry_set_text ( entry, path );
	}

	if ( icon_pos == GTK_ENTRY_ICON_SECONDARY )
	{
		g_signal_emit_by_name ( bar, "bar-click-num", BT_JMP, text );
	}
}

static void gmf_bar_popover_jump_activate ( GtkEntry *entry, GmfBar *bar )
{
	const char *text = gtk_entry_get_text ( entry );

	g_signal_emit_by_name ( bar, "bar-click-num", BT_JMP, text );
}

static void gmf_bar_popover_search ( GtkEntry *entry, G_GNUC_UNUSED GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEventButton *event, GmfBar *bar )
{
	const char *text = gtk_entry_get_text ( entry );

	g_signal_emit_by_name ( bar, "bar-click-num", BT_SRH, text );
}

static void gmf_bar_popover_search_activate ( GtkEntry *entry, GmfBar *bar )
{
	const char *text = gtk_entry_get_text ( entry );

	g_signal_emit_by_name ( bar, "bar-click-num", BT_SRH, text );
}

static GtkPopover * gmf_bar_popover_js ( const char *icon_name, const char *text, void ( *fi )(), void ( *fa )(), GmfBar *bar )
{
	GtkPopover *popover = (GtkPopover *)gtk_popover_new ( NULL );

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_text ( entry, ( text ) ? text : "" );
	if ( g_str_has_suffix ( icon_name, "jump" ) ) gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_PRIMARY, "folder" );
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, icon_name );
	g_signal_connect ( entry, "activate",   G_CALLBACK ( fa ), bar );
	g_signal_connect ( entry, "icon-press", G_CALLBACK ( fi ), bar );

	gtk_container_add ( GTK_CONTAINER ( popover ), GTK_WIDGET ( entry ) );
	gtk_widget_set_visible ( GTK_WIDGET ( entry ), TRUE );

	return popover;
}

static void gmf_bar_clic_jmp ( GtkButton *button, GmfBar *bar )
{
	gtk_popover_set_relative_to ( bar->popover_jump, GTK_WIDGET ( button ) );
	gtk_popover_popup ( bar->popover_jump );
}

static void gmf_bar_clic_srh ( GtkButton *button, GmfBar *bar )
{
	gtk_popover_set_relative_to ( bar->popover_search, GTK_WIDGET ( button ) );
	gtk_popover_popup ( bar->popover_search );
}

static void gmf_bar_clic_prf ( GtkButton *button, GmfBar *bar )
{
	g_signal_emit_by_name ( bar, "bar-act-prf", button );
}

static void gmf_bar_signal_handler_num ( GtkButton *button, GmfBar *bar )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	if ( num == BT_TAB ) gmf_bar_clic_tab ( button, bar );
	if ( num == BT_ACT ) gmf_bar_clic_act ( button, bar );
	if ( num == BT_JMP ) gmf_bar_clic_jmp ( button, bar );
	if ( num == BT_SRH ) gmf_bar_clic_srh ( button, bar );
	if ( num == BT_PRF ) gmf_bar_clic_prf ( button, bar );

	if ( num == BT_TAB || num == BT_PRF || num == BT_JMP || num == BT_SRH ) return;

	g_signal_emit_by_name ( bar, "bar-click-num", num, b_icons[num] );
}

static void gmf_bar_create ( GmfBar *bar )
{
	enum b_num n = BT_WIN;

	for ( n = BT_WIN; n < BT_ALL; n++ )
	{
		bar->button[n] = button_set_image ( b_icons[n], 20 );

		char buf[10];
		sprintf ( buf, "%u", n );
		gtk_widget_set_name ( GTK_WIDGET ( bar->button[n] ), buf );

		g_signal_connect ( bar->button[n], "clicked", G_CALLBACK ( gmf_bar_signal_handler_num ), bar );

		gtk_button_set_relief ( bar->button[n], GTK_RELIEF_NONE );
		gtk_widget_set_visible ( GTK_WIDGET ( bar->button[n] ), TRUE );

		gtk_box_pack_start ( GTK_BOX ( bar ), GTK_WIDGET ( bar->button[n] ), TRUE, TRUE, 0 );
	}
}

static void gmf_bar_init ( GmfBar *bar )
{
	GtkBox *box = GTK_BOX ( bar );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_HORIZONTAL );

	gtk_widget_set_visible ( GTK_WIDGET ( box ), TRUE );

	bar->popover_act = gmf_bar_pp_act ( bar );
	bar->popover_tab = gmf_bar_pp_tab ( bar );

	bar->popover_jump   = gmf_bar_popover_js ( "go-jump", g_get_home_dir (), gmf_bar_popover_jump,   gmf_bar_popover_jump_activate,   bar );
	bar->popover_search = gmf_bar_popover_js ( "system-search", NULL,        gmf_bar_popover_search, gmf_bar_popover_search_activate, bar );

	gmf_bar_create ( bar );

	g_signal_connect ( bar, "bar-run-act", G_CALLBACK ( gmf_bar_button_handler_run_act ), NULL );
	g_signal_connect ( bar, "bar-set-act", G_CALLBACK ( gmf_bar_button_handler_set_act ), NULL );
	g_signal_connect ( bar, "bar-set-tab", G_CALLBACK ( gmf_bar_button_handler_set_tab ), NULL );
}

static void gmf_bar_finalize ( GObject *object )
{
	G_OBJECT_CLASS (gmf_bar_parent_class)->finalize (object);
}

static void gmf_bar_class_init ( GmfBarClass *class )
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = gmf_bar_finalize;

	g_signal_new ( "bar-run-act", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 0 );

	g_signal_new ( "bar-set-act", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT );

	g_signal_new ( "bar-set-tab", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "bar-act-tab", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "bar-act-prf", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_OBJECT );

	g_signal_new ( "bar-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );

	g_signal_new ( "bar-pp-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );
}

GmfBar * gmf_bar_new ( void )
{
	return g_object_new ( GMF_TYPE_BAR, NULL );
}

