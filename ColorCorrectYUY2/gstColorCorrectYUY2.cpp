/**
* SECTION:element-gamma
*
* Performs YUY2 color correction on a video stream.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch-1.0 videotestsrc ! color-correct-yuy2 ! autovideosink
* ]| This pipeline will make the image use the correct colorimetry
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstColorCorrectYUY2.h"
#include <string.h>
#include <math.h>

#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC(gamma_debug);
#define GST_CAT_DEFAULT gamma_debug

/* GstColorCorrectYUY2 properties */
enum
{
	PROP_0,
	PROP_GAMMA
	/* FILL ME */
};

#define DEFAULT_PROP_GAMMA  1

static GstStaticPadTemplate gst_color_correct_yuy2_src_template =
GST_STATIC_PAD_TEMPLATE("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{YUY2}"))
);

static GstStaticPadTemplate gst_color_correct_yuy2_sink_template =
GST_STATIC_PAD_TEMPLATE("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{YUY2}"))
);

static void gst_color_correct_yuy2_set_property(GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec);
static void gst_color_correct_yuy2_get_property(GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec);

static gboolean gst_color_correct_yuy2_set_info(GstVideoFilter * vfilter, GstCaps * incaps,
	GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_color_correct_yuy2_transform_frame_ip(GstVideoFilter * vfilter,
	GstVideoFrame * frame);
static void gst_color_correct_yuy2_before_transform(GstBaseTransform * transform,
	GstBuffer * buf);

static void gst_color_correct_yuy2_calculate_tables(GstColorCorrectYUY2 * gamma);

G_DEFINE_TYPE(GstColorCorrectYUY2, gst_color_correct_yuy2, GST_TYPE_VIDEO_FILTER);

static void
gst_color_correct_yuy2_class_init(GstColorCorrectYUY2Class * g_class)
{
	GObjectClass *gobject_class = (GObjectClass *)g_class;
	GstElementClass *gstelement_class = (GstElementClass *)g_class;
	GstBaseTransformClass *trans_class = (GstBaseTransformClass *)g_class;
	GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *)g_class;

	GST_DEBUG_CATEGORY_INIT(gamma_debug, "gamma", 0, "gamma");

	gobject_class->set_property = gst_color_correct_yuy2_set_property;
	gobject_class->get_property = gst_color_correct_yuy2_get_property;

	g_object_class_install_property(gobject_class, PROP_GAMMA,
		g_param_spec_double("gamma", "Gamma", "gamma",
			0.01, 10, DEFAULT_PROP_GAMMA,
			(GParamFlags)(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));

	gst_element_class_set_static_metadata(gstelement_class,
		"Video gamma correction", "Filter/Effect/Video",
		"Adjusts gamma on a video stream", "Arwed v. Merkatz <v.merkatz@gmx.net");

	gst_element_class_add_static_pad_template(gstelement_class,
		&gst_color_correct_yuy2_sink_template);
	gst_element_class_add_static_pad_template(gstelement_class,
		&gst_color_correct_yuy2_src_template);

	trans_class->before_transform =
		GST_DEBUG_FUNCPTR(gst_color_correct_yuy2_before_transform);
	trans_class->transform_ip_on_passthrough = FALSE;

	vfilter_class->set_info = GST_DEBUG_FUNCPTR(gst_color_correct_yuy2_set_info);
	vfilter_class->transform_frame_ip =
		GST_DEBUG_FUNCPTR(gst_color_correct_yuy2_transform_frame_ip);
}

static void
gst_color_correct_yuy2_init(GstColorCorrectYUY2 * gamma)
{
	/* properties */
	gamma->gamma = DEFAULT_PROP_GAMMA;
	gst_color_correct_yuy2_calculate_tables(gamma);
}

static void
gst_color_correct_yuy2_set_property(GObject * object, guint prop_id, const GValue * value,
	GParamSpec * pspec)
{
	GstColorCorrectYUY2 *gamma = GST_GAMMA(object);

	switch (prop_id) {
	case PROP_GAMMA: {
		gdouble val = g_value_get_double(value);

		GST_DEBUG_OBJECT(gamma, "Changing gamma from %lf to %lf", gamma->gamma,
			val);
		GST_OBJECT_LOCK(gamma);
		gamma->gamma = val;
		GST_OBJECT_UNLOCK(gamma);
		gst_color_correct_yuy2_calculate_tables(gamma);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gst_color_correct_yuy2_get_property(GObject * object, guint prop_id, GValue * value,
	GParamSpec * pspec)
{
	GstColorCorrectYUY2 *gamma = GST_GAMMA(object);

	switch (prop_id) {
	case PROP_GAMMA:
		g_value_set_double(value, gamma->gamma);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gst_color_correct_yuy2_calculate_tables(GstColorCorrectYUY2 * gamma)
{
	gint n;
	gdouble val;
	gdouble exp;
	gboolean passthrough = FALSE;

	GST_OBJECT_LOCK(gamma);
	exp = 1.0 / gamma->gamma;
	for (n = 0; n < 256; n++) {
		//val = n / 255.0;
		val = n;
		//val = pow(val, exp);
		//val = 255.0 * val;
		//val = 1.164 * (n - 16);
		//val = 0.8588 * (n + 16);

		// Scale [0,255] -> [16,235]
		val = ((235.0 - 16.0) / 255.0) * n + 16.0;
		gamma->y_table[n] = (guint8)floor(val + 0.5);

		// Scale [0,255] -> [16, 240]
		val = ((240.0 - 16.0) / 255.0) * n + 16.0;
		gamma->uv_table[n] = (guint8)floor(val + 0.5);
	}
	GST_OBJECT_UNLOCK(gamma);

	gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(gamma), passthrough);
}

static void
gst_color_correct_yuy2_planar_yuv_ip(GstColorCorrectYUY2 * gamma, GstVideoFrame * frame)
{
	gint i, j, height;
	gint width, stride, row_wrap;
	const guint8 *table = gamma->y_table;
	guint8 *data;

	data = GST_VIDEO_FRAME_COMP_DATA(frame, 0);
	stride = GST_VIDEO_FRAME_COMP_STRIDE(frame, 0);
	width = GST_VIDEO_FRAME_COMP_WIDTH(frame, 0);
	height = GST_VIDEO_FRAME_COMP_HEIGHT(frame, 0);
	row_wrap = stride - width;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			*data = table[*data];
			data++;
		}
		data += row_wrap;
	}
}

static void
gst_color_correct_yuy2_packed_yuv_ip(GstColorCorrectYUY2 * gamma, GstVideoFrame * frame)
{
	gint i, j, height;
	gint width, stride, row_wrap;
	gint pixel_stride;
	const guint8 *y_table = gamma->y_table;
	const guint8 *uv_table = gamma->uv_table;
	guint8 *data;

	data = GST_VIDEO_FRAME_COMP_DATA(frame, 0);
	stride = GST_VIDEO_FRAME_COMP_STRIDE(frame, 0);
	width = GST_VIDEO_FRAME_COMP_WIDTH(frame, 0);
	height = GST_VIDEO_FRAME_COMP_HEIGHT(frame, 0);
	pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE(frame, 0);
	row_wrap = stride - pixel_stride * width;
	//pixel_stride = 1;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			*data = y_table[*data];
			data++;
			*data = uv_table[*data];
			data++;
			//data += pixel_stride;
		}
		data += row_wrap;
	}
}

static const int cog_ycbcr_to_rgb_matrix_8bit_sdtv[] = {
	298, 0, 409, -57068,
	298, -100, -208, 34707,
	298, 516, 0, -70870,
};

static const gint cog_rgb_to_ycbcr_matrix_8bit_sdtv[] = {
	66, 129, 25, 4096,
	-38, -74, 112, 32768,
	112, -94, -18, 32768,
};

#define APPLY_MATRIX(m,o,v1,v2,v3) ((m[o*4] * v1 + m[o*4+1] * v2 + m[o*4+2] * v3 + m[o*4+3]) >> 8)

static void
gst_color_correct_yuy2_packed_rgb_ip(GstColorCorrectYUY2 * gamma, GstVideoFrame * frame)
{
	gint i, j, height;
	gint width, stride, row_wrap;
	gint pixel_stride;
	const guint8 *table = gamma->y_table;
	gint offsets[3];
	gint r, g, b;
	gint y, u, v;
	guint8 *data;

	data = (guint8*)GST_VIDEO_FRAME_PLANE_DATA(frame, 0);
	stride = GST_VIDEO_FRAME_PLANE_STRIDE(frame, 0);
	width = GST_VIDEO_FRAME_COMP_WIDTH(frame, 0);
	height = GST_VIDEO_FRAME_COMP_HEIGHT(frame, 0);

	offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET(frame, 0);
	offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET(frame, 1);
	offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET(frame, 2);

	pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE(frame, 0);
	row_wrap = stride - pixel_stride * width;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			r = data[offsets[0]];
			g = data[offsets[1]];
			b = data[offsets[2]];

			y = APPLY_MATRIX(cog_rgb_to_ycbcr_matrix_8bit_sdtv, 0, r, g, b);
			u = APPLY_MATRIX(cog_rgb_to_ycbcr_matrix_8bit_sdtv, 1, r, g, b);
			v = APPLY_MATRIX(cog_rgb_to_ycbcr_matrix_8bit_sdtv, 2, r, g, b);

			y = table[CLAMP(y, 0, 255)];
			r = APPLY_MATRIX(cog_ycbcr_to_rgb_matrix_8bit_sdtv, 0, y, u, v);
			g = APPLY_MATRIX(cog_ycbcr_to_rgb_matrix_8bit_sdtv, 1, y, u, v);
			b = APPLY_MATRIX(cog_ycbcr_to_rgb_matrix_8bit_sdtv, 2, y, u, v);

			data[offsets[0]] = CLAMP(r, 0, 255);
			data[offsets[1]] = CLAMP(g, 0, 255);
			data[offsets[2]] = CLAMP(b, 0, 255);
			data += pixel_stride;
		}
		data += row_wrap;
	}
}

static gboolean
gst_color_correct_yuy2_set_info(GstVideoFilter * vfilter, GstCaps * incaps,
	GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
	GstColorCorrectYUY2 *gamma = GST_GAMMA(vfilter);

	GST_DEBUG_OBJECT(gamma,
		"setting caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps,
		outcaps);

	switch (GST_VIDEO_INFO_FORMAT(in_info)) {
	case GST_VIDEO_FORMAT_I420:
	case GST_VIDEO_FORMAT_YV12:
	case GST_VIDEO_FORMAT_Y41B:
	case GST_VIDEO_FORMAT_Y42B:
	case GST_VIDEO_FORMAT_Y444:
	case GST_VIDEO_FORMAT_NV12:
	case GST_VIDEO_FORMAT_NV21:
		gamma->process = gst_color_correct_yuy2_planar_yuv_ip;
		break;
	case GST_VIDEO_FORMAT_YUY2:
	case GST_VIDEO_FORMAT_UYVY:
	case GST_VIDEO_FORMAT_AYUV:
	case GST_VIDEO_FORMAT_YVYU:
		gamma->process = gst_color_correct_yuy2_packed_yuv_ip;
		break;
	case GST_VIDEO_FORMAT_ARGB:
	case GST_VIDEO_FORMAT_ABGR:
	case GST_VIDEO_FORMAT_RGBA:
	case GST_VIDEO_FORMAT_BGRA:
	case GST_VIDEO_FORMAT_xRGB:
	case GST_VIDEO_FORMAT_xBGR:
	case GST_VIDEO_FORMAT_RGBx:
	case GST_VIDEO_FORMAT_BGRx:
	case GST_VIDEO_FORMAT_RGB:
	case GST_VIDEO_FORMAT_BGR:
		gamma->process = gst_color_correct_yuy2_packed_rgb_ip;
		break;
	default:
		goto invalid_caps;
		break;
	}
	return TRUE;

	/* ERRORS */
invalid_caps:
	{
		GST_ERROR_OBJECT(gamma, "Invalid caps: %" GST_PTR_FORMAT, incaps);
		return FALSE;
	}
}

static void
gst_color_correct_yuy2_before_transform(GstBaseTransform * base, GstBuffer * outbuf)
{
	GstColorCorrectYUY2 *gamma = GST_GAMMA(base);
	GstClockTime timestamp, stream_time;

	timestamp = GST_BUFFER_TIMESTAMP(outbuf);
	stream_time =
		gst_segment_to_stream_time(&base->segment, GST_FORMAT_TIME, timestamp);

	GST_DEBUG_OBJECT(gamma, "sync to %" GST_TIME_FORMAT,
		GST_TIME_ARGS(timestamp));

	if (GST_CLOCK_TIME_IS_VALID(stream_time))
		gst_object_sync_values(GST_OBJECT(gamma), stream_time);
}

static GstFlowReturn
gst_color_correct_yuy2_transform_frame_ip(GstVideoFilter * vfilter, GstVideoFrame * frame)
{
	GstColorCorrectYUY2 *gamma = GST_GAMMA(vfilter);

	if (!gamma->process)
		goto not_negotiated;

	GST_OBJECT_LOCK(gamma);
	gamma->process(gamma, frame);
	GST_OBJECT_UNLOCK(gamma);

	return GST_FLOW_OK;

	/* ERRORS */
not_negotiated:
	{
		GST_ERROR_OBJECT(gamma, "Not negotiated yet");
		return GST_FLOW_NOT_NEGOTIATED;
	}
}

/* entry point to initialize the plug-in
* initialize the plug-in itself
* register the element factories and other features
*/
gboolean
gst_color_correct_yuy2_plugin_init(GstPlugin * plugin)
{
	/* debug category for fltering log messages
	*
	* exchange the string 'Template opencvtextoverlay' with your description
	*/
	GST_DEBUG_CATEGORY_INIT(gst_color_correct_yuy2_debug, "color-correct-yuy2",
		0, "Template color-correct-yuy2");

	return gst_element_register(plugin, "color-correct-yuy2", GST_RANK_NONE,
		GST_TYPE_COLOR_CORRECT_YUY2);
}

#ifndef PACKAGE
#define PACKAGE "example"
#endif

GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	color-correct-yuy2,
	"Quick Color Correction for YUY2",
	gst_color_correct_yuy2_plugin_init,
	"0",
	"Proprietary",
	"Me",
	"me"
)