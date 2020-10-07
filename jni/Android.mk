LOCAL_PATH := $(call my-dir)/..

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_MODULE    := openal
# LOCAL_CFLAGS := -DCPU_EXTS=Default -DHAVE_OPENSL=1
LOCAL_SRC_FILES := \
    common/alcomplex.cpp \
    common/alexcpt.cpp \
    common/alfstream.cpp \
    common/almalloc.cpp \
    common/alstring.cpp \
    common/dynload.cpp \
    common/polyphase_resampler.cpp \
    common/strutils.cpp \
    common/threads.cpp \
    al/auxeffectslot.cpp \
    al/buffer.cpp \
    al/effect.cpp \
    al/error.cpp \
    al/event.cpp \
    al/extension.cpp \
    al/filter.cpp \
    al/listener.cpp \
    al/source.cpp \
    al/state.cpp \
    alc/alc.cpp \
    alc/alu.cpp \
    alc/alconfig.cpp \
    alc/ambdec.cpp \
    alc/bformatdec.cpp \
    alc/bs2b.cpp \
    alc/bsinc_tables.cpp \
    alc/buffer_storage.cpp \
    alc/converter.cpp \
    alc/cpu_caps.cpp \
    alc/effects/autowah.cpp \
    alc/effects/chorus.cpp \
    alc/effects/compressor.cpp \
    alc/effects/convolution.cpp \
    alc/effects/dedicated.cpp \
    alc/effects/distortion.cpp \
    alc/effects/echo.cpp \
    alc/effects/equalizer.cpp \
    alc/effects/fshifter.cpp \
    alc/effects/modulator.cpp \
    alc/effects/null.cpp \
    alc/effects/pshifter.cpp \
    alc/effects/reverb.cpp \
    alc/effects/vmorpher.cpp \
    alc/filters/biquad.cpp \
    alc/filters/nfc.cpp \
    alc/filters/splitter.cpp \
    alc/fmt_traits.cpp \
    alc/fpu_ctrl.cpp \
    alc/helpers.cpp \
    alc/hrtf.cpp \
    alc/mastering.cpp \
    alc/panning.cpp \
    alc/ringbuffer.cpp \
    alc/uhjfilter.cpp \
    alc/uiddefs.cpp \
    alc/voice.cpp \
    alc/mixer/mixer_c.cpp \
    alc/mixer/mixer_neon.cpp.neon \
    alc/backends/base.cpp \
    alc/backends/loopback.cpp \
    alc/backends/null.cpp \
    alc/backends/opensl.cpp
    
#   alc/backends/oboe.cpp
LOCAL_CFLAGS := -mfpu=neon
LOCAL_LDLIBS := -llog -lopensles
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

include $(BUILD_SHARED_LIBRARY)
