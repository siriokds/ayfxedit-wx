// AY-3-8910 sound chip emulator

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
//
// MODIFIED from upstream (ayfxedit-wx, 2026-08-20): added Chip_Type
// selection (ay_3_8910 / ym2149) so the same emulator can model either
// chip's DAC characteristics — different per-step amplitude tables (derived
// with MAME's per-channel resistor-ladder model, ay8910.cpp's
// build_single_table, from its published ay8910_param/ym2149_param/
// ym2149_param_env measurements) and, for ym2149, a 32-step envelope
// instead of 16. See Ay_Apu.cpp for the added chip_type()/build_env_modes_().
#ifndef AY_APU_H
#define AY_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Ay_Apu {
public:
	enum class Chip_Type { ay_3_8910, ym2149 };

	// Selects which chip's DAC/envelope characteristics to emulate
	// (ZX Spectrum: ay_3_8910; most MSX: ym2149). Call before writing any
	// registers; changing it later re-derives the envelope table but does
	// not retroactively fix up amplitudes already in flight.
	void chip_type( Chip_Type );

	// Set buffer to generate all sound into, or disable sound if NULL
	void output( Blip_Buffer* );

	// Reset sound chip
	void reset();

	// Write to register at specified time
	static const unsigned int reg_count = 16;
	void write( blip_time_t time, int addr, int data );

	// Run sound to specified time, end current time frame, then start a new
	// time frame at time 0. Time frames have no effect on emulation and each
	// can be whatever length is convenient.
	void end_frame( blip_time_t length );

// Additional features

	// Set sound output of specific oscillator to buffer, where index is
	// 0, 1, or 2. If buffer is NULL, the specified oscillator is muted.
	static const int osc_count = 3;
	void osc_output( int index, Blip_Buffer* );

	// Set overall volume (default is 1.0)
	void volume( double );

	// Set treble equalization (see documentation)
	void treble_eq( blip_eq_t const& );

public:
	Ay_Apu();
	typedef unsigned char byte;
private:
	struct osc_t
	{
		blip_time_t period;
		blip_time_t delay;
		short last_amp;
		short phase;
		Blip_Buffer* output;
	} oscs [osc_count];
	blip_time_t last_time;
	byte regs [reg_count];

	struct {
		blip_time_t delay;
		uint32_t lfsr;
	} noise;

	// Sized for the largest case (ym2149: 3 segments of 32); ay_3_8910 only
	// uses the first 48 (3 segments of 16) of each row.
	struct {
		blip_time_t delay;
		byte const* wave;
		int pos;
		int steps;        // 16 (ay_3_8910) or 32 (ym2149)
		int reset_pos;     // -3 * steps
		int repeat_pos;    // -2 * steps
		byte modes [8] [3*32];
	} env;

	Chip_Type chip_type_ = Chip_Type::ay_3_8910;

	void run_until( blip_time_t );
	void write_data_( int addr, int data );
	void build_env_modes_();
public:
	static const int amp_range = 255;
	Blip_Synth<blip_good_quality,1> synth_;
};

inline void Ay_Apu::volume( double v ) { synth_.volume( 0.7 / osc_count / amp_range * v ); }

inline void Ay_Apu::treble_eq( blip_eq_t const& eq ) { synth_.treble_eq( eq ); }

inline void Ay_Apu::write( blip_time_t time, int addr, int data )
{
	run_until( time );
	write_data_( addr, data );
}

inline void Ay_Apu::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = buf;
}

inline void Ay_Apu::output( Blip_Buffer* buf )
{
	osc_output( 0, buf );
	osc_output( 1, buf );
	osc_output( 2, buf );
}

inline void Ay_Apu::end_frame( blip_time_t time )
{
	if ( time > last_time )
		run_until( time );

	assert( last_time >= time );
	last_time -= time;
}

#endif
