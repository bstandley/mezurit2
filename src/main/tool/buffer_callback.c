/*
 *  Copyright (C) 2012 California Institute of Technology
 *
 *  This file is part of Mezurit2, written by Brian Standley <brian@brianstandley.com>.
 *
 *  Mezurit2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or (at your option) any later version.
 *
 *  Mezurit2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this
 *  program. If not, see <http://www.gnu.org/licenses/>.
*/

static gboolean buffer_expose_cb (GtkWidget *widget, void *ptr, Buffer *buffer);
static void tzero_cb (GtkWidget *widget, Buffer *buffer);
static void add_set_cb (GtkWidget *widget, Buffer *buffer, ChanSet *chanset);
static gboolean filename_cb (GtkWidget *widget, GdkEvent *event, Buffer *buffer);
static void choose_file_cb (GtkWidget *widget, Buffer *buffer);
static void save_cb (GtkWidget *widget, Buffer *buffer);

static char * tzero_csf (gchar **argv, Buffer *buffer);
static char * add_set_csf (gchar **argv, Buffer *buffer, ChanSet *chanset);
static char * save_csf (gchar **argv, Buffer *buffer);
static char * get_time_csf (gchar **argv, Buffer *buffer);

gboolean buffer_expose_cb (GtkWidget *widget, void *ptr, Buffer *buffer)
{
	if (buffer->adjusted) return 0;
	f_start(F_CALLBACK);

	GtkAllocation alloc;
	gtk_widget_get_allocation(widget, &alloc);

	gtk_widget_set_size_request(buffer->tzero_button, -1, alloc.height - 4);
	gtk_widget_set_size_request(buffer->clear_button, -1, alloc.height - 4);
	gtk_widget_set_size_request(buffer->add_button,   -1, alloc.height - 4);
	gtk_widget_set_size_request(buffer->file_button,  -1, alloc.height - 4);
	gtk_widget_set_size_request(buffer->save_button,  -1, alloc.height - 4);

	buffer->adjusted = 1;
	return 0;
}

void tzero_cb (GtkWidget *widget, Buffer *buffer)
{
	f_start(F_CALLBACK);

	g_static_mutex_lock(&buffer->mutex);
	buffer->do_time_reset = 1;
	g_static_mutex_unlock(&buffer->mutex);
}

char * tzero_csf (gchar **argv, Buffer *buffer)
{
	f_start(F_CONTROL);

	g_static_mutex_lock(&buffer->mutex);
	buffer->do_time_reset = 1;
	g_static_mutex_unlock(&buffer->mutex);

	return cat1(argv[0]);
}

void add_set_cb (GtkWidget *widget, Buffer *buffer, ChanSet *chanset)
{
	f_start(F_CALLBACK);

	g_static_mutex_lock(&buffer->mutex);
	add_set(buffer, chanset);
	g_static_mutex_unlock(&buffer->mutex);
}

char * add_set_csf (gchar **argv, Buffer *buffer, ChanSet *chanset)
{
	f_start(F_CONTROL);

	g_static_mutex_lock(&buffer->mutex);
	bool success = add_set(buffer, chanset);
	g_static_mutex_unlock(&buffer->mutex);

	return supercat("%s;set_added|%d", argv[0], success ? 1 : 0);
}

gboolean filename_cb (GtkWidget *widget, GdkEvent *event, Buffer *buffer)
{
	if (entry_update_required(event, widget))
	{
		f_start(F_CALLBACK);

		replace(buffer->filename, cat1(gtk_entry_get_text(GTK_ENTRY(widget))));
	}

	return 0;
}
	
void choose_file_cb (GtkWidget *widget, Buffer *buffer)
{
	f_start(F_CALLBACK);

	char *filename = run_file_chooser("Specify save filename", GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_OK, buffer->filename);
	if (filename != NULL)
	{
		replace(buffer->filename, filename);

		gtk_entry_set_text(GTK_ENTRY(buffer->file_entry), filename);
		gtk_editable_set_position(GTK_EDITABLE(buffer->file_entry), -1);
	}
}

void save_cb (GtkWidget *widget, Buffer *buffer)
{
	f_start(F_CALLBACK);

	char *dirname _strfree_ = extract_dir(buffer->filename);
	if (str_length(buffer->filename) > 0 && dirname != NULL)
	{
		if (!g_file_test(buffer->filename, G_FILE_TEST_EXISTS) || run_yes_no_dialog("File exists.\nOverwrite?"))
			save_buffer(buffer, buffer->filename, 0, 1);
	}
	else if (str_length(buffer->filename) == 0 || run_yes_no_dialog("The specified filename is unusable.\nDo you want to choose a new filename now?"))
	{
		char *filename = run_file_chooser("Save data", GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_SAVE, buffer->filename);

		if (filename != NULL)
		{
			save_buffer(buffer, filename, 0, 1);
			replace(buffer->filename, filename);

			gtk_entry_set_text(GTK_ENTRY(buffer->file_entry), filename);
			gtk_editable_set_position(GTK_EDITABLE(buffer->file_entry), -1);
		}
	}
}

char * save_csf (gchar **argv, Buffer *buffer)
{
	f_start(F_CONTROL);

	char *filename _strfree_ = NULL;
	if (scan_arg_string(argv[1], "filename", &filename))
	{
		long N_written = save_buffer(buffer, filename, 1, 0);

		return supercat("%s;points_written|%ld", argv[0], N_written);  // reply now
	}
	else return cat1("argument_error");
}

char * get_time_csf (gchar **argv, Buffer *buffer)
{
	f_start(F_CONTROL);

	return supercat("%s;time|%f", argv[0], timer_elapsed(buffer->timer));  // reply now
}

