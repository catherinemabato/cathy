/* 
 * video/terminal.h 
 * By Mark D. Rejhon
 *
 * Contains character sets as lookup tables for on-screen approximations
 * of the 256 IBM characters.  Used by the terminal.c module.
 * This also contains a lookup table for converting integers 0 to 255 to
 * a string very quickly.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

/* The following is the standard default latin character set for VT100 
 * display.
 */
/* cp437 -> iso-8859-1 */
static unsigned char charset_latin[256] =
{
  " \326@hdcs.#o0/+;M*"
  "><|H\266\247+|^v><--^v"
  " !\"#$%&'()*+,-./"
  "0123456789:;<=>?"
  "@ABCDEFGHIJKLMNO"
  "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno"
  "pqrstuvwxyz{|}~^"
  "\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
  "\311\346\306\364\366\362\373\371\377\326\334\242\243\245\120\146"
  "\341\355\363\372\361\321\252\272\277\055\254\275\274\241\253\273"
  ":%&|{{{..{I.'''."
  "``+}-+}}`.**}=**"
  "+*+``..**'.#_][~"
  "a\337\254\266{\363\265t\330\364\326\363o\370En"
  "=\261><()\367=\260\267\267%\140\262= "
};

/* cp850 -> iso-8859-1 */
static unsigned char charset_latin1[256] =
{
  " \326@hdcs.#o0/+;M*"
  "><|H\266\247+|^v><--^v"
  " !\"#$%&'()*+,-./"
  "0123456789:;<=>?"
  "@ABCDEFGHIJKLMNO"
  "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno"
  "pqrstuvwxyz{|}~^"
  "\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
  "\311\346\306\364\366\362\373\371\377\326\334\242\243\245\120\146"
  "\341\355\363\372\361\321\252\272\277\256\254\275\274\241\253\273"
  ":%&|{\301\302\300\251{I.'\242\245."
  "``+}-+\343\303`.**}=*\244"
  "\360\320\312\313\310.\315\316\317'.#_\246\314~"
  "\323\337\324\322\365\325\265\376\336\332\333\331\375\335En"
  "=\261><()\367=\260\267\267%\140\262= "
};

/* cp852 -> iso-8859-2 */
static unsigned char charset_latin2[256] =
{
  " \326@hdcs.#o0/+;M*"
  "><|H\266\247+|^v><--^v"
  " !\"#$%&'()*+,-./"
  "0123456789:;<=>?"
  "@ABCDEFGHIJKLMNO"
  "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno"
  "pqrstuvwxyz{|}~^"
  "\307\374\351\342\344\371\346\347\263\353\325\365\356\254\304\306"
  "\311\305\345\364\366\245\265\246\266\326\334\242\273\243\327\350"
  "\341\355\363\372\241\261\256\276\312\352\254\274\310\272\253\273"
  ":%&|{\301\302\314\252{I.'\257\277."
  "``+}-+\303\343`.**}=*\244"
  "\360\320\317\313\357\316\315\316\354'.#_\336\331~"
  "\323\337\324\321\361\362\251\271\300\332\340\333\375\335\376n"
  "=\261><()\367=\260\267\267\373\330\370= "
};


#if 0 /* To be removed */
/* The following is an almost-full IBM character set, with some control codes.
 * This only works at the console (in IBM printing mode), over a serial line
 * to a DOS-based ANSI terminal (such as Telix), or in a specially-compiled 
 * ANSI Xterm.  See the next character set for something that will work with 
 * a normal xterm or color-xterm.
 */
static unsigned char charset_fullibm[256] =
{
   32,  1,  2,  3,  4,  5,  6,249,178,'o',177,162,157,251,239,'*',
   62, 60, 18, 19, 20, 21, 22, 23, 24, 25, 26,174, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,'^',
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254, 32
};
#endif

/* The following is the IBM character set minus control codes, mainly 
 * for use in ordinary rxvt or xterm's being used with the VGA font
 * which is included with DOSEMU 0.53.
 */
static unsigned char charset_ibm[256] =
{
  32,148,153,'h','d','c','s',249,178,'o',177,162,157,251,239,'*',
  62, 60,173,186,227,159,254,168,244,245,175,174,192,205,'^','v',
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 
  80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 
  96, 97, 98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,'^',
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254, 32
};

#if 0
/* This is the regular color attributes lookup table, for ANSI colors.
 * It is a one-to-one lookup table.
 */
static unsigned char attrset_normal[256] =
{
   0,  1,  2,  3,  4,  5,  6,   7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

/* This is the color attribute lookup table for color rxvts and xterms.
 * Since there is no brightness difference between dim and bold characters
 * (they just use different font appearances), and if the foreground and
 * background colors are the same, the text is still invisible even if the 
 * foreground color is bold!  In these cases, white or black is used instead.
 */
static unsigned char attrset_xterm[256] =
{
   0,  1,  2,  3,  4,  5,  6,   7,  7,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 31, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 47, 43, 44, 45, 46, 47, 
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 63, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 79, 77, 78, 79, 
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 95, 94, 95, 
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,111,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,112,
  128,129,130,131,132,133,134,135,135,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,159,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,175,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,191,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,207,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,223,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,239,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,240
};

static char *fg_color_string[8] =
{
  "30","34","32","36","31","35","33","37"
};

static char *bg_color_string[8] =
{
  "40","44","42","46","41","45","43","47"
};

static char *num_string[256] = 
{
  "0","1","2","3","4","5","6","7","8","9",
  "10","11","12","13","14","15","16","17","18","19",
  "20","21","22","23","24","25","26","27","28","29",
  "30","31","32","33","34","35","36","37","38","39",
  "40","41","42","43","44","45","46","47","48","49",
  "50","51","52","53","54","55","56","57","58","59",
  "60","61","62","63","64","65","66","67","68","69",
  "70","71","72","73","74","75","76","77","78","79",
  "80","81","82","83","84","85","86","87","88","89",
  "90","91","92","93","94","95","96","97","98","99",
  "100","101","102","103","104","105","106","107","108","109",
  "110","111","112","113","114","115","116","117","118","119",
  "120","121","122","123","124","125","126","127","128","129",
  "130","131","132","133","134","135","136","137","138","139",
  "140","141","142","143","144","145","146","147","148","149",
  "150","151","152","153","154","155","156","157","158","159",
  "160","161","162","163","164","165","166","167","168","169",
  "170","171","172","173","174","175","176","177","178","179",
  "180","181","182","183","184","185","186","187","188","189",
  "190","191","192","193","194","195","196","197","198","199",
  "200","201","202","203","204","205","206","207","208","209",
  "210","211","212","213","214","215","216","217","218","219",
  "220","221","222","223","224","225","226","227","228","229",
  "230","231","232","233","234","235","236","237","238","239",
  "240","241","242","243","244","245","246","247","248","249",
  "250","251","252","253","254","255"
};
#endif

#endif
