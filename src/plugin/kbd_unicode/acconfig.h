#ifndef UNICODE_KEYB_CONFIG_H
#define UNICODE_KEYB_CONFIG_H

#define HAVE_UNICODE_KEYB 2

#if DOSEMU_VERSION_CODE < VERSION_OF(1,1,1,1)
  #error "Sorry, wrong DOSEMU version for keyboard unicode plugin, please upgrade"
#endif

@TOP@

/* Define this if you have the XKB extension */
#undef HAVE_XKB

@BOTTOM@

#endif /* UNICODE_KEYB_CONFIG_H */
