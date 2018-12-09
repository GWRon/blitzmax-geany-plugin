/*
 *      blitzmax.h - this file is part of "BlitzMax"-addon
 *
 *      Copyright 2009-2018 Ronny Otto
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */
#include "blitzmax.h"


GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(209)
PLUGIN_SET_TRANSLATABLE_INFO(
		LOCALEDIR, GETTEXT_PACKAGE,
		_("BlitzMax-Addon"),
		_("Various functions for the BlitzMax language in Geany."),
		"0.5",
		"Ronny Otto <ron(at)gamezworld(dot)de>"
	);


// Config vars - to store in config file
typedef struct {
	gchar *configFile;
	gchar *blitzmaxPath;
	gchar *blitzmaxNGPath;
	gint compilerModeTarget;
	gint compilerModeBuild;
	gint compiler;
	gint compilerTargetArch; //NG
} AddonConfig;
static AddonConfig *blitzConfig = NULL;



// store build options globally, so "run" can be set by button-events
gint optionRun			= 1;
gint optionQuickBuild	= 0;
gint optionDebug		= 0;
gint optionConsole		= 0;
gint optionRebuild		= 0;
gint optionThreaded		= 0;
gint optionTargetOS		= 0;
gint optionTargetArch	= 0;

static GtkToolItem *toolbar_compileBoxItem = NULL;
static GtkWidget *toolbar_compileBox = NULL;
static GtkWidget *toolbar_compileModesTarget_dropdown = NULL;
static GtkWidget *toolbar_compiler_dropdown = NULL;
static GtkWidget *toolbar_compileTargetArch_dropdown = NULL;
static GtkWidget *toolbar_compileModesBuild_dropdown = NULL;
static GtkToolItem *toolbar_compile_button = NULL;
static GtkToolItem *toolbar_compileAndRun_button = NULL;
static GtkWidget *blitzmaxPathEntry;
static GtkWidget *blitzmaxNGPathEntry;

//event handler implementations
//-----------------------------

//what to do if preferences are changed
static void onConfigureResponse(GtkDialog *dialog, gint response, gpointer user_data) {
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY) {
		g_free(blitzConfig->blitzmaxPath);
		blitzConfig->blitzmaxPath = g_strdup( gtk_entry_get_text( GTK_ENTRY(blitzmaxPathEntry) ) );

		g_free(blitzConfig->blitzmaxNGPath);
		blitzConfig->blitzmaxNGPath = g_strdup( gtk_entry_get_text( GTK_ENTRY(blitzmaxNGPathEntry) ) );
	}
	saveConfiguration();
}

static void onStartupComplete(GObject *obj, gpointer data) {
	// editor just started - check whether to show toolbar elements
	// - in case of show, it also configs the custom build command
	onDocumentActivate(obj, document_get_current(), data);
}

//everytime active document changes
static void onDocumentActivate(GObject *obj, GeanyDocument *doc, gpointer data) {
	//invalid call - return
	g_return_if_fail(doc != NULL && doc->is_valid);

	gchar *extension = doc->file_type->extension;

	if( g_ascii_strncasecmp(extension, "bmx",3) ==0 )
		showToolbarElements();
	else
		hideToolbarElements();
}

static void onToolbarCompileButtonClicked(GtkWidget *button, gpointer data) {
	optionRun=0;
	executeBuildCommand( 0 );
}
static void onToolbarCompileAndRunButtonClicked(GtkWidget *button, gpointer data) {
	optionRun=1;
	executeBuildCommand( 1 );
}

static void onToolbarCompileModesBuildDropdownChanged(GtkComboBox *box, gpointer data) {
	//store selection in info
	blitzConfig->compilerModeBuild = gtk_combo_box_get_active(box);
	changeBuildCommand();
	saveConfiguration();
}

static void onToolbarCompileModesTargetDropdownChanged(GtkComboBox *box, gpointer data) {
	//store selection in info
	blitzConfig->compilerModeTarget = gtk_combo_box_get_active(box);
	changeBuildCommand();
	saveConfiguration();
}

static void onToolbarCompilerDropdownChanged(GtkComboBox *box, gpointer data) {
	//store selection in info
	blitzConfig->compiler = gtk_combo_box_get_active(box);

	if (blitzConfig->compiler == 0)
		hideToolbarNGElements();
	else
		showToolbarNGElements();

	changeBuildCommand();
	saveConfiguration();
}

static void onToolbarTargetArchDropdownChanged(GtkComboBox *box, gpointer data) {
	//store selection in info
	blitzConfig->compilerTargetArch = gtk_combo_box_get_active(box);
	changeBuildCommand();
	saveConfiguration();
}




static void executeBuildCommand(gint doRun) {
	BuildMenuItems *menu_items;
	GeanyDocument *doc = document_get_current();
	GtkWidget *item;

	g_return_if_fail( doc != NULL );

	//store option whether we want to run the executable or not
	optionRun = doRun;
	//refresh build command
	changeBuildCommand();

	menu_items = build_get_menu_items(doc->file_type->id);
	//run "compile" (1st)
	//item = menu_items->menu_item[GEANY_GBG_FT][GBO_TO_CMD(GEANY_GBO_COMPILE)];
	//run "create" (2nd)
	item = menu_items->menu_item[GEANY_GBG_FT][GBO_TO_CMD(GEANY_GBO_BUILD)];
	if (item && GTK_WIDGET_IS_SENSITIVE(item))
		gtk_menu_item_activate(GTK_MENU_ITEM(item));

	//reset to default 1 - so that F9 compiles and executes
	optionRun = 1;

}

static void changeBuildCommand() {
	//g_strconcat awaits NULL terminated array -> last var is a NULL
	gchar *workingDir1 = g_strconcat(blitzConfig->blitzmaxPath, G_DIR_SEPARATOR_S, "bin", NULL);
	gchar *workingDir2 = g_strconcat(blitzConfig->blitzmaxNGPath, G_DIR_SEPARATOR_S, "bin", NULL);

	const gchar *compiler	= "./bmk";
	#ifdef G_OS_WIN32
		compiler			= "bmk.exe";
	#endif
	const gchar *command	=  "\"%d/%f\""; //quoted "%d/%f"

	optionDebug      = 0;
	optionRebuild    = 0;
	optionThreaded   = 0;
	optionConsole    = 0;
	optionTargetOS   = 0;
	optionQuickBuild = 0;
	optionTargetArch = 0;

	switch( blitzConfig->compilerTargetArch ){
		case 0:     optionTargetArch= 0;
					break;
		case 1:     optionTargetArch= 1;
					break;
	}

	switch( blitzConfig->compilerModeTarget ){
		//Windows Console
		case 1:		optionTargetOS= 0;
					optionConsole = 1;
					break;
		//Windows GUI
		case 2:		optionTargetOS= 0;
					optionConsole = 0;
					break;
		//Windows Console
		case 3:		optionTargetOS= 1;
					optionConsole = 1;
					break;
		//Windows GUI
		case 4:		optionTargetOS= 1;
					optionConsole = 0;
					break;
		//MacOS Console
		case 5:		optionTargetOS= 2;
					optionConsole = 1;
					break;
		//MacOS GUI
		case 6:		optionTargetOS= 2;
					optionConsole = 0;
					break;
	}
	switch( blitzConfig->compilerModeBuild ){
		//Release
		case 1:		break;
		//Release + Quick
		case 2:		optionQuickBuild=1;
					break;
		//Release + Threaded
		case 3:		optionThreaded=1;
					break;
		//Release + Threaded + Quick
		case 4:		optionThreaded=1;
					optionQuickBuild=1;
					break;
		//Release + Rebuild
		case 5:		optionRebuild=1;
					break;
		//Release + Rebuild + Quick
		case 6:		optionRebuild=1;
					optionQuickBuild=1;
					break;
		//Release + Rebuild + Threaded
		case 7:		optionRebuild=1;
					optionThreaded=1;
					break;
		//Release + Rebuild + Threaded + Quick
		case 8:		optionRebuild=1;
					optionThreaded=1;
					optionQuickBuild=1;
					break;
		//Debug
		case 9:		optionDebug=1;
					break;
		//Debug + Quick
		case 10:	optionDebug=1;
					optionQuickBuild=1;
					break;
		//Debug + Threaded
		case 11:	optionDebug=1;
					optionThreaded=1;
					break;
		//Debug + Threaded + Quick
		case 12:	optionDebug=1;
					optionThreaded=1;
					optionQuickBuild=1;
					break;
		//Debug + Rebuild
		case 13:	optionDebug=1;
					optionRebuild=1;
					break;
		//Debug + Rebuild + Quick
		case 14:	optionDebug=1;
					optionRebuild=1;
					optionQuickBuild=1;
					break;
		//Debug +Rebuild +Threaded
		case 15:	optionDebug=1;
					optionRebuild=1;
					optionThreaded=1;
					break;
		//Debug +Rebuild +Threaded + Quick
		case 16:	optionDebug=1;
					optionRebuild=1;
					optionThreaded=1;
					optionQuickBuild=1;
					break;
	}

	if( optionRun==1 )
		command = g_strconcat("-x ", command, NULL);

	if( optionDebug==1 )
		command = g_strconcat("-d ", command, NULL);
	else
		command = g_strconcat("-r ", command, NULL);

	if( optionThreaded==1 )
		command = g_strconcat("-h ", command, NULL);

	if( optionRebuild==1 )
		command = g_strconcat("-a ", command, NULL);

	if( optionQuickBuild==1 )
		command = g_strconcat("-quick ", command, NULL);

	if( optionConsole==1 )
		command = g_strconcat("-t console ", command, NULL);
	else
		command = g_strconcat("-t gui ", command, NULL);

	//NG
	if( blitzConfig->compiler == 1) {
		if( optionTargetArch == 0 )
			command = g_strconcat("-g x86 ", command, NULL);
		else
			command = g_strconcat("-g x64 ", command, NULL);
	}


	//we want to build an app, not a module
	command = g_strconcat("makeapp ", command, NULL);

	//append the custom params to the compiler base
	command = g_strconcat(compiler, " ", command, NULL);

	//1st entry = 0, 2nd entry = 1
	build_set_menu_item(GEANY_BCS_PROJ, GEANY_GBG_FT, 1, GEANY_BC_LABEL,		"Custom Build Command");
	build_set_menu_item(GEANY_BCS_PROJ, GEANY_GBG_FT, 1, GEANY_BC_COMMAND,		command);
	if (blitzConfig->compiler == 0)
		build_set_menu_item(GEANY_BCS_PROJ, GEANY_GBG_FT, 1, GEANY_BC_WORKING_DIR,	workingDir1);
	else
		build_set_menu_item(GEANY_BCS_PROJ, GEANY_GBG_FT, 1, GEANY_BC_WORKING_DIR,	workingDir2);
}

static void saveConfiguration() {
	//save to file
	GKeyFile *config = g_key_file_new();
	gchar *data;
	gchar *config_dir = g_path_get_dirname( blitzConfig->configFile );
	g_key_file_load_from_file( config, blitzConfig->configFile, G_KEY_FILE_NONE, NULL );

	g_key_file_set_string( config, "blitzmax", "blitzmaxPath", blitzConfig->blitzmaxPath );
	g_key_file_set_string( config, "blitzmax", "blitzmaxNGPath", blitzConfig->blitzmaxNGPath );
	g_key_file_set_integer( config, "blitzmax", "compilerModeBuild", blitzConfig->compilerModeBuild );
	g_key_file_set_integer( config, "blitzmax", "compilerModeTarget", blitzConfig->compilerModeTarget );
	g_key_file_set_integer( config, "blitzmax", "compilerTargetArch", blitzConfig->compilerTargetArch );
	g_key_file_set_integer( config, "blitzmax", "compiler", blitzConfig->compiler );

	if ( !g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0) {
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Plugin configuration directory could not be created."));
	}else{
		//save file to disc
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(blitzConfig->configFile, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);

}

void hideToolbarElements() {
	//returnf if not existing
	g_return_if_fail( toolbar_compile_button != NULL );

	gtk_widget_show( toolbar_get_widget_by_name("Build") );
	gtk_widget_show( toolbar_get_widget_by_name("Compile") );
	gtk_widget_show( toolbar_get_widget_by_name("Run") );

//	gtk_box_reorder_child ( GTK_BOX(geany->main_widgets->toolbar), GTK_WIDGET(toolbar_compileBoxItem), 0);

	gtk_widget_hide_all( GTK_WIDGET(toolbar_compileBoxItem) );
}

void showToolbarElements() {
	//returnf if not existing
	g_return_if_fail( toolbar_compile_button != NULL );

	gtk_widget_show_all( GTK_WIDGET(toolbar_compileBoxItem) );

	//hide NG stuff if not needed
	if(g_strcmp0(blitzConfig->blitzmaxNGPath, "") == 0)
		hideToolbarCompilerElements();
	if (blitzConfig->compiler == 0)
		hideToolbarNGElements();

	//hide the default thingies
	gtk_widget_hide( toolbar_get_widget_by_name("Build") );
	gtk_widget_hide( toolbar_get_widget_by_name("Compile") );
	gtk_widget_hide( toolbar_get_widget_by_name("Run") );

	//build up our custom build command
	//- do this on show in case of editor startup and eg. usage of
	//  shortcut F9 (build)
	//  -> in case current doc is a "bmx"-file, we show elements...
	//     and voila: have a custom build command set
	changeBuildCommand();
}

void hideToolbarCompilerElements() {
	g_return_if_fail( toolbar_compiler_dropdown != NULL );

	gtk_widget_hide( toolbar_compiler_dropdown );
}

void showToolbarCompilerElements() {
	g_return_if_fail( toolbar_compiler_dropdown != NULL );

	if(g_strcmp0(blitzConfig->blitzmaxNGPath, "") == 0)
		return;

	gtk_widget_show( toolbar_compiler_dropdown );
}

void hideToolbarNGElements() {
	g_return_if_fail( toolbar_compileTargetArch_dropdown != NULL );

	gtk_widget_hide( toolbar_compileTargetArch_dropdown );
}

void showToolbarNGElements() {
	g_return_if_fail( toolbar_compileTargetArch_dropdown != NULL );

	if(g_strcmp0(blitzConfig->blitzmaxNGPath, "") == 0)
		return;

	gtk_widget_show( toolbar_compileTargetArch_dropdown );
}

void initToolbarElements(GeanyData *data) {
	// use an tool item to store a box into it
	// that box is the container for all our elements...



	// Toolbar container
	//------------------
	toolbar_compileBoxItem = gtk_tool_item_new();
	toolbar_compileBox = gtk_hbox_new(FALSE, 1); //homogenous=false, spacing
	//add box to box item (only items can be stored in the toolbar)
	//-> dropdowns would have to get stored in items first too
	gtk_container_add( GTK_CONTAINER(toolbar_compileBoxItem), GTK_WIDGET(toolbar_compileBox) );
	//add box item to toolbar
	//plugin_add_toolbar_item( geany_plugin, toolbar_compileBoxItem );
	//do not rely on plugin_add_toolbar_item -> we want to place it between the default icons
	gtk_toolbar_insert(GTK_TOOLBAR(geany->main_widgets->toolbar), toolbar_compileBoxItem, 6);



	// Compile button
	//---------------
	toolbar_compile_button = gtk_tool_button_new_from_stock( GTK_STOCK_CONVERT );
	#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_tool_item_set_tooltip_text( GTK_TOOL_ITEM(toolbar_compile_button),	_("Compile") );
	#endif
	ui_add_document_sensitive( GTK_WIDGET(toolbar_compile_button) );
	g_signal_connect( toolbar_compile_button, "clicked", G_CALLBACK(onToolbarCompileButtonClicked), NULL );



	// Compile and Run button
	//---------------
	toolbar_compileAndRun_button = gtk_tool_button_new_from_stock( GTK_STOCK_EXECUTE );
	#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_tool_item_set_tooltip_text( GTK_TOOL_ITEM(toolbar_compileAndRun_button),	_("Compile and Run") );
	#endif
	ui_add_document_sensitive( GTK_WIDGET(toolbar_compileAndRun_button) );
	g_signal_connect( toolbar_compileAndRun_button, "clicked", G_CALLBACK(onToolbarCompileAndRunButtonClicked), NULL );



	// Compilation Modes Target
	// ------------------------
	toolbar_compileModesTarget_dropdown = gtk_combo_box_new_text();
	// add dropdown texts
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "Select Target");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "Windows Console");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "Windows GUI");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "Linux Console");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "Linux GUI");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "MacOS Console");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), "MacOS GUI");
	// set active to stored one (defaults to "Select...")
	gtk_combo_box_set_active( GTK_COMBO_BOX(toolbar_compileModesTarget_dropdown), blitzConfig->compilerModeTarget );
	// connect to "change" event - if user changes selection
	g_signal_connect(G_OBJECT(toolbar_compileModesTarget_dropdown), "changed", G_CALLBACK( onToolbarCompileModesTargetDropdownChanged), data);


	// Compiler
	// ------------------------
	toolbar_compiler_dropdown = gtk_combo_box_new_text();
	// add dropdown texts
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compiler_dropdown), "Vanilla");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compiler_dropdown), "NG");
	// set active to stored one (defaults to "Vanilla")
	gtk_combo_box_set_active( GTK_COMBO_BOX(toolbar_compiler_dropdown), blitzConfig->compiler );
	// connect to "change" event - if user changes selection
	g_signal_connect(G_OBJECT(toolbar_compiler_dropdown), "changed", G_CALLBACK( onToolbarCompilerDropdownChanged), data);


	// Architecure
	// ------------------------
	toolbar_compileTargetArch_dropdown = gtk_combo_box_new_text();
	// add dropdown texts
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileTargetArch_dropdown), "x86 (32 bit)");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileTargetArch_dropdown), "x64 (64 bit)");
	// set active to stored one (defaults to "Vanilla")
	gtk_combo_box_set_active( GTK_COMBO_BOX(toolbar_compileTargetArch_dropdown), blitzConfig->compilerTargetArch );
	// connect to "change" event - if user changes selection
	g_signal_connect(G_OBJECT(toolbar_compileTargetArch_dropdown), "changed", G_CALLBACK( onToolbarTargetArchDropdownChanged), data);



	// Compilation Modes Build
	// -----------------------
	toolbar_compileModesBuild_dropdown = gtk_combo_box_new_text();
	// add dropdown texts
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Select Mode");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Threaded");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Threaded +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Rebuild");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Rebuild +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Rebuild +Threaded");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Release +Rebuild +Threaded +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Threaded");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Threaded +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Rebuild");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Rebuild +Quick");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Rebuild +Threaded");
	gtk_combo_box_append_text( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), "Debug +Rebuild +Threaded +Quick");
	// set active to stored one (defaults to "Select...")
	gtk_combo_box_set_active( GTK_COMBO_BOX(toolbar_compileModesBuild_dropdown), blitzConfig->compilerModeBuild );
	// connect to "change" event - if user changes selection
	g_signal_connect(G_OBJECT(toolbar_compileModesBuild_dropdown), "changed", G_CALLBACK( onToolbarCompileModesBuildDropdownChanged), data);


	//add them to the container
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compileModesTarget_dropdown), TRUE, TRUE, 0);

	//instead of dropdown, one could also like checkboxes
	// - remember to include their events too (and save/read config-values)
	//gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(gtk_check_button_new_with_label("Debug")), TRUE, TRUE, 0);
	//gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(gtk_check_button_new_with_label("Quick")), TRUE, TRUE, 0);
	//gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(gtk_check_button_new_with_label("Threaded")), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compileModesBuild_dropdown), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compiler_dropdown), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compileTargetArch_dropdown), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compile_button), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(toolbar_compileBox), GTK_WIDGET(toolbar_compileAndRun_button), TRUE, TRUE, 0);


}



// events of geany (not normal GTK events)
PluginCallback plugin_callbacks[] = {
	{ "document-activate", (GCallback) &onDocumentActivate, TRUE, NULL },
	{ "geany-startup-complete", (GCallback) &onStartupComplete, TRUE, NULL },
	//has to end with a null entry
	{ NULL, NULL, FALSE, NULL }
};


void plugin_init(GeanyData *data) {
	// "init"
	GKeyFile *config = g_key_file_new();
	blitzConfig = g_new0(AddonConfig, 1);

	// set config file path
	blitzConfig->configFile = g_strconcat(	geany->app->configdir, G_DIR_SEPARATOR_S,
											"plugins", G_DIR_SEPARATOR_S,
											"blitzmax", G_DIR_SEPARATOR_S,
											"blitzmax.conf", NULL );
	// load config file
	g_key_file_load_from_file(config, blitzConfig->configFile, G_KEY_FILE_NONE, NULL);
	// load config settings
	blitzConfig->blitzmaxPath		= utils_get_setting_string(config, "blitzmax", "blitzmaxPath", "");
	blitzConfig->blitzmaxNGPath		= utils_get_setting_string(config, "blitzmax", "blitzmaxNGPath", "");
	blitzConfig->compilerModeBuild	= utils_get_setting_integer(config, "blitzmax", "compilerModeBuild", 0);
	blitzConfig->compilerModeTarget	= utils_get_setting_integer(config, "blitzmax", "compilerModeTarget", 0);
	blitzConfig->compiler			= utils_get_setting_integer(config, "blitzmax", "compiler", 0);
	blitzConfig->compilerTargetArch	= utils_get_setting_integer(config, "blitzmax", "compilerTargetArch", 0);

	if(g_strcmp0(blitzConfig->blitzmaxNGPath, "") == 0)
		blitzConfig->compiler = 0;

	// we want to store our config
	plugin_module_make_resident(geany_plugin);

	// set up toolbar elements
	initToolbarElements( data );
	// hide them as default
	hideToolbarElements();

	// give config free ...
	g_key_file_free(config);
}

// if one tries to configure the plugin - spend him a nice dialog
GtkWidget *plugin_configure(GtkDialog *dialog) {
	GtkWidget *label, *vbox, *box;

	vbox = gtk_vbox_new(FALSE, 6);
	box = gtk_vbox_new(FALSE, 3);

	label = gtk_label_new(_("Path to BlitzMax:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	blitzmaxPathEntry = gtk_entry_new();
	if ( blitzConfig->blitzmaxPath != NULL)
		gtk_entry_set_text(GTK_ENTRY(blitzmaxPathEntry), blitzConfig->blitzmaxPath);
	gtk_widget_set_tooltip_text(blitzmaxPathEntry, _("The absolute path to your BlitzMax directory - do not add the /bin-subdirectory here"));

	gtk_box_pack_start(GTK_BOX(box), blitzmaxPathEntry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 3);

	label = gtk_label_new(_("NO trailing slash\neg. home/ronny/Work/Programming/BlitzMax"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	//

	box = gtk_vbox_new(FALSE, 3);
	label = gtk_label_new(_("Path to BlitzMaxNG:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	blitzmaxNGPathEntry = gtk_entry_new();
	if ( blitzConfig->blitzmaxNGPath != NULL)
		gtk_entry_set_text(GTK_ENTRY(blitzmaxNGPathEntry), blitzConfig->blitzmaxNGPath);
	gtk_widget_set_tooltip_text(blitzmaxNGPathEntry, _("The absolute path to your BlitzMaxNG directory - do not add the /bin-subdirectory here"));

	gtk_box_pack_start(GTK_BOX(box), blitzmaxNGPathEntry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 3);

	label = gtk_label_new(_("NO trailing slash\neg. home/ronny/Work/Programming/BlitzMaxNG"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);


	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(onConfigureResponse), NULL);
	return vbox;
}





void plugin_cleanup(void) {
    saveConfiguration();

	//destroys compileBox and children
    gtk_widget_destroy(GTK_WIDGET(toolbar_compileBox));

}
