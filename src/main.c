/*
* Copyright 2022 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "gmf-app.h"

int main ( int argc, char *argv[] )
{
	GmfApp *app = gmf_app_new ();

	int status = g_application_run ( G_APPLICATION ( app ), argc, argv );

	g_object_unref ( app );

	return status;
}
