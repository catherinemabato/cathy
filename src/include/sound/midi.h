/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "sound/sound.h"

#define MIDI_W_PREFERRED 1
#define MIDI_W_PCM 2

enum SynthType { ST_ANY, ST_GM, ST_MT32, ST_MAX };

#ifdef __cplusplus
struct midi_out_plugin : public pcm_plugin_base {
  midi_out_plugin(const char *nm, const char *lnm, void *gcfg, void *op,
      void *clo, int w, void *wr, void *stp, void *r, int st, int flgs) :
    pcm_plugin_base(nm, lnm, gcfg, op, clo, NULL, stp, flgs, w),
    write(wr),
    run(r),
    stype(st)
    {}
#else
struct midi_out_plugin {
  pcm_plugin_base;
#endif
  void (*write)(unsigned char);
  void (*run)(void);
  enum SynthType stype;
};

#ifdef __cplusplus
struct midi_in_plugin : public pcm_plugin_base {
  midi_in_plugin(const char *nm, void *op, void *clo) :
    pcm_plugin_base(nm, NULL, NULL, op, clo, NULL, NULL, 0, 0)
    {}
#else
struct midi_in_plugin {
  pcm_plugin_base;
#endif
};

extern void midi_write(unsigned char val);
extern void midi_init(void);
extern void midi_done(void);
extern void midi_stop(void);
extern void midi_timer(void);
extern void midi_put_data(unsigned char *buf, size_t size);
extern int midi_get_data_byte(unsigned char *buf);
extern int midi_register_output_plugin(const struct midi_out_plugin *plugin);
extern int midi_register_input_plugin(const struct midi_in_plugin *plugin);
extern int midi_set_synth_type(enum SynthType st);
extern enum SynthType midi_get_synth_type(void);
extern int midi_set_synth_type_from_string(const char *stype);
