/**
 * OpenAL cross platform audio library
 * Copyright (C) 2018 by Raul Herraiz.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>

#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "alu.h"
#include "filters/defs.h"

#include "alcomplex.h"

#define HIL_SIZE 1024
#define OVERSAMP (1<<2)

#define HIL_STEP     (HIL_SIZE / OVERSAMP)
#define FIFO_LATENCY (HIL_STEP * (OVERSAMP-1))


typedef struct ALfshifterState {
    DERIVE_FROM_TYPE(ALeffectState);

    /* Effect parameters */
    ALsizei  count;
    ALdouble frac_freq;
    ALdouble inc;
    ALdouble ld_sign;

    /*Effects buffers*/ 
    ALfloat   InFIFO[HIL_SIZE];
    ALcomplex OutFIFO[HIL_SIZE];
    ALcomplex OutputAccum[2*HIL_SIZE];
    ALcomplex Analytic[HIL_SIZE];
    ALcomplex Outdata[BUFFERSIZE];

    alignas(16) ALfloat BufferOut[BUFFERSIZE];

    /* Effect gains for each output channel */
    ALfloat CurrentGains[MAX_OUTPUT_CHANNELS];
    ALfloat TargetGains[MAX_OUTPUT_CHANNELS];
} ALfshifterState;

static ALvoid ALfshifterState_Destruct(ALfshifterState *state);
static ALboolean ALfshifterState_deviceUpdate(ALfshifterState *state, ALCdevice *device);
static ALvoid ALfshifterState_update(ALfshifterState *state, const ALCcontext *context, const ALeffectslot *slot, const ALeffectProps *props);
static ALvoid ALfshifterState_process(ALfshifterState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels);
DECLARE_DEFAULT_ALLOCATORS(ALfshifterState)

DEFINE_ALEFFECTSTATE_VTABLE(ALfshifterState);

/* Define a Hann window, used to filter the HIL input and output. */
alignas(16) static ALdouble HannWindow[HIL_SIZE];

static void InitHannWindow(void)
{
    ALsizei i;

    /* Create lookup table of the Hann window for the desired size, i.e. HIL_SIZE */
    for(i = 0;i < HIL_SIZE>>1;i++)
    {
        ALdouble val = sin(M_PI * (ALdouble)i / (ALdouble)(HIL_SIZE-1));
        HannWindow[i] = HannWindow[HIL_SIZE-1-i] = val * val;
    }
}

static alonce_flag HannInitOnce = AL_ONCE_FLAG_INIT;

static void ALfshifterState_Construct(ALfshifterState *state)
{
    ALeffectState_Construct(STATIC_CAST(ALeffectState, state));
    SET_VTABLE2(ALfshifterState, ALeffectState, state);

    alcall_once(&HannInitOnce, InitHannWindow);
}

static ALvoid ALfshifterState_Destruct(ALfshifterState *state)
{
    ALeffectState_Destruct(STATIC_CAST(ALeffectState,state));
}

static ALboolean ALfshifterState_deviceUpdate(ALfshifterState *state, ALCdevice *UNUSED(device))
{
    /* (Re-)initializing parameters and clear the buffers. */
    state->count     = FIFO_LATENCY;
    state->frac_freq = 0.0;
    state->inc       = 0.0;
    state->ld_sign   = 1.0;

    memset(state->InFIFO,      0, sizeof(state->InFIFO));
    memset(state->OutFIFO,     0, sizeof(state->OutFIFO));
    memset(state->OutputAccum, 0, sizeof(state->OutputAccum));
    memset(state->Analytic,    0, sizeof(state->Analytic));

    memset(state->CurrentGains, 0, sizeof(state->CurrentGains));
    memset(state->TargetGains,  0, sizeof(state->TargetGains));

    return AL_TRUE;
}

static ALvoid ALfshifterState_update(ALfshifterState *state, const ALCcontext *context, const ALeffectslot *slot, const ALeffectProps *props)
{
    const ALCdevice *device = context->Device;
    ALfloat coeffs[MAX_AMBI_COEFFS];

    state->frac_freq = props->Fshifter.Frequency/(ALdouble)device->Frequency;

    switch(props->Fshifter.Left_direction)
    {
        case AL_FREQUENCY_SHIFTER_DIRECTION_DOWN:
            state->ld_sign = -1.0;
            break;

        case AL_FREQUENCY_SHIFTER_DIRECTION_UP:
            state->ld_sign =  1.0;
            break;

        case AL_FREQUENCY_SHIFTER_DIRECTION_OFF:
            state->inc = 0.0;
            state->frac_freq = 0.0;
            break;
    }

    CalcAngleCoeffs(0.0f, 0.0f, 0.0f, coeffs);
    ComputeDryPanGains(&device->Dry, coeffs, slot->Params.Gain, state->TargetGains);
}

static ALvoid ALfshifterState_process(ALfshifterState *state, ALsizei SamplesToDo, const ALfloat (*restrict SamplesIn)[BUFFERSIZE], ALfloat (*restrict SamplesOut)[BUFFERSIZE], ALsizei NumChannels)
{
    ALfloat *restrict BufferOut = state->BufferOut;
    ALsizei i, k;

    for(i = 0; i < SamplesToDo;i++)
    {
        /* Fill FIFO buffer with samples data */
        state->InFIFO[state->count] = SamplesIn[0][i];
        state->Outdata[i]  = state->OutFIFO[state->count-FIFO_LATENCY];
        state->count++;

        /* Check whether FIFO buffer is filled */
        if(state->count >= HIL_SIZE)
        {
            state->count = FIFO_LATENCY;

            /* Real signal windowing and store in Analytic buffer */
            for(k = 0;k < HIL_SIZE;k++)
            {
                state->Analytic[k].Real = state->InFIFO[k] * HannWindow[k];
                state->Analytic[k].Imag = 0.0;
            }

            /* Processing signal by Discrete Hilbert Transform (analytical
             * signal).
             */
            hilbert(HIL_SIZE, state->Analytic);

            /* Windowing and add to output accumulator */
            for(k = 0;k < HIL_SIZE;k++)
            {
                state->OutputAccum[k].Real += 2.0/OVERSAMP*HannWindow[k]*state->Analytic[k].Real;
                state->OutputAccum[k].Imag += 2.0/OVERSAMP*HannWindow[k]*state->Analytic[k].Imag;
            }
            for(k = 0;k < HIL_STEP;k++)
                state->OutFIFO[k] = state->OutputAccum[k];

            /* Shift accumulator */
            memmove(state->OutputAccum, state->OutputAccum+HIL_STEP, HIL_SIZE*sizeof(ALcomplex));

            /* Move input FIFO */
            for(k = 0;k < FIFO_LATENCY;k++)
                state->InFIFO[k] = state->InFIFO[k + HIL_STEP];
        }
    }

    /* Process frequency shifter using the analytic signal obtained. */
    for(k = 0;k < SamplesToDo;k++)
    {
        ALdouble phase;

        if(state->inc >= 1.0)
            state->inc -= 1.0;

        phase = 2.0*M_PI * state->inc;
        BufferOut[k] = (ALfloat)(state->Outdata[k].Real*cos(phase) +
                                 state->ld_sign*state->Outdata[k].Imag*sin(phase));

        state->inc += state->frac_freq;
    }

    /* Now, mix the processed sound data to the output. */
    MixSamples(BufferOut, NumChannels, SamplesOut, state->CurrentGains, state->TargetGains,
               maxi(SamplesToDo, 512), 0, SamplesToDo);
}

typedef struct FshifterStateFactory {
    DERIVE_FROM_TYPE(EffectStateFactory);
} FshifterStateFactory;

static ALeffectState *FshifterStateFactory_create(FshifterStateFactory *UNUSED(factory))
{
    ALfshifterState *state;

    NEW_OBJ0(state, ALfshifterState)();
    if(!state) return NULL;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_EFFECTSTATEFACTORY_VTABLE(FshifterStateFactory);

EffectStateFactory *FshifterStateFactory_getFactory(void)
{
    static FshifterStateFactory FshifterFactory = { { GET_VTABLE2(FshifterStateFactory, EffectStateFactory) } };

    return STATIC_CAST(EffectStateFactory, &FshifterFactory);
}

void ALfshifter_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FREQUENCY_SHIFTER_FREQUENCY:
            if(!(val >= AL_FREQUENCY_SHIFTER_MIN_FREQUENCY && val <= AL_FREQUENCY_SHIFTER_MAX_FREQUENCY))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Frequency shifter frequency out of range");
            props->Fshifter.Frequency = (ALfloat) val;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid frequency shifter float property 0x%04x", param);
    }
}

void ALfshifter_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    ALfshifter_setParamf(effect, context, param, vals[0]);
}

void ALfshifter_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FREQUENCY_SHIFTER_LEFT_DIRECTION:
            if(!(val >= AL_FREQUENCY_SHIFTER_MIN_LEFT_DIRECTION && val <= AL_FREQUENCY_SHIFTER_MAX_LEFT_DIRECTION))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Frequency shifter left direction out of range");
            props->Fshifter.Left_direction = val;
            break;

        case AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION:
            if(!(val >= AL_FREQUENCY_SHIFTER_MIN_RIGHT_DIRECTION && val <= AL_FREQUENCY_SHIFTER_MAX_RIGHT_DIRECTION))
                SETERR_RETURN(context, AL_INVALID_VALUE,,"Frequency shifter right direction out of range");
            props->Fshifter.Right_direction = val;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid frequency shifter integer property 0x%04x", param);
    }
}
void ALfshifter_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    ALfshifter_setParami(effect, context, param, vals[0]);
}

void ALfshifter_getParami(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FREQUENCY_SHIFTER_LEFT_DIRECTION:
            *val = (ALint)props->Fshifter.Left_direction;
            break;
        case AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION:
            *val = (ALint)props->Fshifter.Right_direction;
            break;
        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid frequency shifter integer property 0x%04x", param);
    }
}
void ALfshifter_getParamiv(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    ALfshifter_getParami(effect, context, param, vals);
}

void ALfshifter_getParamf(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{

    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FREQUENCY_SHIFTER_FREQUENCY:
            *val = (ALfloat)props->Fshifter.Frequency;
            break;

        default:
            alSetError(context, AL_INVALID_ENUM, "Invalid frequency shifter float property 0x%04x", param);
    }

}

void ALfshifter_getParamfv(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    ALfshifter_getParamf(effect, context, param, vals);
}

DEFINE_ALEFFECT_VTABLE(ALfshifter);