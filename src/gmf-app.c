/*
* Copyright 2021 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-app.h"
#include "gmf-win.h"

#include "copy-win.h"
#include "info-win.h"

struct _GmfApp
{
	GtkApplication  parent_instance;
};

G_DEFINE_TYPE ( GmfApp, gmf_app, GTK_TYPE_APPLICATION )

static int gmf_app_cmdline ( GApplication *app, GApplicationCommandLine *cmdline )
{
	int c = 0, i = 0, argc = 0;

	char **argv = g_application_command_line_get_arguments ( cmdline, &argc );

	char *uris[argc - 2];

	for ( i = 3; i < argc; i++ ) uris[c++] = argv[i];

	uris[c] = NULL;

	if ( g_str_has_prefix ( argv[1], "--dialog-cp" ) )
		copy_win_new ( CM_CP, ( argv[2] ) ? argv[2] : g_get_home_dir (), uris, app, 1.0 ); // Copy: destination - argv[2], source { argv[3] ... argv[n] }

	if ( g_str_has_prefix ( argv[1], "--dialog-mv" ) )
		copy_win_new ( CM_MV, ( argv[2] ) ? argv[2] : g_get_home_dir (), uris, app, 1.0 ); // Move: destination - argv[2], source { argv[3] ... argv[n] }

	if ( g_str_has_prefix ( argv[1], "--dialog-info" ) )
	{
		GList *list = NULL;

		if ( argc > 2 ) for ( i = 2; i < argc; i++ ) { list = g_list_append ( list, argv[i] ); }

		if ( argc == 2 )
			info_win_new ( I_SEL_0, g_get_home_dir (), NULL, app, 1.0 ); // test: home-dir
		if ( argc == 3 )
			info_win_new ( I_SEL_1, argv[2], list, app, 1.0 ); // ignored: list
		if ( argc > 3 )
			info_win_new ( I_SEL_N, argv[2], list, app, 1.0 ); // only list: list = argv[2] ... argv[n]

		g_list_free ( list );
	}

	g_strfreev (argv);

	return 0;
}

static void gmf_app_open ( GApplication *app, GFile **files, G_GNUC_UNUSED int n_files, G_GNUC_UNUSED const char *hint )
{
	g_autofree char *path = g_file_get_path ( files[0] );

	gmf_win_new ( path, GMF_APP ( app ) );
}

static void gmf_app_activate ( GApplication *app )
{
	gmf_win_new ( NULL, GMF_APP ( app ) );
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

	G_APPLICATION_CLASS (class)->command_line = gmf_app_cmdline;

	object_class->finalize = gmf_app_finalize;
}

GmfApp * gmf_app_new ( gboolean dialog )
{
	return g_object_new ( GMF_TYPE_APP, "application-id", "org.gtk.gmf", "flags", 
		( dialog ) ? G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_NON_UNIQUE : G_APPLICATION_HANDLES_OPEN, NULL );
}

