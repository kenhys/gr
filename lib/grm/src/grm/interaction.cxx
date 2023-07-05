/* ######################### includes ############################################################################### */

#include <cmath>
#include <cfloat>
#include <climits>

#include "args_int.h"
#include "interaction_int.h"
#include "plot_int.h"
#include "util_int.h"
#include "gr.h"
#include "grm/dom_render/render.hxx"
#include <grm/dom_render/graphics_tree/util.hxx>


/* ========================= macros ================================================================================= */

/* ------------------------- math ----------------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ######################### public implementation ################################################################## */

/* ========================= methods ================================================================================ */

/* ------------------------- user interaction ----------------------------------------------------------------------- */

int grm_input(const grm_args_t *input_args)
{
  /*
   * reset_ranges:
   * - `x`, `y`: mouse cursor position
   * - `key`: Pressed key (as string)
   * zoom:
   * - `x`, `y`: start point
   * - `angle_delta`: mouse wheel rotation angle in eighths of a degree, can be replaced by factor (double type)
   * - `factor`: zoom factor, can be replaced by angle_delta (double type)
   * box zoom:
   * - `x1`, `y1`, `x2`, `y2`: coordinates of a box selection, (x1, y1) is the fixed corner
   * - `keep_aspect_ratio`: if set to `1`, the aspect ratio of the gr window is preserved (defaults to `1`)
   * pan:
   * - `x`, `y`: start point
   * - `xshift`, `yshift`: shift in x and y direction
   *
   * All coordinates are expected to be given as workstation coordinates (integer type)
   */
  int width, height, max_width_height;
  int x, y, x1, y1, x2, y2;
  double viewport_mid_x, viewport_mid_y;

  logger((stderr, "Processing input\n"));

  get_figure_size(nullptr, &width, &height, nullptr, nullptr);
  max_width_height = grm_max(width, height);
  logger((stderr, "Using size (%d, %d)\n", width, height));

  if (grm_args_values(input_args, "x", "i", &x) && grm_args_values(input_args, "y", "i", &y))
    {
      double ndc_x, ndc_y;
      char *key;

      ndc_x = (double)x / max_width_height;
      ndc_y = (double)(height - y) / max_width_height;
      logger((stderr, "x: %d, y: %d, ndc_x: %lf, ndc_y: %lf\n", x, y, ndc_x, ndc_y));

      auto subplot_element = get_subplot_from_ndc_point_using_dom(ndc_x, ndc_y);

      if (grm_args_values(input_args, "key", "s", &key))
        {
          logger((stderr, "Got key \"%s\"\n", key));

          if (strcmp(key, "r") == 0)
            {
              if (subplot_element != nullptr)
                {
                  logger((stderr, "Reset single subplot coordinate ranges\n"));
                  subplot_element->setAttribute("reset_ranges", 1);
                }
              else
                {
                  logger((stderr, "Reset all subplot coordinate ranges\n"));
                  grm_set_attribute_on_all_subplots("reset_ranges", 1);
                }
            }

          return 1;
        }

      if (subplot_element != nullptr)
        {
          double angle_delta, factor;
          int xshift, yshift, xind, yind;
          std::string kind;
          double viewport[4];
          viewport[0] = static_cast<double>(subplot_element->getAttribute("viewport_xmin"));
          viewport[1] = static_cast<double>(subplot_element->getAttribute("viewport_xmax"));
          viewport[2] = static_cast<double>(subplot_element->getAttribute("viewport_ymin"));
          viewport[3] = static_cast<double>(subplot_element->getAttribute("viewport_ymax"));

          kind = static_cast<std::string>(subplot_element->getAttribute("kind"));
          if (kind == "marginalheatmap")
            {
              auto current_series = subplot_element->querySelectorsAll("series_heatmap")[0];

              unsigned int x_length, y_length;
              double x_0, x_end, y_0, y_end, x_step, y_step;

              grm_args_values(input_args, "x", "i", &x);
              grm_args_values(input_args, "y", "i", &y);
              auto x_series_key = static_cast<std::string>(current_series->getAttribute("x"));
              auto y_series_key = static_cast<std::string>(current_series->getAttribute("y"));

              std::shared_ptr<GRM::Context> context = grm_get_render()->getContext();

              auto x_series_vec = GRM::get<std::vector<double>>((*context)[x_series_key]);
              auto y_series_vec = GRM::get<std::vector<double>>((*context)[y_series_key]);

              x_length = x_series_vec.size();
              y_length = y_series_vec.size();
              x_0 = x_series_vec[0], x_end = x_series_vec[x_length - 1];
              y_0 = y_series_vec[0], y_end = y_series_vec[y_length - 1];

              GRM::Render::processViewport(subplot_element);
              GRM::Render::processLimits(subplot_element);

              gr_savestate();
              for (const auto &elem : subplot_element->parentElement()->children())
                {
                  if (elem->hasAttribute("viewport_xmin"))
                    {
                      gr_setviewport(
                          (double)elem->getAttribute("viewport_xmin"), (double)elem->getAttribute("viewport_xmax"),
                          (double)elem->getAttribute("viewport_ymin"), (double)elem->getAttribute("viewport_ymax"));
                      gr_setwindow((double)elem->getAttribute("window_xmin"), (double)elem->getAttribute("window_xmax"),
                                   (double)elem->getAttribute("window_ymin"),
                                   (double)elem->getAttribute("window_ymax"));
                      break;
                    }
                }

              gr_wctondc(&x_0, &y_0);
              gr_wctondc(&x_end, &y_end);
              x_0 = x_0 * max_width_height;
              x_end = x_end * max_width_height;
              y_0 = height - y_0 * max_width_height;
              y_end = height - y_end * max_width_height;

              x_step = (x_end - x_0) / x_length;
              y_step = (y_end - y_0) / y_length;
              xind = (int)((x - x_0) / x_step);
              yind = (int)((y - y_0) / y_step);

              if (xind < 0 || xind >= x_length || yind < 0 || yind >= y_length)
                {
                  xind = -1;
                  yind = -1;
                }

              current_series->parentElement()->setAttribute("xind", xind);
              current_series->parentElement()->setAttribute("yind", yind);

              for (auto &child : current_series->parentElement()->children())
                {
                  std::string childKind = static_cast<std::string>(child->getAttribute("kind"));
                  if (childKind == "hist")
                    {
                      // reset bar colors
                      bool is_horizontal = static_cast<std::string>(child->getAttribute("orientation")) == "horizontal";

                      for (auto &childSeries : child->children())
                        {
                          auto groups = childSeries->children(); // innerFillGroup and outerFillGroup
                          std::shared_ptr<GRM::Element> innerFillGroup;
                          if (groups.size() == 2)
                            {
                              innerFillGroup = groups[0];
                            }
                          else
                            {
                              // no fillGroups?
                              break;
                            }

                          int fillColorInd = static_cast<int>(innerFillGroup->getAttribute("fillcolorind"));
                          if (xind != -1)
                            {
                              innerFillGroup->children()[xind]->removeAttribute("fillcolorind");
                            }
                          if (yind != -1)
                            {
                              innerFillGroup->children()[yind]->removeAttribute("fillcolorind");
                            }
                        }
                    }
                }
              gr_restorestate();
            }

          if (grm_args_values(input_args, "angle_delta", "d", &angle_delta))
            {
              double focus_x, focus_y;

              if (str_equals_any(kind.c_str(), 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  /*
                   * TODO Zoom in 3D
                   */
                }
              else
                {
                  viewport_mid_x = (viewport[0] + viewport[1]) / 2.0;
                  viewport_mid_y = (viewport[2] + viewport[3]) / 2.0;
                  focus_x = ndc_x - viewport_mid_x;
                  focus_y = ndc_y - viewport_mid_y;
                  logger(
                      (stderr, "Zoom to ndc focus point (%lf, %lf), angle_delta %lf\n", focus_x, focus_y, angle_delta));
                  double zoom = 1.0 - INPUT_ANGLE_DELTA_FACTOR * angle_delta;
                  auto panzoom_element = grm_get_render()->createPanzoom(focus_x, focus_y, zoom, zoom);
                  subplot_element->append(panzoom_element);
                  subplot_element->setAttribute("panzoom", true);
                }

              return 1;
            }
          else if (grm_args_values(input_args, "factor", "d", &factor))
            {
              double focus_x, focus_y;

              if (str_equals_any(kind.c_str(), 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  /*
                   * TODO Zoom in 3D
                   */
                }
              else
                {
                  viewport_mid_x = (viewport[0] + viewport[1]) / 2.0;
                  viewport_mid_y = (viewport[2] + viewport[3]) / 2.0;
                  focus_x = ndc_x - viewport_mid_x;
                  focus_y = ndc_y - viewport_mid_y;
                  logger((stderr, "Zoom to ndc focus point (%lf, %lf), factor %lf\n", focus_x, focus_y, factor));
                  auto panzoom_element = grm_get_render()->createPanzoom(focus_x, focus_y, factor, factor);
                  subplot_element->append(panzoom_element);
                  subplot_element->setAttribute("panzoom", true);
                }

              return 1;
            }

          if (grm_args_values(input_args, "xshift", "i", &xshift) &&
              grm_args_values(input_args, "yshift", "i", &yshift))
            {
              double ndc_xshift, ndc_yshift, rotation, tilt;
              int shift_pressed;

              if (str_equals_any(kind.c_str(), 7, "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume",
                                 "isosurface"))
                {
                  if (grm_args_values(input_args, "shift_pressed", "i", &shift_pressed) && shift_pressed)
                    {
                      /*
                       * TODO Translate in 3D
                       */
                    }
                  else
                    {
                      rotation = static_cast<double>(subplot_element->getAttribute("space3d_phi"));
                      tilt = static_cast<double>(subplot_element->getAttribute("space3d_theta"));

                      rotation += xshift * 0.2;
                      tilt -= yshift * 0.2;

                      if (tilt > 180)
                        {
                          tilt = 180;
                        }
                      else if (tilt < 0)
                        {
                          tilt = 0;
                        }
                      subplot_element->setAttribute("space3d_phi", rotation);
                      subplot_element->setAttribute("space3d_theta", tilt);
                    }
                }
              else
                {
                  ndc_xshift = (double)-xshift / max_width_height;
                  ndc_yshift = (double)yshift / max_width_height;
                  logger((stderr, "Translate by ndc coordinates (%lf, %lf)\n", ndc_xshift, ndc_yshift));
                  auto panzoom_element = grm_get_render()->createPanzoom(ndc_xshift, ndc_yshift, 0, 0);
                  subplot_element->append(panzoom_element);
                  subplot_element->setAttribute("panzoom", true);
                }
              return 1;
            }
        }
    }

  if (grm_args_values(input_args, "x1", "i", &x1) && grm_args_values(input_args, "x2", "i", &x2) &&
      grm_args_values(input_args, "y1", "i", &y1) && grm_args_values(input_args, "y2", "i", &y2))
    {
      double focus_x, focus_y, factor_x, factor_y;
      int keep_aspect_ratio = INPUT_DEFAULT_KEEP_ASPECT_RATIO;
      std::shared_ptr<GRM::Element> subplot_element;

      grm_args_values(input_args, "keep_aspect_ratio", "i", &keep_aspect_ratio);

      if (!get_focus_and_factor_from_dom(x1, y1, x2, y2, keep_aspect_ratio, &factor_x, &factor_y, &focus_x, &focus_y,
                                         subplot_element))
        {
          return 0;
        }

      logger((stderr, "Got widget size: (%d, %d)\n", width, height));
      logger((stderr, "Got box: (%d, %d, %d, %d)\n", x1, y1, x2, y2));
      logger((stderr, "zoom focus: (%lf, %lf)\n", focus_x, focus_y));
      logger((stderr, "zoom factors: (%lf, %lf)\n", factor_x, factor_y));

      auto panzoom_element = grm_get_render()->createPanzoom(focus_x, focus_y, factor_x, factor_y);
      subplot_element->append(panzoom_element);
      subplot_element->setAttribute("panzoom", true);

      return 1;
    }

  return 0;
}

int grm_is3d(const int x, const int y)
{
  int width, height, max_width_height;
  double ndc_x, ndc_y;
  const char *kind;

  get_figure_size(nullptr, &width, &height, nullptr, nullptr);
  max_width_height = grm_max(width, height);
  ndc_x = (double)x / max_width_height;
  ndc_y = (double)y / max_width_height;

  auto subplot_element = get_subplot_from_ndc_points_using_dom(1, &ndc_x, &ndc_y);

  if (subplot_element && str_equals_any(static_cast<std::string>(subplot_element->getAttribute("kind")).c_str(), 7,
                                        "wireframe", "surface", "plot3", "scatter3", "trisurf", "volume", "isosurface"))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

int grm_get_box(const int x1, const int y1, const int x2, const int y2, const int keep_aspect_ratio, int *x, int *y,
                int *w, int *h)
{
  int width, height, max_width_height;
  double focus_x, focus_y, factor_x, factor_y;
  double viewport_mid_x, viewport_mid_y;
  double wswindow[4], viewport[4];
  std::shared_ptr<GRM::Element> subplot_element;
  get_figure_size(nullptr, &width, &height, nullptr, nullptr);
  max_width_height = grm_max(width, height);
  if (!get_focus_and_factor_from_dom(x1, y1, x2, y2, keep_aspect_ratio, &factor_x, &factor_y, &focus_x, &focus_y,
                                     subplot_element))
    {
      return 0;
    }
  wswindow[0] = static_cast<double>(subplot_element->parentElement()->getAttribute("wswindow_xmin"));
  wswindow[1] = static_cast<double>(subplot_element->parentElement()->getAttribute("wswindow_xmax"));
  wswindow[2] = static_cast<double>(subplot_element->parentElement()->getAttribute("wswindow_ymin"));
  wswindow[3] = static_cast<double>(subplot_element->parentElement()->getAttribute("wswindow_ymax"));
  viewport[0] = static_cast<double>(subplot_element->getAttribute("viewport_xmin"));
  viewport[1] = static_cast<double>(subplot_element->getAttribute("viewport_xmax"));
  viewport[2] = static_cast<double>(subplot_element->getAttribute("viewport_ymin"));
  viewport[3] = static_cast<double>(subplot_element->getAttribute("viewport_ymax"));
  viewport_mid_x = (viewport[1] + viewport[0]) / 2.0;
  viewport_mid_y = (viewport[3] + viewport[2]) / 2.0;
  *w = (int)grm_round(factor_x * width * (viewport[1] - viewport[0]) / (wswindow[1] - wswindow[0]));
  *h = (int)grm_round(factor_y * height * (viewport[3] - viewport[2]) / (wswindow[3] - wswindow[2]));
  *x = (int)grm_round(((viewport_mid_x + focus_x) - ((viewport_mid_x + focus_x) - viewport[0]) * factor_x) *
                      max_width_height);
  *y = (int)grm_round(height - ((viewport_mid_y + focus_y) - ((viewport_mid_y + focus_y) - viewport[3]) * factor_y) *
                                   max_width_height);
  return 1;
}

grm_tooltip_info_t *grm_get_tooltip(const int mouse_x, const int mouse_y)
{
  auto info = static_cast<grm_tooltip_info_t *>(malloc(sizeof(grm_tooltip_info_t)));
  double *x_series, *y_series, *z_series, x, y, x_min, x_max, y_min, y_max, mindiff = DBL_MAX, diff;
  double x_range_min, x_range_max, y_range_min, y_range_max, x_px, y_px;
  int width, height, max_width_height;
  std::vector<std::string> labels;
  unsigned int num_labels = 0;
  std::string kind;
  unsigned int x_length, y_length, z_length, series_i = 0, i;
  std::string orientation;
  int is_vertical = 0;

  get_figure_size(nullptr, &width, &height, nullptr, nullptr);
  max_width_height = grm_max(width, height);
  x = (double)mouse_x / max_width_height;
  y = (double)(height - mouse_y) / max_width_height;

  auto subplot_element = get_subplot_from_ndc_points_using_dom(1, &x, &y);

  if (subplot_element != nullptr)
    {
      kind = static_cast<std::string>(subplot_element->getAttribute("kind"));
      if (subplot_element->hasAttribute("orientation"))
        {
          orientation = static_cast<std::string>(subplot_element->getAttribute("orientation"));
          is_vertical = orientation == "vertical";
        }
    }
  if (subplot_element == nullptr ||
      !str_equals_any(kind.c_str(), 13, "line", "scatter", "stem", "stairs", "heatmap", "marginalheatmap", "contour",
                      "imshow", "contourf", "pie", "hexbin", "shade", "quiver"))
    {
      info->x_px = -1;
      info->y_px = -1;
      info->x = 0;
      info->y = 0;
      info->xlabel = "x";
      info->ylabel = "y";
      info->label = "";
      return info;
    }

  GRM::Render::processViewport(subplot_element);
  GRM::Render::processLimits(subplot_element);

  gr_savestate();
  if (subplot_element->hasAttribute("viewport_xmin"))
    {
      gr_setviewport((double)subplot_element->getAttribute("viewport_xmin"),
                     (double)subplot_element->getAttribute("viewport_xmax"),
                     (double)subplot_element->getAttribute("viewport_ymin"),
                     (double)subplot_element->getAttribute("viewport_ymax"));
      gr_setwindow(
          (double)subplot_element->getAttribute("window_xmin"), (double)subplot_element->getAttribute("window_xmax"),
          (double)subplot_element->getAttribute("window_ymin"), (double)subplot_element->getAttribute("window_ymax"));
    }

  gr_ndctowc(&x, &y);

  auto axes_vec = subplot_element->querySelectorsAll("axes");
  std::vector<std::shared_ptr<GRM::Element>> label_vec;
  for (const auto &child : axes_vec)
    {
      if (child->hasAttribute("xlabel") && child->hasAttribute("ylabel"))
        {
          label_vec.push_back(child);
          break;
        }
    }

  if (label_vec.empty())
    {
      info->xlabel = "x";
      info->ylabel = "y";
    }
  else
    {
      if (!label_vec[0]->hasAttribute("xlabel"))
        {
          info->xlabel = "x";
        }
      else
        {
          static std::string xlabel = static_cast<std::string>(label_vec[0]->getAttribute("xlabel"));
          info->xlabel = xlabel.c_str();
        }
      if (!label_vec[0]->hasAttribute("ylabel"))
        {
          info->ylabel = "y";
        }
      else
        {
          static std::string ylabel = static_cast<std::string>(label_vec[0]->getAttribute("ylabel"));
          info->ylabel = ylabel.c_str();
        }
    }

  x_range_min = (double)(mouse_x - 50) / max_width_height;
  x_range_max = (double)(mouse_x + 50) / max_width_height;
  y_range_min = (double)(height - (mouse_y + 50)) / max_width_height;
  y_range_max = (double)(height - (mouse_y - 50)) / max_width_height;
  gr_ndctowc(&x_range_min, &y_range_min);
  gr_ndctowc(&x_range_max, &y_range_max);

  x_min = static_cast<double>(subplot_element->getAttribute("_xlim_min"));
  x_max = static_cast<double>(subplot_element->getAttribute("_xlim_max"));
  y_min = static_cast<double>(subplot_element->getAttribute("_ylim_min"));
  y_max = static_cast<double>(subplot_element->getAttribute("_ylim_max"));

  x_range_min = (x_min > x_range_min) ? x_min : x_range_min;
  y_range_min = (y_min > y_range_min) ? y_min : y_range_min;
  x_range_max = (x_max < x_range_max) ? x_max : x_range_max;
  y_range_max = (y_max < y_range_max) ? y_max : y_range_max;

  std::shared_ptr<GRM::Context> context = grm_get_render()->getContext();
  auto draw_legend_element_vec = subplot_element->getElementsByClassName("legend");
  num_labels = 0;
  if (!draw_legend_element_vec.empty())
    {
      auto &draw_legend_element = draw_legend_element_vec[0];
      std::string labels_key = static_cast<std::string>(draw_legend_element->getAttribute("labels"));
      labels = GRM::get<std::vector<std::string>>((*context)[labels_key]);
      num_labels = labels.size();
    }
  if (kind == "pie")
    {
      static char output[50];
      double max_x = 0.95, min_x = 0.05, max_y = 0.05, min_y = 0.95;
      int center_x, center_y;
      double radius;
      double start_angle, end_angle, act_angle;

      gr_wctondc(&max_x, &max_y);
      max_x = max_x * max_width_height;
      max_y = height - max_y * max_width_height;
      gr_wctondc(&min_x, &min_y);
      min_x = min_x * max_width_height;
      min_y = height - min_y * max_width_height;

      /* calculate the circle */
      radius = (max_x - min_x) / 2.0;
      center_x = (int)(max_x - radius);
      center_y = (int)(max_y - radius);

      auto current_series = subplot_element->querySelectorsAll("series_" + kind)[0];
      auto x_key = static_cast<std::string>(current_series->getAttribute("x"));
      std::vector<double> x_series_vec;
      x_series_vec = GRM::get<std::vector<double>>((*context)[x_key]);

      x_length = x_series_vec.size();
      std::vector<double> normalized_x(x_length);
      GRM::normalize_vec(x_series_vec, &normalized_x);
      start_angle = 90.0;
      for (i = 0; i < x_length; ++i)
        {
          end_angle = start_angle - normalized_x[i] * 360.0;
          act_angle =
              acos((mouse_x - center_x) / sqrt(pow(abs(mouse_x - center_x), 2) + pow(abs(mouse_y - center_y), 2)));
          act_angle = (mouse_y - center_y > 0) ? act_angle * 180 / M_PI : 360 - act_angle * 180 / M_PI;
          if (act_angle >= 270 && act_angle <= 360) act_angle = act_angle - 360;
          act_angle *= -1;
          if (start_angle >= act_angle && end_angle <= act_angle)
            {
              snprintf(output, 50, "%f", x_series_vec[i]);
            }
          start_angle = end_angle;
        }

      if (sqrt(pow(abs(mouse_x - center_x), 2) + pow(abs(mouse_y - center_y), 2)) <= radius)
        {
          mindiff = 0;
          info->x = 0;
          info->y = 0;
          info->x_px = mouse_x;
          info->y_px = mouse_y;
          info->label = output;
          return info;
        }
      else
        {
          mindiff = DBL_MAX;
        }
    }
  else
    {
      auto current_series_group_vec = subplot_element->querySelectorsAll("series_" + kind);
      std::vector<std::shared_ptr<GRM::Element>> current_series_vec;
      for (const auto &current_series_group : current_series_group_vec)
        {
          if (str_equals_any(kind.c_str(), 7, "hexbin", "imshow", "contour", "contourf", "heatmap", "quiver", "shade"))
            {
              current_series_vec.push_back(current_series_group);
            }
          else
            {
              for (const auto &current_series_group_child : current_series_group->children())
                {
                  if ((kind == "marginalheatmap" && current_series_group_child->localName() == "series_heatmap") ||
                      kind != "marginalheatmap")
                    current_series_vec.push_back(current_series_group_child);
                }
            }
        }

      for (auto &current_series : current_series_vec)
        {
          std::string x_key_string = "x";
          std::string y_key_string = "y";
          std::string z_key_string = "z";
          std::string z_key;

          if (kind == "contour" || kind == "contourf")
            {
              x_key_string = "px";
              y_key_string = "py";
              z_key_string = "pz";
            }

          if (!current_series->hasAttribute(x_key_string) || !current_series->hasAttribute(y_key_string))
            {
              mindiff = DBL_MAX;
              continue;
            }
          auto x_key = static_cast<std::string>(current_series->getAttribute(x_key_string));
          auto y_key = static_cast<std::string>(current_series->getAttribute(y_key_string));

          if (!str_equals_any(kind.c_str(), 7, "hexbin", "quiver", "line", "scatter", "stem", "stairs", "shade"))
            {
              if (!current_series->hasAttribute(z_key_string))
                {
                  mindiff = DBL_MAX;
                  continue;
                }
              z_key = static_cast<std::string>(current_series->getAttribute(z_key_string));
            }

          std::vector<double> x_series_vec;
          std::vector<double> y_series_vec;

          if (is_vertical)
            {
              x_series_vec = GRM::get<std::vector<double>>((*context)[y_key]);
              y_series_vec = GRM::get<std::vector<double>>((*context)[x_key]);
            }
          else
            {
              x_series_vec = GRM::get<std::vector<double>>((*context)[x_key]);
              y_series_vec = GRM::get<std::vector<double>>((*context)[y_key]);
            }
          std::vector<double> z_series_vec;

          if (str_equals_any(kind.c_str(), 5, "heatmap", "marginalheatmap", "contour", "imshow", "contourf"))
            {
              z_series_vec = GRM::get<std::vector<double>>((*context)[z_key]);
              z_length = z_series_vec.size();
            }

          for (i = 0; i < x_series_vec.size(); i++)
            {
              if ((x_series_vec[i] < x_range_min || x_series_vec[i] > x_range_max || y_series_vec[i] < y_range_min ||
                   y_series_vec[i] > y_range_max) &&
                  !str_equals_any(kind.c_str(), 6, "heatmap", "marginalheatmap", "contour", "imshow", "contourf",
                                  "quiver"))
                {
                  continue;
                }
              x_px = x_series_vec[i];
              y_px = y_series_vec[i];
              gr_wctondc(&x_px, &y_px);
              x_px = (x_px * max_width_height);
              y_px = (height - y_px * max_width_height);
              diff = sqrt((x_px - mouse_x) * (x_px - mouse_x) + (y_px - mouse_y) * (y_px - mouse_y));
              if (diff < mindiff && diff <= 50)
                {
                  mindiff = diff;
                  info->x = x_series_vec[i];
                  info->y = y_series_vec[i];
                  info->x_px = (int)x_px;
                  info->y_px = (int)y_px;
                  if (num_labels > series_i)
                    {
                      info->label = labels[series_i].c_str();
                    }
                  else
                    {
                      info->label = "";
                    }
                }
              else if (str_equals_any(kind.c_str(), 6, "heatmap", "marginalheatmap", "contour", "imshow", "contourf",
                                      "quiver"))
                {
                  static char output[50];
                  double num;
                  x_length = x_series_vec.size();
                  y_length = y_series_vec.size();
                  double x_0 = x_series_vec[0], x_end = x_series_vec[x_length - 1], y_0 = y_series_vec[0],
                         y_end = y_series_vec[y_length - 1];
                  double x_step, y_step, x_series_idx, y_series_idx;
                  double *u_series, *v_series;

                  if (kind == "imshow") x_0 = x_min, x_end = x_max, y_0 = y_min, y_end = y_max;

                  gr_wctondc(&x_0, &y_0);
                  gr_wctondc(&x_end, &y_end);
                  x_0 = x_0 * max_width_height;
                  x_end = x_end * max_width_height;
                  y_0 = height - y_0 * max_width_height;
                  y_end = height - y_end * max_width_height;

                  x_step = (x_end - x_0) / x_length;
                  y_step = (y_end - y_0) / y_length;
                  if (kind == "quiver")
                    {
                      auto u_key = static_cast<std::string>(current_series->getAttribute("u"));
                      auto v_key = static_cast<std::string>(current_series->getAttribute("v"));

                      auto u_series_vec = GRM::get<std::vector<double>>((*context)[u_key]);
                      auto v_series_vec = GRM::get<std::vector<double>>((*context)[v_key]);
                      u_series = &u_series_vec[0];
                      v_series = &v_series_vec[0];
                    }

                  mindiff = 0;
                  x_series_idx = (mouse_x - x_0) / x_step;
                  y_series_idx = (mouse_y - y_0) / y_step;
                  if (x_series_idx < 0 || x_series_idx >= x_length || y_series_idx < 0 || y_series_idx >= y_length)
                    {
                      mindiff = DBL_MAX;
                      break;
                    }
                  if (kind == "quiver")
                    {
                      info->xlabel = "u";
                      info->ylabel = "v";
                      info->x = u_series[(int)(y_series_idx)*x_length + (int)(x_series_idx)];
                      info->y = v_series[(int)(y_series_idx)*x_length + (int)(x_series_idx)];
                    }
                  else
                    {
                      info->x = x_series_vec[(int)x_series_idx];
                      info->y = y_series_vec[(int)y_series_idx];
                    }
                  info->x_px = mouse_x;
                  info->y_px = mouse_y;

                  if (kind == "quiver")
                    {
                      info->label = "";
                    }
                  else
                    {
                      num = z_series_vec[(int)y_series_idx * x_length + (int)x_series_idx];
                      snprintf(output, 50, "%f", num);
                      info->label = output;
                    }
                }
            }
          ++series_i;
        }
    }
  gr_restorestate();
  if (mindiff == DBL_MAX)
    {
      info->x_px = -1;
      info->y_px = -1;
      info->x = 0;
      info->y = 0;
      info->label = "";
    }
  return info;
}
