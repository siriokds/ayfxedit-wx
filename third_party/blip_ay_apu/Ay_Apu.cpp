// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Ay_Apu.h"

/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

// Emulation inaccuracies:
// * Noise isn't run when not in use
// * Changes to envelope and noise periods are delayed until next reload
// * Super-sonic tone should attenuate output to about 60%, not 50%

// Tones above this frequency are treated as disabled tone at half volume.
// Power of two is more efficient (avoids division).
//
// ayfxedit-wx history: this shortcut was removed for a while (see git log)
// on the theory that the single hard digital step it produces, combined
// with Blip_Buffer's own DC-blocking, was causing an audible click in
// effects that toggle a channel's tone period between an audible value and
// 0 every frame. Letting the true (~clock/32, tens of kHz) tone run
// through Blip_Buffer unshortcut instead turned out worse: real chip
// silicon settles a tone that fast to a near-DC level proportional to
// volume (its analog output can't fully swing at that rate) -- it does
// NOT keep ringing as an audible tone -- but Blip_Buffer's synthesis of
// the *actual* tens-of-kHz square wave aliases back down into an audible
// mid/high-frequency ripple instead of disappearing above Nyquist,
// producing a persistent whistle, confirmed by ear and by inspecting the
// rendered samples (see the ayfxedit-wx commit that restored this). So the
// shortcut is back -- it's the physically-accurate behavior, not just a
// perf hack -- and the original click is instead addressed by smoothing
// frame-boundary discontinuities in AudioEngine::renderFrames() rather
// than by letting Ay_Apu synthesize a tone that shouldn't be audible.
static unsigned const inaudible_freq = 16384;

static int const period_factor = 16;

// Per-channel DAC tables, derived with MAME's build_single_table() resistor-
// ladder model (ay8910.cpp) from its published measurements: ay8910_param
// (Matthew Westcott's ZX Spectrum measurements, RL=2000, "channel off" at
// level 0 excludes r_up from the divider) for ay_3_8910, and
// ym2149_param/ym2149_param_env (RL=1000, no "channel off" special case —
// a real, audible AY-vs-YM difference: YM2149's level 0 is not silent) for
// ym2149. Each table is independently normalised so its own loudest step is
// amp_range; see the ayfxedit-wx commit that added chip_type() for the
// derivation script.
static byte const ay_amp_table [16] =
{
	 62,  65,  66,  67,  69,  72,  75,  84,
	 89, 105, 121, 139, 163, 187, 221, 255,
};

static byte const ym_amp_table [16] =
{
	149, 150, 150, 151, 152, 152, 154, 156,
	159, 163, 169, 175, 188, 202, 226, 255,
};

// ym2149's envelope generator has 32 steps instead of ay_3_8910's 16.
static byte const ym_env_table [32] =
{
	149, 149, 149, 150, 150, 150, 150, 151,
	151, 152, 152, 152, 153, 154, 155, 156,
	157, 159, 161, 163, 165, 169, 172, 175,
	181, 188, 194, 202, 213, 226, 240, 255,
};

static byte const modes [8] =
{
#define MODE( a0,a1, b0,b1, c0,c1 ) \
		(a0 | a1<<1 | b0<<2 | b1<<3 | c0<<4 | c1<<5)
	MODE( 1,0, 1,0, 1,0 ),
	MODE( 1,0, 0,0, 0,0 ),
	MODE( 1,0, 0,1, 1,0 ),
	MODE( 1,0, 1,1, 1,1 ),
	MODE( 0,1, 0,1, 0,1 ),
	MODE( 0,1, 1,1, 1,1 ),
	MODE( 0,1, 1,0, 0,1 ),
	MODE( 0,1, 0,0, 0,0 ),
};

// Builds env.modes (the 8 upper envelope waveforms, values already passed
// through the chip's own volume table) plus env.steps/reset_pos/repeat_pos,
// for whichever chip chip_type_ currently selects.
void Ay_Apu::build_env_modes_()
{
	byte const* table = ay_amp_table;
	int steps = 16;
	if ( chip_type_ == Chip_Type::ym2149 )
	{
		table = ym_env_table;
		steps = 32;
	}
	env.steps      = steps;
	env.reset_pos  = -3 * steps;
	env.repeat_pos = -2 * steps;

	for ( int m = 8; m--; )
	{
		byte* out = env.modes [m];
		int flags = modes [m];
		for ( int x = 3; --x >= 0; )
		{
			int amp = flags & 1;
			int end = flags >> 1 & 1;
			int step = end - amp;
			amp *= steps - 1;
			for ( int y = steps; --y >= 0; )
			{
				*out++ = table [amp];
				amp += step;
			}
			flags >>= 2;
		}
	}
}

void Ay_Apu::chip_type( Chip_Type type )
{
	chip_type_ = type;
	build_env_modes_();
}

Ay_Apu::Ay_Apu()
{
	build_env_modes_();
	output( 0 );
	volume( 1.0 );
	reset();
}

void Ay_Apu::reset()
{
	last_time   = 0;
	noise.delay = 0;
	noise.lfsr  = 1;

	osc_t* osc = &oscs [osc_count];
	do
	{
		osc--;
		osc->period   = period_factor;
		osc->delay    = 0;
		osc->last_amp = 0;
		osc->phase    = 0;
	}
	while ( osc != oscs );

	for ( int i = sizeof regs; --i >= 0; )
		regs [i] = 0;
	regs [7] = 0xFF;
	write_data_( 13, 0 );
}

void Ay_Apu::write_data_( int addr, int data )
{
	assert( (unsigned) addr < reg_count );

	if ( (unsigned) addr >= 14 )
	{
		#ifdef debug_printf
			debug_printf( "Wrote to I/O port %02X\n", (int) addr );
		#endif
	}

	// envelope mode
	if ( addr == 13 )
	{
		if ( !(data & 8) ) // convert modes 0-7 to proper equivalents
			data = (data & 4) ? 15 : 9;
		env.wave = env.modes [(data - 7) & 0x07];
		env.pos = env.reset_pos;
		env.delay = 0; // will get set to envelope period in run_until()
	}
	regs [addr] = data;

	// handle period changes accurately
	int i = addr >> 1;
	if ( i < osc_count )
	{
		blip_time_t period = (regs [i * 2 + 1] & 0x0F) * (0x100L * period_factor) +
				regs [i * 2] * period_factor;
		if ( !period )
			period = period_factor;

		// adjust time of next timer expiration based on change in period
		osc_t& osc = oscs [i];
		if ( (osc.delay += period - osc.period) < 0 )
			osc.delay = 0;
		osc.period = period;
	}

	// TODO: same as above for envelope timer, and it also has a divide by two after it
}

static int const noise_off = 0x08;
static int const tone_off  = 0x01;

void Ay_Apu::run_until( blip_time_t final_end_time )
{
	require( final_end_time >= last_time );

	// noise period and initial values
	blip_time_t const noise_period_factor = period_factor * 2; // verified
	blip_time_t noise_period = (regs [6] & 0x1F) * noise_period_factor;
	if ( !noise_period )
		noise_period = noise_period_factor;
	blip_time_t const old_noise_delay = noise.delay;
	uint32_t const old_noise_lfsr = noise.lfsr;

	// envelope period. ym2149's envelope counter has twice the steps
	// (env.steps) of ay_3_8910's and counts up twice as fast, so each step
	// gets half the period, keeping the same overall envelope frequency for
	// a given register period value.
	blip_time_t const env_period_factor =
			chip_type_ == Chip_Type::ym2149 ? period_factor : period_factor * 2;
	blip_time_t env_period = (regs [12] * 0x100L + regs [11]) * env_period_factor;
	if ( !env_period )
		env_period = env_period_factor; // same as period 1 on my AY chip
	if ( !env.delay )
		env.delay = env_period;

	// run each osc separately
	for ( int index = 0; index < osc_count; index++ )
	{
		osc_t* const osc = &oscs [index];
		int osc_mode = regs [7] >> index;

		// output
		Blip_Buffer* const osc_output = osc->output;
		if ( !osc_output )
			continue;
		osc_output->set_modified();

		// period
		int half_vol = 0;
		blip_time_t inaudible_period = (uint32_t) (osc_output->clock_rate() +
				inaudible_freq) / (inaudible_freq * 2);
		if ( osc->period <= inaudible_period && !(osc_mode & tone_off) )
		{
			half_vol = 1; // Actually around 60%, but 50% is close enough
			osc_mode |= tone_off;
		}

		// envelope
		blip_time_t start_time = last_time;
		blip_time_t end_time   = final_end_time;
		int const vol_mode = regs [0x08 + index];
		byte const* const fixed_vol_table =
				chip_type_ == Chip_Type::ym2149 ? ym_amp_table : ay_amp_table;
		int volume = fixed_vol_table [vol_mode & 0x0F] >> half_vol;
		int osc_env_pos = env.pos;
		if ( vol_mode & 0x10 )
		{
			volume = env.wave [osc_env_pos] >> half_vol;
			// use envelope only if it's a repeating wave or a ramp that hasn't finished
			if ( !(regs [13] & 1) || osc_env_pos < -32 )
			{
				end_time = start_time + env.delay;
				if ( end_time >= final_end_time )
					end_time = final_end_time;

				//if ( !(regs [12] | regs [11]) )
				//  debug_printf( "Used envelope period 0\n" );
			}
			else if ( !volume )
			{
				osc_mode = noise_off | tone_off;
			}
		}
		else if ( !volume )
		{
			osc_mode = noise_off | tone_off;
		}

		// tone time
		blip_time_t const period = osc->period;
		blip_time_t time = start_time + osc->delay;
		if ( osc_mode & tone_off ) // maintain tone's phase when off
		{
			int32_t count = (final_end_time - time + period - 1) / period;
			time += count * period;
			osc->phase ^= count & 1;
		}

		// noise time
		blip_time_t ntime = final_end_time;
		uint32_t noise_lfsr = 1;
		if ( !(osc_mode & noise_off) )
		{
			ntime = start_time + old_noise_delay;
			noise_lfsr = old_noise_lfsr;
			//if ( (regs [6] & 0x1F) == 0 )
			//  debug_printf( "Used noise period 0\n" );
		}

		// The following efficiently handles several cases (least demanding first):
		// * Tone, noise, and envelope disabled, where channel acts as 4-bit DAC
		// * Just tone or just noise, envelope disabled
		// * Envelope controlling tone and/or noise
		// * Tone and noise disabled, envelope enabled with high frequency
		// * Tone and noise together
		// * Tone and noise together with envelope

		// This loop only runs one iteration if envelope is disabled. If envelope
		// is being used as a waveform (tone and noise disabled), this loop will
		// still be reasonably efficient since the bulk of it will be skipped.
		while ( 1 )
		{
			// current amplitude
			int amp = 0;
			if ( (osc_mode | osc->phase) & 1 & (osc_mode >> 3 | noise_lfsr) )
				amp = volume;
			{
				int delta = amp - osc->last_amp;
				if ( delta )
				{
					osc->last_amp = amp;
					synth_.offset( start_time, delta, osc_output );
				}
			}

			// Run wave and noise interleved with each catching up to the other.
			// If one or both are disabled, their "current time" will be past end time,
			// so there will be no significant performance hit.
			if ( ntime < end_time || time < end_time )
			{
				// Since amplitude was updated above, delta will always be +/- volume,
				// so we can avoid using last_amp every time to calculate the delta.
				int delta = amp * 2 - volume;
				int delta_non_zero = delta != 0;
				int phase = osc->phase | (osc_mode & tone_off); assert( tone_off == 0x01 );
				do
				{
					// run noise
					blip_time_t end = end_time;
					if ( end_time > time ) end = time;
					if ( phase & delta_non_zero )
					{
						while ( ntime <= end ) // must advance *past* time to avoid hang
						{
							int changed = noise_lfsr + 1;
							noise_lfsr = (uMinus(noise_lfsr & 1) & 0x12000) ^ (noise_lfsr >> 1);
							if ( changed & 2 )
							{
								delta = -delta;
								synth_.offset( ntime, delta, osc_output );
							}
							ntime += noise_period;
						}
					}
					else
					{
						// 20 or more noise periods on average for some music
						int32_t remain = end - ntime;
						int32_t count = remain / noise_period;
						if ( remain >= 0 )
							ntime += noise_period + count * noise_period;
					}

					// run tone
					end = end_time;
					if ( end_time > ntime ) end = ntime;
					if ( noise_lfsr & delta_non_zero )
					{
						while ( time < end )
						{
							delta = -delta;
							synth_.offset( time, delta, osc_output );
							time += period;
							//phase ^= 1;
						}
						//assert( phase == (delta > 0) );
						phase = unsigned (-delta) >> (CHAR_BIT * sizeof (unsigned) - 1);
						// (delta > 0)
					}
					else
					{
						// loop usually runs less than once
						//SUB_CASE_COUNTER( (time < end) * (end - time + period - 1) / period );

						while ( time < end )
						{
							time += period;
							phase ^= 1;
						}
					}
				}
				while ( time < end_time || ntime < end_time );

				osc->last_amp = (delta + volume) >> 1;
				if ( !(osc_mode & tone_off) )
					osc->phase = phase;
			}

			if ( end_time >= final_end_time )
				break; // breaks first time when envelope is disabled

			// next envelope step
			if ( ++osc_env_pos >= 0 )
				osc_env_pos = env.repeat_pos;
			volume = env.wave [osc_env_pos] >> half_vol;

			start_time = end_time;
			end_time += env_period;
			if ( end_time > final_end_time )
				end_time = final_end_time;
		}
		osc->delay = time - final_end_time;

		if ( !(osc_mode & noise_off) )
		{
			noise.delay = ntime - final_end_time;
			noise.lfsr = noise_lfsr;
		}
	}

	// TODO: optimized saw wave envelope?

	// maintain envelope phase
	blip_time_t remain = final_end_time - last_time - env.delay;
	if ( remain >= 0 )
	{
		int32_t count = (remain + env_period) / env_period;
		env.pos += count;
		if ( env.pos >= 0 )
			env.pos = (env.pos & (2 * env.steps - 1)) + env.repeat_pos;
		remain -= count * env_period;
		assert( -remain <= env_period );
	}
	env.delay = -remain;
	assert( env.delay > 0 );
	assert( env.pos < 0 );

	last_time = final_end_time;
}
