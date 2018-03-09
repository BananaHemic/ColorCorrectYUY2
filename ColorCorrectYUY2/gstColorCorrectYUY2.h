#pragma once
#ifndef GST_COLOR_CORRECT_YUY2
#define GST_COLOR_CORRECT_YUY2

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_STATIC(gst_color_correct_yuy2_debug);
#define GST_CAT_DEFAULT gst_color_correct_yuy2_debug

#define GST_TYPE_COLOR_CORRECT_YUY2 \
  (gst_color_correct_yuy2_get_type())
#define GST_GAMMA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_COLOR_CORRECT_YUY2,GstColorCorrectYUY2))
#define GST_GAMMA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_COLOR_CORRECT_YUY2,GstColorCorrectYUY2Class))
#define GST_IS_GAMMA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_COLOR_CORRECT_YUY2))
#define GST_IS_GAMMA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_COLOR_CORRECT_YUY2))

typedef struct _GstColorCorrectYUY2 GstColorCorrectYUY2;
typedef struct _GstColorCorrectYUY2Class GstColorCorrectYUY2Class;

/**
* GstColorCorrectYUY2:
*
* Opaque data structure.
*/
struct _GstColorCorrectYUY2
{
	GstVideoFilter videofilter;

	/* < private > */
	/* properties */
	gdouble gamma;

	/* tables */
	guint8 y_table[256];
	guint8 uv_table[256];

	void(*process) (GstColorCorrectYUY2 *gamma, GstVideoFrame *frame);
};

struct _GstColorCorrectYUY2Class
{
	GstVideoFilterClass parent_class;
};

GType gst_color_correct_yuy2_get_type(void);

G_END_DECLS

#endif /* GST_COLOR_CORRECT_YUY2 */
