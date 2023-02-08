/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-app.h"
#include "gmf-win.h"

struct _GmfApp
{
	GtkApplication  parent_instance;
};

G_DEFINE_TYPE ( GmfApp, gmf_app, GTK_TYPE_APPLICATION )

static void gmf_app_activate ( GApplication *app )
{
	gmf_win_new ( NULL, GMF_APP ( app ) );
}

static void gmf_app_open ( GApplication *app, GFile **files, G_GNUC_UNUSED int n_files, G_GNUC_UNUSED const char *hint )
{
	gmf_win_new ( files[0], GMF_APP ( app ) );
}

static void gmf_app_init ( G_GNUC_UNUSED GmfApp *app )
{

}

static void gmf_app_finalize ( GObject *object )
{
	G_OBJECT_CLASS (gmf_app_parent_class)->finalize (object);
}

static void gmf_app_class_init ( GmfAppClass *class )
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	G_APPLICATION_CLASS (class)->open     = gmf_app_open;
	G_APPLICATION_CLASS (class)->activate = gmf_app_activate;

	object_class->finalize = gmf_app_finalize;
}

GmfApp * gmf_app_new ( void )
{
	return g_object_new ( GMF_TYPE_APP, "application-id", "org.gtk.gmf", "flags", G_APPLICATION_HANDLES_OPEN | G_APPLICATION_NON_UNIQUE, NULL );
}
