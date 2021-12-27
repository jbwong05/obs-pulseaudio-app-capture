/*
Copyright (C) 2021 by Joshua Wong <jbwong05@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <util/platform.h>
#include <util/bmem.h>
#include <util/util_uint64.h>
#include <obs-module.h>
#include "plugin-macros.h.in"
#include "pulse-wrapper.h"
#include <unordered_map>
#include <string>

using std::unordered_map;
using std::pair;
using std::string;

#define NSEC_PER_SEC 1000000000LL
#define NSEC_PER_MSEC 1000000L

#define PULSE_DATA(voidptr) \
	struct pulse_data *data = (struct pulse_data *)voidptr;

struct pulse_data {
	obs_source_t *source;
	pa_stream *stream;

	/* client info */
	uint32_t client_idx;
	char *client;

	/* sink input info */
	uint32_t sink_input_idx;
	uint32_t sink_input_sink_idx;
	char *sink_input_sink_name;

	int move_success;

	/* combine module info */
	// maps from sink input sink index to combine module index
	unordered_map<uint32_t, uint32_t> combine_modules;
	int load_module_success;

	/* server info */
	enum speaker_layout speakers;
	pa_sample_format_t format;
	uint_fast32_t samples_per_sec;
	uint_fast32_t bytes_per_frame;
	uint_fast8_t channels;
	uint64_t first_ts;

	/* statistics */
	uint_fast32_t packets;
	uint_fast64_t frames;
};

static void pulse_stop_recording(struct pulse_data *data);

/**
 * get obs from pulse audio format
 */
static enum audio_format pulse_to_obs_audio_format(pa_sample_format_t format)
{
	switch (format) {
	case PA_SAMPLE_U8:
		return AUDIO_FORMAT_U8BIT;
	case PA_SAMPLE_S16LE:
		return AUDIO_FORMAT_16BIT;
	case PA_SAMPLE_S32LE:
		return AUDIO_FORMAT_32BIT;
	case PA_SAMPLE_FLOAT32LE:
		return AUDIO_FORMAT_FLOAT;
	default:
		return AUDIO_FORMAT_UNKNOWN;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

/**
 * Get obs speaker layout from number of channels
 *
 * @param channels number of channels reported by pulseaudio
 *
 * @return obs speaker_layout id
 *
 * @note This *might* not work for some rather unusual setups, but should work
 *       fine for the majority of cases.
 */
static enum speaker_layout
pulse_channels_to_obs_speakers(uint_fast32_t channels)
{
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	}

	return SPEAKERS_UNKNOWN;
}

static pa_channel_map pulse_channel_map(enum speaker_layout layout)
{
	pa_channel_map ret;

	ret.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
	ret.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
	ret.map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
	ret.map[3] = PA_CHANNEL_POSITION_LFE;
	ret.map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
	ret.map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
	ret.map[6] = PA_CHANNEL_POSITION_SIDE_LEFT;
	ret.map[7] = PA_CHANNEL_POSITION_SIDE_RIGHT;

	switch (layout) {
	case SPEAKERS_MONO:
		ret.channels = 1;
		ret.map[0] = PA_CHANNEL_POSITION_MONO;
		break;

	case SPEAKERS_STEREO:
		ret.channels = 2;
		break;

	case SPEAKERS_2POINT1:
		ret.channels = 3;
		ret.map[2] = PA_CHANNEL_POSITION_LFE;
		break;

	case SPEAKERS_4POINT0:
		ret.channels = 4;
		ret.map[3] = PA_CHANNEL_POSITION_REAR_CENTER;
		break;

	case SPEAKERS_4POINT1:
		ret.channels = 5;
		ret.map[4] = PA_CHANNEL_POSITION_REAR_CENTER;
		break;

	case SPEAKERS_5POINT1:
		ret.channels = 6;
		break;

	case SPEAKERS_7POINT1:
		ret.channels = 8;
		break;

	case SPEAKERS_UNKNOWN:
	default:
		ret.channels = 0;
		break;
	}

	return ret;
}

static inline uint64_t samples_to_ns(size_t frames, uint_fast32_t rate)
{
	return util_mul_div64(frames, NSEC_PER_SEC, rate);
}

static inline uint64_t get_sample_time(size_t frames, uint_fast32_t rate)
{
	return os_gettime_ns() - samples_to_ns(frames, rate);
}

#define STARTUP_TIMEOUT_NS (500 * NSEC_PER_MSEC)

/**
 * Callback for pulse which gets executed when new audio data is available
 *
 * @warning The function may be called even after disconnecting the stream
 */
static void pulse_stream_read(pa_stream *p, size_t nbytes, void *userdata)
{
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(nbytes);
	PULSE_DATA(userdata);

	const void *frames;
	size_t bytes;

	if (!data->stream)
		goto exit;

	pa_stream_peek(data->stream, &frames, &bytes);

	// check if we got data
	if (!bytes)
		goto exit;

	if (!frames) {
		blog(LOG_ERROR, "Got audio hole of %u bytes",
		     (unsigned int)bytes);
		pa_stream_drop(data->stream);
		goto exit;
	}

	struct obs_source_audio out;
	out.speakers = data->speakers;
	out.samples_per_sec = data->samples_per_sec;
	out.format = pulse_to_obs_audio_format(data->format);
	out.data[0] = (uint8_t *)frames;
	out.frames = bytes / data->bytes_per_frame;
	out.timestamp = get_sample_time(out.frames, out.samples_per_sec);

	if (!data->first_ts)
		data->first_ts = out.timestamp + STARTUP_TIMEOUT_NS;

	if (out.timestamp > data->first_ts)
		obs_source_output_audio(data->source, &out);

	data->packets++;
	data->frames += out.frames;

	pa_stream_drop(data->stream);
exit:
	pulse_signal(0);
}

/**
 * Source client callback
 *
 * We use the default stream settings for recording here unless pulse is
 * configured to something obs can't deal with.
 */
static void pulse_source_info(pa_context *c, const pa_source_info *i, int eol,
			      void *userdata)
{
	UNUSED_PARAMETER(c);
	PULSE_DATA(userdata);
	// An error occured
	if (eol < 0) {
		data->format = PA_SAMPLE_INVALID;
		pulse_signal(0);
		return;
	}
	// Terminating call for multi instance callbacks
	if (eol > 0) {
		pulse_signal(0);
		return;
	}

	pa_proplist *proplist = i->proplist;

	blog(LOG_INFO,
	     "Audio format: %s, %" PRIu32 " Hz"
	     ", %" PRIu8 " channels",
	     pa_sample_format_to_string(i->sample_spec.format),
	     i->sample_spec.rate, i->sample_spec.channels);

	pa_sample_format_t format = i->sample_spec.format;
	if (pulse_to_obs_audio_format(format) == AUDIO_FORMAT_UNKNOWN) {
		format = PA_SAMPLE_FLOAT32LE;

		blog(LOG_INFO,
		     "Sample format %s not supported by OBS,"
		     "using %s instead for recording",
		     pa_sample_format_to_string(i->sample_spec.format),
		     pa_sample_format_to_string(format));
	}

	uint8_t channels = i->sample_spec.channels;
	if (pulse_channels_to_obs_speakers(channels) == SPEAKERS_UNKNOWN) {
		channels = 2;

		blog(LOG_INFO,
		     "%c channels not supported by OBS,"
		     "using %c instead for recording",
		     i->sample_spec.channels, channels);
	}

	data->format = format;
	data->samples_per_sec = i->sample_spec.rate;
	data->channels = channels;
}

/**
 * Start recording
 *
 * We request the default format used by pulse here because the data will be
 * converted and possibly re-sampled by obs anyway.
 *
 * For now we request a buffer length of 25ms although pulse seems to ignore
 * this setting for monitor streams. For "real" input streams this should work
 * fine though.
 */
static int_fast32_t pulse_start_recording(struct pulse_data *data)
{
	string monitor_name =
		"OBSPulseAppRecord" + string(data->client) + ".monitor";
	if (pulse_get_source_info_by_name(pulse_source_info,
					  monitor_name.c_str(), data) < 0) {
		blog(LOG_ERROR, "Unable to get client info !");
		return -1;
	}

	if (data->format == PA_SAMPLE_INVALID) {
		blog(LOG_ERROR,
		     "An error occurred while getting the client info!");
		return -1;
	}

	pa_sample_spec spec;
	spec.format = data->format;
	spec.rate = data->samples_per_sec;
	spec.channels = data->channels;

	if (!pa_sample_spec_valid(&spec)) {
		blog(LOG_ERROR, "Sample spec is not valid");
		return -1;
	}

	data->speakers = pulse_channels_to_obs_speakers(spec.channels);
	data->bytes_per_frame = pa_frame_size(&spec);

	pa_channel_map channel_map = pulse_channel_map(data->speakers);

	data->stream = pulse_stream_new(obs_source_get_name(data->source),
					&spec, &channel_map);
	if (!data->stream) {
		blog(LOG_ERROR, "Unable to create stream");
		return -1;
	}

	pulse_lock();
	pa_stream_set_read_callback(data->stream, pulse_stream_read,
				    (void *)data);
	pulse_unlock();

	pa_buffer_attr attr;
	attr.fragsize = pa_usec_to_bytes(25000, &spec);
	attr.maxlength = (uint32_t)-1;
	attr.minreq = (uint32_t)-1;
	attr.prebuf = (uint32_t)-1;
	attr.tlength = (uint32_t)-1;

	pa_stream_flags_t flags = PA_STREAM_ADJUST_LATENCY;

	pulse_lock();
	int_fast32_t ret = pa_stream_connect_record(
		data->stream, monitor_name.c_str(), &attr, flags);
	pulse_unlock();
	if (ret < 0) {
		pulse_stop_recording(data);
		blog(LOG_ERROR, "Unable to connect to stream");
		return -1;
	}

	blog(LOG_INFO, "Started recording from '%s'", data->client);
	return 0;
}

/**
 * stop recording
 */
static void pulse_stop_recording(struct pulse_data *data)
{
	if (data->stream) {
		pulse_lock();
		pa_stream_disconnect(data->stream);
		pa_stream_unref(data->stream);
		data->stream = NULL;
		pulse_unlock();
	}

	blog(LOG_INFO, "Stopped recording from '%s'", data->client);
	blog(LOG_INFO,
	     "Got %" PRIuFAST32 " packets with %" PRIuFAST64 " frames",
	     data->packets, data->frames);

	data->first_ts = 0;
	data->packets = 0;
	data->frames = 0;
}

/**
 * input info callback
 */
static void pulse_client_info_list_cb(pa_context *c, const pa_client_info *i,
				      int eol, void *userdata)
{
	UNUSED_PARAMETER(c);
	if (eol != 0 || i->index != PA_INVALID_INDEX)
		goto skip;

	//char index[11];
	//memset(index, 0, sizeof(index));
	//snprintf(index, 10, "%d", i->index);
	// name is the index, value is the client name
	obs_property_list_add_string((obs_property_t *)userdata, i->name,
				     i->name);

skip:
	pulse_signal(0);
}

/**
 * Get plugin properties
 */
static obs_properties_t *pulse_properties()
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *clients = obs_properties_add_list(
		props, "client", obs_module_text("Device"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);

	pulse_init();
	pulse_get_client_info_list(pulse_client_info_list_cb, (void *)clients);

	size_t count = obs_property_list_item_count(clients);

	//if (count > 0)
	//	obs_property_list_insert_string(
	//		clients, 0, obs_module_text("Default"), "default");

	return props;
}

static obs_properties_t *pulse_input_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	return pulse_properties();
}

/**
 * Get plugin defaults
 */
static void pulse_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "client", "default");
}

/**
 * Returns the name of the plugin
 */
static const char *pulse_input_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("PulseAppInput");
}

/**
 * Destroy the plugin object and free all memory
 */
static void pulse_destroy(void *vptr)
{
	PULSE_DATA(vptr);

	if (!data)
		return;

	if (data->stream)
		pulse_stop_recording(data);
	pulse_unref();

	if (data->client)
		bfree(data->client);

	if (data->sink_input_sink_name)
		bfree(data->sink_input_sink_name);

	bfree(data);
}

static void get_client_idx_cb(pa_context *c, const pa_client_info *i, int eol,
			      void *userdata)
{
	PULSE_DATA(userdata);

	if (eol == 0 && data->client_idx == PA_INVALID_INDEX &&
	    strcmp(data->client, i->name) == 0) {
		data->client_idx = i->index;
	} else {
		pulse_signal(0);
	}
}

static void get_sink_input_cb(pa_context *c, const pa_sink_input_info *i,
			      int eol, void *userdata)
{
	PULSE_DATA(userdata);

	if (eol == 0 && data->sink_input_idx == PA_INVALID_INDEX &&
	    data->client_idx == i->client) {
		data->sink_input_idx = i->index;
		data->sink_input_sink_idx = i->sink;
	} else {
		pulse_signal(0);
	}
}

static void get_sink_name_by_index_cb(pa_context *c, const pa_sink_info *i,
				      int eol, void *userdata)
{
	PULSE_DATA(userdata);

	if (eol == 0 && i->index != PA_INVALID_INDEX) {
		data->sink_input_sink_name = bstrdup(i->name);
	} else {
		pulse_signal(0);
	}
}

static void load_new_combine_module_cb(pa_context *c, uint32_t idx,
				       void *userdata)
{
	PULSE_DATA(userdata);

	if (idx != PA_INVALID_INDEX) {
		data->combine_modules.insert(pair<uint32_t, uint32_t>(
			data->sink_input_sink_idx, idx));
	} else {
	}
}

static void move_sink_input_cb(pa_context *c, int success, void *userdata)
{
	PULSE_DATA(userdata);
	data->move_success = success;
}

/**
 * Update the input settings
 */
static void pulse_update(void *vptr, obs_data_t *settings)
{
	PULSE_DATA(vptr);
	bool restart = false;
	const char *new_client;

	new_client = obs_data_get_string(settings, "client");
	if (!data->client || strcmp(data->client, new_client) != 0) {
		if (data->client)
			bfree(data->client);
		data->client = bstrdup(new_client);
		data->client_idx = PA_INVALID_INDEX;

		restart = true;
	}

	if (!restart)
		return;

	if (data->stream)
		pulse_stop_recording(data);

	// Move existing sink-input back to old sink
	pulse_move_sink_input(data->sink_input_idx, data->sink_input_sink_idx,
			      move_sink_input_cb, data);

	if (!data->move_success) {
		return;
	}

	// Find client idx
	pulse_get_client_info_list(get_client_idx_cb, data);

	if (data->client_idx == PA_INVALID_INDEX) {
		return;
	}

	// Find sink-input with corresponding client
	data->sink_input_idx = PA_INVALID_INDEX;
	pulse_get_sink_input_info_list(get_sink_input_cb, data);

	if (data->sink_input_idx == PA_INVALID_INDEX) {
		return;
	}

	if (data->combine_modules.find(data->sink_input_sink_idx) ==
	    data->combine_modules.end()) {
		// A module for this sink has not yet been loaded

		// Get sink name from index
		pulse_get_sink_name_by_index(data->sink_input_sink_idx,
					     get_sink_name_by_index_cb, data);

		// Load the new module
		data->load_module_success = 1;
		string name = "OBSPulseAppRecord" + string(data->client);
		string args = "slaves=" + string(data->sink_input_sink_name);
		pulse_load_new_combine_module(name.c_str(), args.c_str(),
					      load_new_combine_module_cb, data);

		if (!data->load_module_success) {
			return;
		}
	}

	// Move sink-input to new module
	data->move_success = 1;
	uint32_t combine_module_idx =
		data->combine_modules.find(data->sink_input_sink_idx)->second;
	pulse_move_sink_input(data->sink_input_idx, combine_module_idx,
			      move_sink_input_cb, data);

	if (!data->move_success) {
		return;
	}

	// Start recording from the combine module
	pulse_start_recording(data);
}

/**
 * Create the plugin object
 */
static void *pulse_create(obs_data_t *settings, obs_source_t *source)
{
	struct pulse_data *data =
		(struct pulse_data *)bzalloc(sizeof(struct pulse_data));

	data->source = source;

	pulse_init();
	pulse_update(data, settings);

	return data;
}

static void *pulse_input_create(obs_data_t *settings, obs_source_t *source)
{
	return pulse_create(settings, source);
}

struct obs_source_info pulse_app_capture = {
	.id = "pulse_app_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = pulse_input_getname,
	.create = pulse_input_create,
	.destroy = pulse_destroy,
	.get_defaults = pulse_defaults,
	.get_properties = pulse_input_properties,
	.update = pulse_update,
	.icon_type = OBS_ICON_TYPE_AUDIO_INPUT,
};
