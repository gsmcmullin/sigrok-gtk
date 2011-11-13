/*
 * This file is part of the sigrok project.
 *
 * Copyright (C) 2011 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sigrok.h>
#include <gtk/gtk.h>
#include "sigrok-gtk.h"

GtkWidget *sigview;

static const char *colours[8] = {
	"black", "brown", "red", "orange",
	"gold", "darkgreen", "blue", "magenta",
};

static void
datafeed_in(struct sr_device *device, struct sr_datafeed_packet *packet)
{
	static int probelist[65] = { 0 };
	static int unitsize = 0;
	struct sr_probe *probe;
	struct sr_datafeed_header *header;
	struct sr_datafeed_logic *logic = NULL;
	int num_enabled_probes, sample_size, i;
	uint64_t filter_out_len;
	char *filter_out;
	GArray *data;

	switch (packet->type) {
	case SR_DF_HEADER:
		g_message("cli: Received SR_DF_HEADER");
		header = packet->payload;
		num_enabled_probes = 0;
		gtk_list_store_clear(siglist);
		for (i = 0; i < header->num_logic_probes; i++) {
			probe = g_slist_nth_data(device->probes, i);
			if (probe->enabled) {
				GtkTreeIter iter;
				probelist[num_enabled_probes++] = probe->index;
				gtk_list_store_append(siglist, &iter);
				gtk_list_store_set(siglist, &iter,
						0, probe->name,
						1, colours[(num_enabled_probes - 1) & 7],
						2, num_enabled_probes - 1,
						-1);
			}
		}
		/* How many bytes we need to store num_enabled_probes bits */
		unitsize = (num_enabled_probes + 7) / 8;
		data = g_array_new(FALSE, FALSE, unitsize);
		g_object_set_data(G_OBJECT(siglist), "sampledata", data);

		break;
	case SR_DF_END:
		sigview_zoom(sigview, 1, 0);
		g_message("cli: Received SR_DF_END");
		sr_session_halt();
		break;
	case SR_DF_TRIGGER:
		g_message("cli: received SR_DF_TRIGGER at %"PRIu64" ms",
				packet->timeoffset / 1000000);
		break;
	case SR_DF_LOGIC:
		logic = packet->payload;
		sample_size = logic->unitsize;
		g_message("cli: received SR_DF_LOGIC at %f ms duration %f ms, %"PRIu64" bytes",
				packet->timeoffset / 1000000.0, packet->duration / 1000000.0,
				logic->length);
		break;
	case SR_DF_ANALOG:
		break;
	}

	if (!logic)
		return;

	if (sr_filter_probes(sample_size, unitsize, probelist,
				   logic->data, logic->length,
				   &filter_out, &filter_out_len) != SR_OK)
		return;

	data = g_object_get_data(G_OBJECT(siglist), "sampledata");
	g_return_if_fail(data != NULL);

	g_array_append_vals(data, filter_out, filter_out_len);
}

int main(int argc, char **argv)
{
	GtkWindow *window;
	GtkWidget *vbox, *vpaned, *log;
	gtk_init(&argc, &argv);
	sr_init();
	sr_session_new();
	sr_session_datafeed_callback_add(datafeed_in);

	window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_title(window, "Sigrok-GTK");
	gtk_window_set_default_size(window, 600, 400);

	g_signal_connect(window, "destroy", gtk_main_quit, NULL);

	vbox = gtk_vbox_new(FALSE, 0);
	vpaned = gtk_vpaned_new();

	gtk_box_pack_start(GTK_BOX(vbox), toolbar_init(window), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vpaned, TRUE, TRUE, 0);
	log = log_init();
	gtk_widget_set_no_show_all(log, TRUE);
	g_object_set_data(G_OBJECT(window), "logview", log);
	gtk_paned_add2(GTK_PANED(vpaned), log);
	sigview = sigview_init();
	g_object_set_data(G_OBJECT(window), "sigview", sigview);
	gtk_paned_pack1(GTK_PANED(vpaned), sigview, TRUE, FALSE);

	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show_all(GTK_WIDGET(window));

	sr_set_loglevel(5);

	gtk_main();

	sr_session_destroy();
	gtk_exit(0);

	return 0;
}

