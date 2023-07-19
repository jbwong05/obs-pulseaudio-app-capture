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

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>

#pragma once

/**
 * Initialize the pulseaudio mainloop and increase the reference count
 */
int_fast32_t pulse_init();

/**
 * Unreference the pulseaudio mainloop, when the reference count reaches
 * zero the mainloop will automatically be destroyed
 */
void pulse_unref();

/**
 * Lock the mainloop
 *
 * In order to allow for multiple threads to use the same mainloop pulseaudio
 * provides it's own locking mechanism. This function should be called before
 * using any pulseaudio function that is in any way related to the mainloop or
 * context.
 *
 * @note use of this function may cause deadlocks
 *
 * @warning do not use with pulse_ wrapper functions
 */
void pulse_lock();

/**
 * Unlock the mainloop
 *
 * @see pulse_lock()
 */
void pulse_unlock();

/**
 * Wait for events to happen
 *
 * This function should be called when waiting for an event to happen.
 */
void pulse_wait();

/**
 * Wait for accept signal from calling thread
 *
 * This function tells the pulseaudio mainloop wheter the data provided to
 * the callback should be retained until the calling thread executes
 * pulse_accept()
 *
 * If wait_for_accept is 0 the function returns and the data is freed.
 */
void pulse_signal(int wait_for_accept);

/**
 * Signal the waiting callback to return
 *
 * This function is used in conjunction with pulse_signal()
 */
void pulse_accept();

/**
 * Request client information
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulse_get_client_info_list(pa_client_info_cb_t cb, void *userdata);

int_fast32_t pulse_get_source_info_by_idx(pa_source_info_cb_t cb, uint32_t idx,
					  void *userdata);

/**
 * Request source information from a specific source
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @param cb pointer to the callback function
 * @param name the source name to get information for
 * @param userdata pointer to userdata the callback will be called with
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulse_get_source_info_by_name(pa_source_info_cb_t cb,
					   const char *name, void *userdata);

/**
 * Request server information
 *
 * The function will block until the operation was executed and the mainloop
 * called the provided callback function.
 *
 * @return negative on error
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
int_fast32_t pulse_get_server_info(pa_server_info_cb_t cb, void *userdata);

/**
 * Create a new stream with the default properties
 *
 * @note The function will block until the server context is ready.
 *
 * @warning call without active locks
 */
pa_stream *pulse_stream_new(const char *name, const pa_sample_spec *ss,
			    const pa_channel_map *map);

int_fast32_t pulse_get_sink_input_info_list(pa_sink_input_info_cb_t cb,
					    void *userdata);

int_fast32_t pulse_get_sink_info_list(pa_sink_info_cb_t cb, void *userdata);

int_fast32_t pulse_get_sink_name_by_index(uint32_t idx, pa_sink_info_cb_t cb,
					  void *userdata);

int_fast32_t pulse_load_new_module(const char *name, const char *argument,
				   pa_context_index_cb_t cb, void *userdata);

int_fast32_t pulse_get_sink_list(pa_sink_info_cb_t cb, void *userdata);

int_fast32_t pulse_move_sink_input(uint32_t sink_input_idx,
				   uint32_t new_sink_idx,
				   pa_context_success_cb_t cb, void *userdata);

int_fast32_t pulse_unload_module(uint32_t idx, pa_context_success_cb_t cb,
				 void *userdata);

int_fast32_t pulse_subscribe_events(pa_context_subscribe_cb_t cb,
				    void *userdata);

#ifdef __cplusplus
}
#endif
