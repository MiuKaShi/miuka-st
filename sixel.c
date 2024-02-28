// originally written by kmiya@cluti (https://github.com/saitoha/sixel/blob/master/fromsixel.c)
// Licensed under the terms of the GNU General Public License v3 or later.

#include <stdlib.h>
#include <string.h>  /* memcpy */

#include "st.h"
#include "win.h"

#define DECSIXEL_PARAMVALUE_MAX 65535
#define DECSIXEL_WIDTH_MAX 4096
#define DECSIXEL_HEIGHT_MAX 4096

#define SIXEL_RGB(r, g, b) ( 0xff000000 | (r) << 16 | (g) << 8 | (b))
#define SIXEL_PALVAL(n,a,m) (((n) * (a) + ((m) / 2)) / (m))
#define SIXEL_XRGB(r,g,b) SIXEL_RGB(SIXEL_PALVAL(r, 255, 100), SIXEL_PALVAL(g, 255, 100), SIXEL_PALVAL(b, 255, 100))

#define MIN3(a,b,c) MIN((a), MIN((b), (c)))
#define FUNCF(k) ((3000*lum - a * MAX(-30, MIN3(30, k-90, 270-k)))/1176)

/* stolen from https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB_alternative */
static sixel_color_t
hls_to_rgb(int32_t hue, int32_t lum, int32_t sat){
	int32_t a = sat * MIN(lum, 100 - lum);
	int32_t kr = ((240 + hue) % 360);
	int32_t kg = ((120 + hue) % 360);
	int32_t kb = ((0 + hue) % 360);
	return SIXEL_RGB(FUNCF(kr), FUNCF(kg), FUNCF(kb));
}

static int
sixel_image_init(sixel_image_t *image)
{
	image->w = 1;
	image->h = 1;
	image->tw = DIVCEIL(image->w, win.cw);
	image->th = DIVCEIL(image->h, win.ch);
	image->data = (sixel_color_no_t *)malloc(sizeof(sixel_color_no_t));
	if (image->data == NULL) {
		return -1;
	}
	*(image->data) = 0;

	xloadsixelcols(image);

	return 0;
}

static int
image_buffer_resize(sixel_image_t *image, unsigned int width, unsigned int height)
{
	size_t size;
	sixel_color_no_t *alt_buffer;
	int n;
	int min_height;

	size = (size_t)(width * height) * sizeof(sixel_color_no_t);
	alt_buffer = (sixel_color_no_t *)malloc(size);
	if (alt_buffer == NULL) {
		/* free source image */
		free(image->data);
		image->data = NULL;
		return -1;
	}

	min_height = MIN(height, image->h);
	if (width > image->w) {  /* if width is extended */
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + (width) * n,
				image->data + image->w * n,
				(size_t)image->w * sizeof(sixel_color_no_t));
			/* fill extended area with background color */
			memset(alt_buffer + width * n + image->w,
				0,
				(size_t)(width - image->w) * sizeof(sixel_color_no_t));
		}
	} else {
		for (n = 0; n < min_height; ++n) {
			/* copy from source image */
			memcpy(alt_buffer + width * n,
				image->data + image->w * n,
				(size_t)width * sizeof(sixel_color_no_t));
		}
	}

	if (height > image->h) {  /* if height is extended */
		/* fill extended area with background color */
		memset(alt_buffer + width * image->h,
			0,
			(size_t)(width * (height - image->h)) * sizeof(sixel_color_no_t));
	}
	/* free source image */
	free(image->data);

	image->data = alt_buffer;
	image->w = width;
	image->h = height;
	image->tw = DIVCEIL(image->w, win.cw);
	image->th = DIVCEIL(image->h, win.ch);

	return 0;
}

static void
sixel_image_deinit(sixel_image_t *image)
{
	free(image->data);
	image->data = NULL;
}

int
sixel_parser_init(sixel_state_t *st)
{
	int status = -1;

	st->state = PS_DECSIXEL;
	st->pos_x = 0;
	st->pos_y = 0;
	st->max_x = 0;
	st->max_y = 0;
	st->attributed_pan = 2;
	st->attributed_pad = 1;
	st->attributed_ph = 0;
	st->attributed_pv = 0;
	st->repeat_count = 1;
	st->color_index = 16;
	st->nparams = 0;
	st->param = 0;

	/* buffer initialization */
	status = sixel_image_init(&st->im);

	return status;
}

unsigned char *
sixel_parser_finalize(sixel_state_t *st)
{
	int status;
	sixel_image_t *image = &st->im;
	unsigned int x, y;
	sixel_color_no_t *src;
	unsigned char *dst;
	sixel_color_t color;

	unsigned int sx, sy;

	st->max_x = MAX(st->max_x+1, st->attributed_ph);
	st->max_y = MAX(st->max_y+1, st->attributed_pv);

	sx = DIVCEIL(st->max_x, win.cw) * win.cw;
	sy = DIVCEIL(st->max_y, win.ch) * win.ch;

	if (image->w > sx || image->h > sy) {
		status = image_buffer_resize(image, sx, st->max_y);
		if (status < 0) {
			return 0;
		}
	}

	int size_pixels = st->im.w * st->im.h * sizeof(sixel_color_t);
	unsigned char *pixels = (unsigned char *)malloc(size_pixels);
	if (!pixels){
		return 0;
	}

	src = st->im.data;
	dst = pixels;
	for (y = 0; y < st->im.h; ++y) {
		for (x = 0; x < st->im.w; ++x) {
			if (dst + 4 > pixels + size_pixels) {
				free(pixels);
				return 0;
			}
			color = st->im.palette[*src++];
			*dst++ = color >> 24 & 0xff;   /* a */
			*dst++ = color >> 16 & 0xff;   /* r */
			*dst++ = color >> 8 & 0xff;    /* g */
			*dst++ = color >> 0 & 0xff;    /* b */
		}
	}

	return pixels;
}

/* convert sixel data into indexed pixel bytes and palette data */
int
sixel_parser_parse(sixel_state_t *st, unsigned char *p, int cx)
{
	int status = -1;
	int n;
	int i;
	unsigned int x, y;
	int bits;
	int sixel_vertical_mask;
	unsigned int sx;
	unsigned int sy;
	int c;
	int pos;
	sixel_image_t *image = &st->im;

	/* Ensure image is no wider than terminal */
	unsigned int max_width = MIN(DECSIXEL_WIDTH_MAX, win.tw - (cx*win.cw));
	unsigned int max_height = DECSIXEL_HEIGHT_MAX;

	if (!image->data)
		return -1;

start:

	switch (st->state) {
	case PS_ESC:
		return -1;

	case PS_DECSIXEL:
		switch (*p) {
		case '\x1b':
			st->state = PS_ESC;
			break;
		case '!':
			st->param = 0;
			st->nparams = 0;
			st->state = PS_DECGRI;
			break;
		case '"':
			st->param = 0;
			st->nparams = 0;
			st->state = PS_DECGRA;
			break;
		case '#':
			st->param = 0;
			st->nparams = 0;
			st->state = PS_DECGCI;
			break;
		case '$':
			/* DECGCR Graphics Carriage Return */
			st->pos_x = 0;
			break;
		case '-':
			/* DECGNL Graphics Next Line */
			st->pos_x = 0;
			if (st->pos_y + 6 + 5 < max_height)
				st->pos_y += 6;
			else
				st->pos_y = max_height + 1;
			break;
		default:
			if (*p >= '?' && *p <= '~') {  /* sixel characters */
				if ((image->w < (st->pos_x + st->repeat_count) || image->h < (st->pos_y + 6))
						&& image->w < max_width && image->h < max_height) {
					sx = image->w * 2;
					sy = image->h * 2;
					while (sx < (st->pos_x + st->repeat_count) || sy < (st->pos_y + 6)) {
						sx *= 2;
						sy *= 2;
					}

					sx = MIN(sx, max_width);
					sy = MIN(sy, max_width);

					status = image_buffer_resize(image, sx,sy);
					if (status < 0)
						return status;
				}

				if (st->pos_x + st->repeat_count > image->w)
					st->repeat_count = image->w - st->pos_x;

				if (st->repeat_count > 0 && st->pos_y < image->h + 5) {
					bits = *p - '?';
					if (bits != 0) {
						sixel_vertical_mask = 0x01;
						if (st->repeat_count <= 1) {
							for (i = 0; i < 6; i++) {
								if ((bits & sixel_vertical_mask) != 0) {
									pos = image->w * (st->pos_y + i) + st->pos_x;
									image->data[pos] = st->color_index;
									if (st->max_x < st->pos_x)
										st->max_x = st->pos_x;
									if (st->max_y < (st->pos_y + i))
										st->max_y = st->pos_y + i;
								}
								sixel_vertical_mask <<= 1;
							}
						} else {
							/* st->repeat_count > 1 */
							for (i = 0; i < 6; i++) {
								if ((bits & sixel_vertical_mask) != 0) {
									c = sixel_vertical_mask << 1;
									for (n = 1; (i + n) < 6; n++) {
										if ((bits & c) == 0)
											break;
										c <<= 1;
									}
									for (y = st->pos_y + i; y < st->pos_y + i + n; ++y) {
										for (x = st->pos_x; x < st->pos_x + st->repeat_count; ++x)
											image->data[image->w * y + x] = st->color_index;
									}
									if (st->max_x < (st->pos_x + st->repeat_count - 1))
										st->max_x = st->pos_x + st->repeat_count - 1;
									if (st->max_y < (st->pos_y + i + n - 1))
										st->max_y = st->pos_y + i + n - 1;
									i += (n - 1);
									sixel_vertical_mask <<= (n - 1);
								}
								sixel_vertical_mask <<= 1;
							}
						}
					}
				}
				if (st->repeat_count > 0)
					st->pos_x += st->repeat_count;
				st->repeat_count = 1;
			}
			break;
		}
		break;

	case PS_DECGRI:
		/* DECGRI Graphics Repeat Introducer ! Pn Ch */
		switch (*p) {
		case '\x1b':
			st->state = PS_ESC;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			st->param = st->param * 10 + *p - '0';
			if (st->param > DECSIXEL_PARAMVALUE_MAX)
				st->param = DECSIXEL_PARAMVALUE_MAX;
			break;
		default:
			st->repeat_count = st->param;
			if (st->repeat_count == 0)
				st->repeat_count = 1;
			st->state = PS_DECSIXEL;
			st->param = 0;
			st->nparams = 0;
			goto start;
			break;
		}
		break;

	case PS_DECGRA:
		/* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
		switch (*p) {
		case '\x1b':
			st->state = PS_ESC;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			st->param = st->param * 10 + *p - '0';
			if (st->param > DECSIXEL_PARAMVALUE_MAX)
				st->param = DECSIXEL_PARAMVALUE_MAX;
			break;
		case ';':
			if (st->nparams < DECSIXEL_PARAMS_MAX)
				st->params[st->nparams++] = st->param;
			st->param = 0;
			break;
		default:
			if (st->nparams < DECSIXEL_PARAMS_MAX)
				st->params[st->nparams++] = st->param;
			if (st->nparams > 0)
				st->attributed_pad = st->params[0];
			if (st->nparams > 1)
				st->attributed_pan = st->params[1];
			if (st->nparams > 2 && st->params[2] > 0)
				st->attributed_ph = st->params[2];
			if (st->nparams > 3 && st->params[3] > 0)
				st->attributed_pv = st->params[3];

			st->attributed_pan = MAX(1, st->attributed_pan);
			st->attributed_pad = MAX(1, st->attributed_pad);

			if (image->w < st->attributed_ph ||
					image->h < st->attributed_pv) {

				sx = MAX(image->w, st->attributed_ph);
				sy = MAX(image->h, st->attributed_pv);

				sx = DIVCEIL(sx, win.cw) * win.cw;
				sy = DIVCEIL(sy, win.ch) * win.ch;

				sx = MIN(max_width, sx);
				sy = MIN(max_height, sy);

				status = image_buffer_resize(image, sx, sy);
				if (status < 0)
					return status;
			}
			st->state = PS_DECSIXEL;
			st->param = 0;
			st->nparams = 0;
			goto start;
		}
		break;

	case PS_DECGCI:
		/* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
		switch (*p) {
		case '\x1b':
			st->state = PS_ESC;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			st->param = st->param * 10 + *p - '0';
			if (st->param > DECSIXEL_PARAMVALUE_MAX)
				st->param = DECSIXEL_PARAMVALUE_MAX;
			break;
		case ';':
			if (st->nparams < DECSIXEL_PARAMS_MAX)
				st->params[st->nparams++] = st->param;
			st->param = 0;
			break;
		default:
			st->state = PS_DECSIXEL;
			if (st->nparams < DECSIXEL_PARAMS_MAX)
				st->params[st->nparams++] = st->param;
			st->param = 0;

			if (st->nparams > 0) {
				st->color_index = 1 + st->params[0];  /* offset 1(background color) added */
				if (st->color_index >= DECSIXEL_PALETTE_MAX)
					st->color_index = DECSIXEL_PALETTE_MAX - 1;
			}

			if (st->nparams > 4) {
				if (st->params[1] == 1) {
					/* HLS */
					image->palette[st->color_index]
						= hls_to_rgb(MIN(st->params[2], 360),
									 MIN(st->params[3], 100),
									 MIN(st->params[4], 100));
				} else if (st->params[1] == 2) {
					/* RGB */
					image->palette[st->color_index]
						= SIXEL_XRGB(MIN(st->params[2], 100),
									 MIN(st->params[3], 100),
									 MIN(st->params[4], 100));
				}
			}
			goto start;
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

void
sixel_parser_deinit(sixel_state_t *st)
{
	if (st)
		sixel_image_deinit(&st->im);
}
