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

static void draw_axis (cairo_t *cr, Axis *axis, VSP vs, Region r, ColorScheme *colorscheme, double *lw0, double *lw1, double *lw_max);
static void plot_axis_points (Axis *axis_array, VSP vs, Region r, long begin, long end);
static void draw_marker (cairo_t *cr, Axis *axis_array, Region r, ColorScheme *colorscheme, double XM, double YM);

static void draw_buffer_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme);
static void draw_scope_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme);
static void erase_scope_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme);
static void draw_buffer_display (Display *display, Region r, ColorScheme *colorscheme);

static int  detect_region (double X, double Y, Region r);
static void flip_surface (GtkWidget *area_widget, cairo_surface_t *surface);
static void set_source_rgb (cairo_t *cr, Color src);

static void draw_rectangle     (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad);
static void draw_horz_triangle (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad);
static void draw_vert_triangle (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad);

static void   draw_actual_axis (cairo_t *cr, Axis *axis, Region r, ColorScheme *colorscheme);
static char * make_tick_label  (cairo_t *cr, Axis *axis, VSP vs, Region r, double point, cairo_text_extents_t *ext, double *XTL, double *YTL);

static void brush_renew  (Brush *brush);
static void brush_stroke (Brush *brush);

static void plot_one_line  (double x, double y, Brush *brush);
static void plot_one_point (double x, double y, Brush *brush);

void set_source_rgb (cairo_t *cr, Color src)
{
	cairo_set_source_rgb(cr, src.r, src.g, src.b);
}

void flip_surface (GtkWidget *area_widget, cairo_surface_t *surface)
{
	// paints a clean plot (surface) onto the actual user's window (crd)

	cairo_t *crd = gdk_cairo_create(gtk_widget_get_window(area_widget));
	cairo_set_source_surface(crd, surface, 0, 0);
	cairo_paint(crd);
	cairo_destroy(crd);
}

void brush_renew (Brush *brush)
{
	if (brush->enabled)
	{
		cairo_new_path(brush->cr);
		cairo_new_path(brush->crd);

		set_source_rgb(brush->cr , brush->color);
		set_source_rgb(brush->crd, brush->color);

		if (brush->started)
		{
			cairo_move_to(brush->cr,  brush->x0, brush->y0);
			cairo_move_to(brush->crd, brush->x0, brush->y0);
		}
	}
}

void brush_stroke (Brush *brush)
{
	cairo_stroke(brush->cr);
	cairo_stroke(brush->crd);

	brush->unstroked = 0;
}

void plot_axis_points (Axis *axis_array, VSP vs, Region r, long begin, long end)
{
	bool do_axis[3];  // element 0 is not used
	for (int a = 1; a < 3; a++)
	{
		do_axis[a] = (axis_array[a].vci != -1);
		if (do_axis[a])
		{
			brush_renew(&axis_array[a].lines);
			brush_renew(&axis_array[a].points);
		}
	}

	Axis *x_axis = &axis_array[X_AXIS];
	for (long j = begin; j < end; j++)
	{
		double x = scale_point(vs_value(vs, j, x_axis->vci), x_axis, r.X0, r.X1);

		for (int a = 1; a < 3; a++)
		{
			if (do_axis[a])
			{
				double y = scale_point(vs_value(vs, j, axis_array[a].vci), &axis_array[a], r.Y1, r.Y0);

				if (detect_region(x, y, r) == REGION_INSIDE)
				{
					plot_one_line  (x, y, &axis_array[a].lines);
					plot_one_point (x, y, &axis_array[a].points);
				}
				else axis_array[a].lines.started = 0;
			}
		}
	}

	for (int a = 1; a < 3; a++)
	{
		if (do_axis[a])
		{
			if (axis_array[a].lines.enabled)  brush_stroke(&axis_array[a].lines);
			if (axis_array[a].points.enabled) brush_stroke(&axis_array[a].points);
		}
	}
}

void plot_one_line (double x, double y, Brush *brush)
{
	if (brush->enabled && (fabs(x - brush->x0) > M2_PLOT_LINE_MIN_DISPLACEMENT ||
	                       fabs(y - brush->y0) > M2_PLOT_LINE_MIN_DISPLACEMENT))
	{
		if (brush->unstroked > M2_MAX_CAIRO_PTS) brush_stroke(brush);

		brush->x0 = x;
		brush->y0 = y;

		if (brush->started)
		{
			cairo_line_to(brush->cr,  x, y);
			cairo_line_to(brush->crd, x, y);
		}
		else
		{
			cairo_move_to(brush->cr,  x, y);
			cairo_move_to(brush->crd, x, y);
			brush->started = 1;
		}

		brush->unstroked++;
	}
}

void plot_one_point (double x, double y, Brush *brush)
{
	// note: brush->started is not needed when plotting points

	if (brush->enabled && (fabs(x - brush->x0) > M2_PLOT_POINT_MIN_DISPLACEMENT ||
	                       fabs(y - brush->y0) > M2_PLOT_POINT_MIN_DISPLACEMENT))
	{
		if (brush->unstroked > M2_MAX_CAIRO_PTS) brush_stroke(brush);

		brush->x0 = x;
		brush->y0 = y;

		x -= M2_PLOT_POINT_HALFWIDTH;
		y -= M2_PLOT_POINT_HALFWIDTH;
		cairo_rectangle(brush->cr,  x, y, M2_PLOT_POINT_WIDTH, M2_PLOT_POINT_WIDTH);
		cairo_rectangle(brush->crd, x, y, M2_PLOT_POINT_WIDTH, M2_PLOT_POINT_WIDTH);

		brush->unstroked++;
	}
}

void draw_actual_axis (cairo_t *cr, Axis *axis, Region r, ColorScheme *colorscheme)
{
	set_source_rgb(cr, colorscheme->axis);

	if (axis->i == X_AXIS)
	{
		cairo_move_to(cr, r.X0, r.Y0);
		cairo_line_to(cr, r.X1, r.Y0);
		cairo_move_to(cr, r.X0, r.Y1);
		cairo_line_to(cr, r.X1, r.Y1);
	}
	else
	{
		cairo_move_to(cr, axis->i == 1 ? r.X0 : r.X1, r.Y0);
		cairo_line_to(cr, axis->i == 1 ? r.X0 : r.X1, r.Y1);
	}

	cairo_stroke(cr);
}

char * make_tick_label (cairo_t *cr, Axis *axis, VSP vs, Region r, double point, cairo_text_extents_t *ext, double *XTL, double *YTL)
{
	char *str = supercat(axis->tick_format, point);
	cairo_text_extents(cr, str, ext);

	if (axis->i == X_AXIS)
	{
		*XTL = scale_point(point, axis, r.X0, r.X1) - ext->width/2;
		*YTL = r.Y - M2_XBORDER - M2_XTL_OFFSET;
	}
	else
	{
		double border = (axis->vci != -1) ? M2_YBORDER + M2_YTL_OFFSET : M2_YTL_OFFSET;
		*XTL = (axis->i == Y1_AXIS) ? border : r.X - border - ext->width;
		*YTL = scale_point(point, axis, r.Y1, r.Y0) + ext->height/2;
	}

	return str;
}

int detect_region (double X, double Y, Region r)
{
	int xspot = (X < r.X0) ? -1 : ((X < r.X1) ? 0 :  1);
	int yspot = (Y < r.Y0) ?  1 : ((Y < r.Y1) ? 0 : -1);

	if (xspot == 0)
	{
		return (yspot == 0)  ? REGION_INSIDE :
		       (yspot == -1) ? REGION_BOTTOM : REGION_TOP;
	}
	else
	{
		return (yspot != 0)  ? REGION_CORNER :
		       (xspot == -1) ? REGION_LEFT : REGION_RIGHT;
	}
}

void draw_axis (cairo_t *cr, Axis *axis, VSP vs, Region r, ColorScheme *colorscheme, double *lw0, double *lw1, double *lw_max)
{
	f_start(F_VERBOSE);

	// passing a non-NULL value for lw0, lw1, or lw_max will turn on simulation mode
	bool sim = lw0 != NULL || lw1 != NULL || lw_max != NULL;

	// draw the actual axis
	if (!sim) draw_actual_axis(cr, axis, r, colorscheme);

	if (lw_max != NULL) *lw_max = 0.0;  // label width to determine plot margins
	cairo_text_extents_t ext;

	// ticks and in-between tick labels:
	if (!sim) set_source_rgb(cr, colorscheme->axis);

	for (int j = ceil_int(axis->limit.value[LOWER]/axis->tick); j <= floor_int(axis->limit.value[UPPER]/axis->tick); j++)
	{
		double tick_len = (j % 10 == 0) ? M2_TICK_LENGTH_LARGE :
		                  (j % 5  == 0) ? M2_TICK_LENGTH_MED   :
		                                  M2_TICK_LENGTH_SMALL;

		double point = j * axis->tick;
		double xory = scale_point(point, axis, (axis->i == X_AXIS) ? r.X0 : r.Y1, (axis->i == X_AXIS) ? r.X1 : r.Y0);

		// draw ticks:
		if (!sim)
		{
			if (axis->i == X_AXIS)
			{
				cairo_move_to(cr, xory, r.Y1);
				cairo_line_to(cr, xory, r.Y1 + tick_len);
			}
			else
			{
				cairo_move_to(cr, (axis->i == Y1_AXIS) ? r.X0 : r.X1, xory);
				cairo_line_to(cr, (axis->i == Y1_AXIS) ? r.X0 - tick_len : r.X1 + tick_len, xory);
			}
		}

		// tick labels:
		if (j % 5 == 0)
		{
			double xtl, ytl;
			char *str = atg(make_tick_label(cr, axis, vs, r, point, &ext, &xtl, &ytl));

			if (lw_max != NULL) *lw_max = max_double(*lw_max, ext.width);

			if (!sim)
			{
				cairo_move_to(cr, xtl, ytl);
				cairo_show_text(cr, str);
			}
		}
	}

	if (!sim) cairo_stroke(cr);

	// min/max tick labels:
	double minmax_xtl[2], minmax_ytl[2];
	for sides
	{
		char *str = atg(make_tick_label(cr, axis, vs, r, axis->limit.value[s], &ext, &minmax_xtl[s], &minmax_ytl[s]));

		if (s == LOWER) { if (lw0 != NULL) *lw0 = ext.width; }
		else            { if (lw1 != NULL) *lw1 = ext.width; }

		if (lw_max != NULL) *lw_max = max_double(*lw_max, ext.width);

		if (!sim)
		{
			axis->label[s].X0 = minmax_xtl[s] - 1;
			axis->label[s].X1 = minmax_xtl[s] + ext.width + 4;
			axis->label[s].Y0 = minmax_ytl[s] - ext.height - 2;
			axis->label[s].Y1 = minmax_ytl[s] + 2;

			set_source_rgb(cr, colorscheme->background);
			draw_rectangle(cr, axis->label[s].X0, axis->label[s].X1, axis->label[s].Y0, axis->label[s].Y1, M2_MINMAX_LABEL_BORDER);

			set_source_rgb(cr, colorscheme->minmax_bg);
			draw_rectangle(cr, axis->label[s].X0, axis->label[s].X1, axis->label[s].Y0, axis->label[s].Y1, 0);

			set_source_rgb(cr, colorscheme->minmax);
			cairo_move_to(cr, minmax_xtl[s], minmax_ytl[s]);
			cairo_show_text(cr, str);
			cairo_stroke(cr);
		}
	}

	// draw axis labels
	if (!sim && axis->vci != -1)
	{
		set_source_rgb(cr, colorscheme->label);

		char *label = atg(supercat("%s (%s)", axis->name, get_colunit(vs, axis->vci)));
		cairo_text_extents(cr, label, &ext);

		if (axis->i == X_AXIS)
		{
			cairo_move_to(cr, (r.X0 + r.X1) / 2.0 - ext.width / 2.0, r.Y - M2_XLABEL_OFFSET);
			cairo_show_text(cr, label);
		}
		else
		{
			cairo_move_to(cr, (axis->i == Y1_AXIS) ? M2_YLABEL_OFFSET : r.X - M2_YLABEL_OFFSET, (r.Y0 + r.Y1) / 2.0 + ext.width / (axis->i == Y1_AXIS ? 2.0 : -2.0));
			cairo_rotate(cr, G_PI / (axis->i == Y1_AXIS ? -2.0 : 2.0));
			cairo_show_text(cr, label);
			cairo_rotate(cr, G_PI / (axis->i == Y1_AXIS ? 2.0 : -2.0));
		}

		cairo_stroke(cr);
	}
}

void draw_rectangle (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad)
{
	double Xpad = (X1 > X0) ? pad : -1 * pad;
	double Ypad = (Y1 > Y0) ? pad : -1 * pad;

	cairo_move_to(cr, X0 - Xpad, Y0 - Ypad);
	cairo_line_to(cr, X1 + Xpad, Y0 - Ypad);
	cairo_line_to(cr, X1 + Xpad, Y1 + Ypad);
	cairo_line_to(cr, X0 - Xpad, Y1 + Ypad);
	cairo_close_path(cr);

	cairo_fill_preserve(cr);
	cairo_stroke(cr);
}

void draw_horz_triangle (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad)
{
	double Xpad = (X1 > X0) ? pad : -1 * pad;
	double Ypad = (Y1 > Y0) ? pad : -1 * pad;

	cairo_move_to(cr, X0 - Xpad, Y0 - Ypad);
	cairo_line_to(cr, X0 - Xpad, Y1 + Ypad);
	cairo_line_to(cr, X1 + Xpad, (Y0 + Y1) / 2);
	cairo_close_path(cr);

	cairo_fill_preserve(cr);
	cairo_stroke(cr);
}

void draw_vert_triangle (cairo_t *cr, double X0, double X1, double Y0, double Y1, double pad)
{
	double Xpad = (X1 > X0) ? pad : -1 * pad;
	double Ypad = (Y1 > Y0) ? pad : -1 * pad;

	cairo_move_to(cr, X0 - Xpad,     Y0 - Ypad);
	cairo_line_to(cr, X1 + Xpad,     Y0 - Ypad);
	cairo_line_to(cr, (X0 + X1) / 2, Y1 + Ypad);
	cairo_close_path(cr);

	cairo_fill_preserve(cr);
	cairo_stroke(cr);
}

void draw_buffer_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme)
{
	double buffer_height = M2_BUFFER_SYMBOL_WIDTH  * 0.5;

	cairo_new_path(cr);
	set_source_rgb(cr, colorscheme->display);

	draw_horz_triangle(cr, 4 + M2_BUFFER_SYMBOL_WIDTH * 0.3, 4 + M2_BUFFER_SYMBOL_WIDTH * 0.68, r.Y - 9, r.Y - 9 - buffer_height, 0);
	cairo_move_to(cr, 4,                          r.Y - 9 - (buffer_height / 2));
	cairo_line_to(cr, 4 + M2_BUFFER_SYMBOL_WIDTH, r.Y - 9 - (buffer_height / 2));

	cairo_stroke(cr);
}

#define SCOPE_WIDTH  14
#define SCOPE_HEIGHT 11

void draw_scope_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme)
{
	double X_left = r.X - 6 - M2_SCOPE_PROGRESS_WIDTH - 6 - SCOPE_WIDTH;
	set_source_rgb(cr, colorscheme->display);
	draw_rectangle(cr, X_left, X_left + SCOPE_WIDTH, r.Y - 6, r.Y - 6 - SCOPE_HEIGHT, 0);

	set_source_rgb(cr, colorscheme->display_bg);
	cairo_move_to(cr, X_left + SCOPE_WIDTH * 0.10, r.Y - 6 - SCOPE_HEIGHT * 0.25);
	cairo_line_to(cr, X_left + SCOPE_WIDTH * 0.40, r.Y - 6 - SCOPE_HEIGHT * 0.25);
	cairo_line_to(cr, X_left + SCOPE_WIDTH * 0.65, r.Y - 6 - SCOPE_HEIGHT * 0.65);
	cairo_line_to(cr, X_left + SCOPE_WIDTH * 0.65, r.Y - 6 - SCOPE_HEIGHT * 0.25);
	cairo_line_to(cr, X_left + SCOPE_WIDTH * 0.90, r.Y - 6 - SCOPE_HEIGHT * 0.25);
	cairo_stroke(cr);
}

void erase_scope_symbol (cairo_t *cr, Region r, ColorScheme *colorscheme)
{
	double X_left = r.X - 6 - M2_SCOPE_PROGRESS_WIDTH - 6 - SCOPE_WIDTH;
	set_source_rgb(cr, colorscheme->background);
	draw_rectangle(cr, X_left, X_left + SCOPE_WIDTH, r.Y - 6, r.Y - 6 - SCOPE_HEIGHT, 1);
}

#undef SCOPE_WIDTH
#undef SCOPE_HEIGHT

void draw_buffer_display (Display *display, Region r, ColorScheme *colorscheme)
{
	if (display->str_dirty)
	{
		display->str_dirty = 0;

		double X_left = 4 + M2_BUFFER_SYMBOL_WIDTH + 6;
		double Y_top = r.Y - 6 - display->ext.height;

		cairo_new_path(display->cr);
		cairo_new_path(display->crd);

		// cover old readout
		set_source_rgb(display->cr,  colorscheme->background);
		set_source_rgb(display->crd, colorscheme->background);

		draw_rectangle(display->cr,  X_left, X_left + display->ext.width, Y_top, Y_top + display->ext.height, 1);
		draw_rectangle(display->crd, X_left, X_left + display->ext.width, Y_top, Y_top + display->ext.height, 1);

		set_source_rgb(display->cr,  colorscheme->display);
		set_source_rgb(display->crd, colorscheme->display);

		cairo_text_extents(display->cr, display->str, &display->ext);

		cairo_move_to(display->cr,  X_left, r.Y - 2 - display->ext.height / 2);
		cairo_move_to(display->crd, X_left, r.Y - 2 - display->ext.height / 2);

		cairo_show_text(display->cr,  display->str);
		cairo_show_text(display->crd, display->str);

		cairo_stroke(display->cr);
		cairo_stroke(display->crd);
	}

	if (display->percent != display->old_percent)
	{
		double X_left = r.X - 6 - M2_SCOPE_PROGRESS_WIDTH;
		double Y_top  = r.Y - 13;

		bool need_bar_bg = 0;

		cairo_new_path(display->cr);
		cairo_new_path(display->crd);

		if (display->percent == -1)  // erase symbol
		{
			erase_scope_symbol(display->cr,  r, colorscheme);
			erase_scope_symbol(display->crd, r, colorscheme);
		}
		else if (display->old_percent == -1)  // draw symbol
		{
			draw_scope_symbol(display->cr,  r, colorscheme);
			draw_scope_symbol(display->crd, r, colorscheme);

			need_bar_bg = 1;
		}
		
		if (display->percent < display->old_percent)  // erase old bar
		{
			set_source_rgb(display->cr,  colorscheme->background);
			set_source_rgb(display->crd, colorscheme->background);

			draw_rectangle(display->cr,  X_left, X_left + M2_SCOPE_PROGRESS_WIDTH, Y_top, Y_top + 4, 1);
			draw_rectangle(display->crd, X_left, X_left + M2_SCOPE_PROGRESS_WIDTH, Y_top, Y_top + 4, 1);

			if (display->percent >= 0) need_bar_bg = 1;
		}

		if (need_bar_bg)
		{
			cairo_new_path(display->cr);
			cairo_new_path(display->crd);

			set_source_rgb(display->cr,  colorscheme->display_bg);
			set_source_rgb(display->crd, colorscheme->display_bg);

			draw_rectangle(display->cr,  X_left, X_left + M2_SCOPE_PROGRESS_WIDTH, Y_top, Y_top + 4, 0);
			draw_rectangle(display->crd, X_left, X_left + M2_SCOPE_PROGRESS_WIDTH, Y_top, Y_top + 4, 0);
		}

		if (display->percent > 0)  // fill in bar
		{
			set_source_rgb(display->cr,  colorscheme->display);
			set_source_rgb(display->crd, colorscheme->display);

			draw_rectangle(display->cr,  X_left, X_left + M2_SCOPE_PROGRESS_WIDTH * (double) display->percent / 100.0, Y_top, Y_top + 4, 0);
			draw_rectangle(display->crd, X_left, X_left + M2_SCOPE_PROGRESS_WIDTH * (double) display->percent / 100.0, Y_top, Y_top + 4, 0);
		}

		display->old_percent = display->percent;
	}
}

void draw_marker (cairo_t *cr, Axis *axis_array, Region r, ColorScheme *colorscheme, double XM, double YM)
{
	f_start(F_RUN);

	char str[16];
	cairo_text_extents_t ext;

	cairo_new_path(cr);
	cairo_set_font_size(cr, M2_TICK_LABEL_SIZE);

	// X axis

	if (axis_array[X_AXIS].vci != -1)
	{
		snprintf(str, 16, axis_array[X_AXIS].marker_format, descale_point(XM, &axis_array[X_AXIS], r.X0, r.X1));
		cairo_text_extents(cr, str, &ext);

		double Ybase = r.Y1 + M2_MARKER_ARROW_OFFSET + M2_MARKER_ARROW_SIZE + ext.height;

		set_source_rgb     (cr, colorscheme->background);
		draw_rectangle     (cr, XM - ext.width / 2,               XM + ext.width / 2 + 1,           Ybase,              Ybase - ext.height,            2);
		draw_vert_triangle (cr, XM - M2_MARKER_ARROW_SIZE * 0.75, XM + M2_MARKER_ARROW_SIZE * 0.75, Ybase - ext.height, r.Y1 + M2_MARKER_ARROW_OFFSET, 1);

		set_source_rgb     (cr, colorscheme->marker);
		draw_rectangle     (cr, XM - ext.width / 2,               XM + ext.width / 2 + 1,           Ybase,              Ybase - ext.height,            1);
		draw_vert_triangle (cr, XM - M2_MARKER_ARROW_SIZE * 0.75, XM + M2_MARKER_ARROW_SIZE * 0.75, Ybase - ext.height, r.Y1 + M2_MARKER_ARROW_OFFSET, 0);

		set_source_rgb  (cr, colorscheme->background);
		cairo_move_to   (cr, XM - ext.width / 2, Ybase);
		cairo_show_text (cr, str);
	}

	// Y axes

	for (int a = 1; a < 3; a++)
	{
		if (axis_array[a].vci == -1) continue;

		snprintf(str, 16, axis_array[a].marker_format, descale_point(YM, &axis_array[a], r.Y1, r.Y0));
		cairo_text_extents(cr, str, &ext);

		double Xbase = (a == Y1_AXIS) ? r.X0 - M2_MARKER_ARROW_OFFSET - M2_MARKER_ARROW_SIZE - ext.width :
		                                r.X1 + M2_MARKER_ARROW_OFFSET + M2_MARKER_ARROW_SIZE;

		double Xtri0 = (a == Y1_AXIS) ? Xbase + ext.width             : Xbase;
		double Xtri1 = (a == Y1_AXIS) ? r.X0 - M2_MARKER_ARROW_OFFSET : r.X1 + M2_MARKER_ARROW_OFFSET;

		set_source_rgb     (cr, colorscheme->background);
		draw_rectangle     (cr, Xbase, Xbase + ext.width + 1, YM - ext.height / 2,     YM + ext.height / 2,     2);
		draw_horz_triangle (cr, Xtri0, Xtri1,                 YM - ext.height / 2 + 1, YM + ext.height / 2 - 1, 1);

		set_source_rgb     (cr, colorscheme->marker);
		draw_rectangle     (cr, Xbase, Xbase + ext.width + 1, YM - ext.height / 2,     YM + ext.height / 2,     1);
		draw_horz_triangle (cr, Xtri0, Xtri1,                 YM - ext.height / 2 + 1, YM + ext.height / 2 - 1, 0);

		set_source_rgb  (cr, colorscheme->background);
		cairo_move_to   (cr, Xbase, YM + ext.height / 2);
		cairo_show_text (cr, str);
	}

	// marker itself

	set_source_rgb(cr, colorscheme->marker);

	cairo_move_to(cr, XM - M2_MARKER_LENGTH, YM);
	cairo_line_to(cr, XM - 2,                YM);
	cairo_move_to(cr, XM + M2_MARKER_LENGTH, YM);
	cairo_line_to(cr, XM + 2,                YM);
	cairo_move_to(cr, XM,                    YM - M2_MARKER_LENGTH);
	cairo_line_to(cr, XM,                    YM - 2);
	cairo_move_to(cr, XM,                    YM + M2_MARKER_LENGTH);
	cairo_line_to(cr, XM,                    YM + 2);

	cairo_stroke(cr);
}
