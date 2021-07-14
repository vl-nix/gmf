/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-bar.h"

struct _GmfBar
{
	GtkBox parent_instance;

	GtkButton *button[BT_ALL];
	GtkButton *button_pp[BTP_ALL];

	GtkPopover *popover_tab;
	GtkButton *button_tab[MAX_TAB];
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
	[BT_INF] = "gtk-info",
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
			: (GtkButton *)gtk_button_new_from_icon_name ( bp_icons[n], GTK_ICON_SIZE_MENU );

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

static void gmf_bar_clic_act ( GtkButton *button, GmfBar *bar )
{
	gtk_popover_set_relative_to ( bar->popover_tab, GTK_WIDGET ( button ) );
	gtk_popover_popup ( bar->popover_tab );
}

static void gmf_bar_signal_handler_num ( GtkButton *button, GmfBar *bar )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	uint8_t num = ( uint8_t )( atoi ( name ) );

	if ( num == BT_ACT ) gmf_bar_clic_act ( button, bar );

	g_signal_emit_by_name ( bar, "bar-click-num", num, b_icons[num] );
}

static void gmf_bar_create ( GmfBar *bar )
{
	enum b_num n = BT_WIN;

	for ( n = BT_WIN; n < BT_ALL; n++ )
	{
		bar->button[n] = (GtkButton *)gtk_button_new_from_icon_name ( b_icons[n], GTK_ICON_SIZE_MENU );

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

	bar->popover_tab = gmf_bar_pp_act ( bar );

	gmf_bar_create ( bar );

	g_signal_connect ( bar, "bar-run-act", G_CALLBACK ( gmf_bar_button_handler_run_act ), NULL );
	g_signal_connect ( bar, "bar-set-act", G_CALLBACK ( gmf_bar_button_handler_set_act ), NULL );
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

	g_signal_new ( "bar-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );

	g_signal_new ( "bar-pp-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_LAST,
		0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING );
}

GmfBar * gmf_bar_new ( void )
{
	return g_object_new ( GMF_TYPE_BAR, NULL );
}

