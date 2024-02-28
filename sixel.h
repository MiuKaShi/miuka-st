#include <stdint.h>

#define DECSIXEL_PARAMS_MAX 16
#define DECSIXEL_PALETTE_MAX 1024

typedef uint16_t sixel_color_no_t;
typedef uint32_t sixel_color_t;

typedef struct sixel_image_buffer {
	sixel_color_no_t * data;
	unsigned int w; /* width */
	unsigned int h; /* height */
	unsigned int tw; /* width in terminal chars */
	unsigned int th; /* height in terminal chars */
	sixel_color_t palette[DECSIXEL_PALETTE_MAX];
} sixel_image_t;

typedef enum parse_state {
	PS_ESC        = 1,  /* ESC */
	PS_DECSIXEL   = 2,  /* DECSIXEL body part ", $, -, ? ... ~ */
	PS_DECGRA     = 3,  /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
	PS_DECGRI     = 4,  /* DECGRI Graphics Repeat Introducer ! Pn Ch */
	PS_DECGCI     = 5,  /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
} parse_state_t;

typedef struct parser_context {
	parse_state_t state;
	unsigned int pos_x;
	unsigned int pos_y;
	unsigned int max_x;
	unsigned int max_y;
	unsigned int attributed_pan;
	unsigned int attributed_pad;
	unsigned int attributed_ph;
	unsigned int attributed_pv;
	int repeat_count;
	unsigned int color_index;
	unsigned int param;
	unsigned int nparams;
	unsigned int params[DECSIXEL_PARAMS_MAX];
	sixel_image_t im; /* image */
} sixel_state_t;

int sixel_parser_init(sixel_state_t * st);
int sixel_parser_parse(sixel_state_t * st, unsigned char *p, int cx);
unsigned char * sixel_parser_finalize(sixel_state_t * st);
void sixel_parser_deinit(sixel_state_t * st);
