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


#ifndef BLITZMAX_H
#define BLITZMAX_H 1

#ifdef HAVE_CONFIG_H
    #include "config.h" /* for the gettext domain */
#endif

#include <geanyplugin.h>
GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

//base declarations
static void onStartupComplete(GObject *obj, gpointer data);
static void onDocumentActivate(GObject *obj, GeanyDocument *doc, gpointer data);
static void onConfigureResponse(GtkDialog *dialog, gint response, gpointer user_data);
static void onToolbarCompileModesBuildDropdownChanged(GtkComboBox *box, gpointer data);
static void onToolbarCompilerDropdownChanged(GtkComboBox *box, gpointer data);
static void onToolbarCompileModesTargetDropdownChanged(GtkComboBox *box, gpointer data);
static void onToolbarCompileButtonClicked(GtkWidget *button, gpointer data);
static void onToolbarCompileAndRunButtonClicked(GtkWidget *button, gpointer data);

static void saveConfiguration();
static void changeBuildCommand();
static void executeBuildCommand(gint doRun);

void hideToolbarElements();
void showToolbarElements();

void hideToolbarCompilerElements();
void showToolbarCompilerElements();

void hideToolbarProjectElements();
void showToolbarProjectElements();

void hideToolbarNGElements();
void showToolbarNGElements();


#endif
