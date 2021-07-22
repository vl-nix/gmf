/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

#define MAX_TAB 8

enum b_num 
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

enum b_pp_num
{
	BTP_NDR,
	BTP_NFL,
	BTP_PST,
	BTP_HDN,
	BTP_TRM,
	BTP_EDT,
	BTP_CPY,
	BTP_CUT,
	BTP_RMV,
	BTP_ARC,
	BTP_RUN,
	BTP_ALL
};

#define GMF_TYPE_BAR gmf_bar_get_type ()

G_DECLARE_FINAL_TYPE ( GmfBar, gmf_bar, GMF, BAR, GtkBox )

GmfBar * gmf_bar_new ( void );

