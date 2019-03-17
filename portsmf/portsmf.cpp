#include <stdlib.h>
#include <string.h>
#include <portsmf/allegro.h>
#include "Gwion.hpp"

static Type t_midifileev;
#define TYPE(o)  *(m_uint*)(o->data + o_midiev_type)
#define START(o) *(m_float*)(o->data + o_midiev_start)
#define PITCH(o) *(m_float*)(o->data + o_midiev_pitch)
#define LOUD(o)  *(m_float*)(o->data + o_midiev_loud)
#define END(o)   *(m_float*)(o->data + o_midiev_end)
#define DUR(o)   *(m_float*)(o->data + o_midiev_dur)
#define SEQ(o)   *(Alg_seq**)(o->data + o_midifile_seq)
m_int o_midifile_seq;
m_int o_midiev_type;
m_int o_midiev_start;
m_int o_midiev_end;
m_int o_midiev_dur;
m_int o_midiev_pitch;
m_int o_midiev_loud;
//static void ctor(M_Object o, VM_Shred shred);
//static void dtor(M_Object o, VM_Shred shred);
static CTOR(ctor);
static DTOR(dtor);

extern "C"
{
MFUN(midifile_open);
MFUN(midifile_tracks);
MFUN(midifile_track_len);
MFUN(midifile_event);
MFUN(midifile_add_track);
MFUN(midifile_add_note);
MFUN(midifile_write);
GWION_IMPORT(portsmf) {
  t_midifileev = gwi_mk_type(gwi, "MidiFileEv", SZ_INT, t_event);
  CHECK_BB(gwi_class_ini(gwi, t_midifileev, NULL, NULL))
	gwi_item_ini(gwi,"int", "type");
  o_midiev_type = gwi_item_end(gwi,   ae_flag_const, NULL);
  CHECK_BB(o_midiev_type);
	gwi_item_ini(gwi,"float", "pitch");
  o_midiev_pitch = gwi_item_end(gwi, ae_flag_const, NULL);
  CHECK_BB(o_midiev_pitch);
	gwi_item_ini(gwi,"float", "loud");
  o_midiev_loud  = gwi_item_end(gwi,  ae_flag_const, NULL);
  CHECK_BB(o_midiev_loud);
	gwi_item_ini(gwi,"float", "start");
  o_midiev_start = gwi_item_end(gwi, ae_flag_const, NULL);
  CHECK_BB(o_midiev_start);
	gwi_item_ini(gwi,"float", "end");
  o_midiev_end   = gwi_item_end(gwi,   ae_flag_const, NULL);
  CHECK_BB(o_midiev_start);
	gwi_item_ini(gwi,"float", "dur");
  o_midiev_dur   = gwi_item_end(gwi,   ae_flag_const, NULL);
  CHECK_BB(o_midiev_dur);
  CHECK_BB(gwi_class_end(gwi))

  const Type t_midifile = gwi_mk_type(gwi, "MidiFile",  SZ_INT, t_object);
  CHECK_BB(gwi_class_ini(gwi, t_midifile, ctor, dtor))
	gwi_item_ini(gwi,"int", "@seq");
  o_midifile_seq = gwi_item_end(gwi, ae_flag_member, NULL);
  CHECK_BB(o_midifile_seq);
  gwi_func_ini(gwi, "void", "open", (m_uint)midifile_open);
    gwi_func_arg(gwi, "string", "filename");
    gwi_func_arg(gwi, "int", "smf");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))
  gwi_func_ini(gwi, "int", "tracks", (m_uint)midifile_tracks);
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))

  gwi_func_ini(gwi, "int", "len", (m_uint)midifile_track_len);
    gwi_func_arg(gwi, "int", "track");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))

  gwi_func_ini(gwi, "MidiFileEv", "event", (m_uint)midifile_event);
    gwi_func_arg(gwi, "int", "track");
    gwi_func_arg(gwi, "int", "event_number");
//    gwi_func_arg(gwi, "MidiFileEv", "event");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))

  gwi_func_ini(gwi, "void", "add_track", (m_uint)midifile_add_track);
    gwi_func_arg(gwi, "int", "number");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))

  gwi_func_ini(gwi, "int", "add_note", (m_uint)midifile_add_note);
    gwi_func_arg(gwi, "int", "track");
    gwi_func_arg(gwi, "MidiFileEv", "note");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))

  gwi_func_ini(gwi, "void", "write", (m_uint)midifile_write);
    gwi_func_arg(gwi, "string", "filename");
  CHECK_BB(gwi_func_end(gwi, ae_flag_member))
  CHECK_BB(gwi_class_end(gwi))
  return 1;
}
}


static CTOR(ctor) { SEQ(o) = new Alg_seq(); }

static DTOR(dtor) { delete SEQ(o); }

MFUN(midifile_open)
{
  delete SEQ(o);
  SEQ(o) = new Alg_seq(STRING(*(M_Object*)MEM(SZ_INT)), *(m_uint*)MEM(SZ_INT), 0);
  Alg_seq* seq = SEQ(o);
  seq->convert_to_seconds();
}

MFUN(midifile_tracks)
{
  Alg_seq* seq = SEQ(o);
  *(m_uint*)RETURN = seq->tracks();
}

MFUN(midifile_add_track)
{
  Alg_seq* seq = SEQ(o);
  seq->add_track(*(m_uint*)MEM(SZ_INT));
}
MFUN(midifile_track_len)
{
  Alg_seq* seq = SEQ(o);
  *(m_uint*)RETURN = seq->track(*(m_uint*)MEM(SZ_INT))->length();

}
MFUN(midifile_event)
{
  Alg_seq* seq = SEQ(o);
  m_uint track = *(m_uint*)MEM(SZ_INT);
  m_uint n     = *(m_uint*)MEM(SZ_INT*2);
//  if(track < 0 || track >= seq->tracks())
//    exit(12);
  Alg_track* tr = seq->track(track);
//  M_Object obj = *(M_Object*)MEM(SZ_INT*3);
  M_Object obj = new_object(shred, t_midifileev);
  if(n < 0 || n >= tr->length())
      TYPE(obj) = 'e'; // error
  else
  {
    Alg_event* ev = tr[0][*(m_uint*)MEM(SZ_INT*2)];
    TYPE(obj) = ev->get_type();
    if(ev->get_type() == 'n')
    {
      PITCH(obj) = ev->get_pitch();
      LOUD(obj)  = ev->get_loud();
      START(obj) = ev->get_start_time() * shred->info->vm->bbq->si->sr;
      END(obj)   = ev->get_end_time()   * shred->info->vm->bbq->si->sr;
      DUR(obj)   = ev->get_duration()   * shred->info->vm->bbq->si->sr;
    }
  }
  *(m_uint*)RETURN = (m_uint)obj;
}

MFUN(midifile_add_note)
{
  Alg_seq* seq = SEQ(o);
  M_Object obj = *(M_Object*)MEM(SZ_INT);
  if(TYPE(obj) != 'n')
  {
    err_msg(0, "not a note.");
    shred->info->me = NULL;
    return;
  }
  Alg_note* ev = new Alg_note();
  ev->pitch = PITCH(obj);
  ev->loud  = LOUD(obj);
  ev->dur   = DUR(obj);
  seq->add_event(ev, *(m_uint*)MEM(SZ_INT*2));
  Alg_track* tr = seq->track(*(m_uint*)MEM(SZ_INT*2));
  tr->set_start_time(ev, START(obj));
}

MFUN(midifile_write)
{
  Alg_seq* seq = SEQ(o);
  seq->write(STRING(*(M_Object*)MEM(SZ_INT)));

}
