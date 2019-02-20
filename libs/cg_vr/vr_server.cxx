#include <vr/vr_driver.h>
#include "vr_server.h"
#include <cgv/gui/application.h>
#include <cgv/signal/rebind.h>
#include <cgv/gui/trigger.h>
#include <cassert>

namespace cgv {
	namespace gui {
		// some helper function to compare arrays
		bool array_unequal(const float* a1, const float* a2, unsigned n) {
			for (unsigned i = 0; i < n; ++i)
				if (a1[i] != a2[i])
					return true;
			return false;
		}
		/// construct a key event from its textual description 
		vr_key_event::vr_key_event(void* _device_handle, int _controller_index, const vr::vr_kit_state& _state,
			unsigned short _key, KeyAction _action, unsigned char _char, 
			unsigned char _modifiers, double _time)
			: device_handle(_device_handle), controller_index(_controller_index), state(_state),
			key_event(_key, _action, _char, _modifiers, 0, _time)
		{
			flags = EF_VR;
		}
		/// write to stream
		void vr_key_event::stream_out(std::ostream& os) const
		{
			event::stream_out(os);
			os << vr::get_key_string(key);
			switch (action) {
			case KA_RELEASE:
				os << " up";
				break;
			case KA_PRESS:
				if (get_char())
					os << " = '" << get_char() << "'";
				break;
			case KA_REPEAT:
				os << " repeat ";
				break;
			}
			os << "[" << device_handle << "]";
			if (get_modifiers() != 0) {
				os << " {" << vr::get_state_flag_string(vr::VRButtonStateFlags(get_modifiers())) << "}";
			}
		}
		/// read from stream
		void vr_key_event::stream_in(std::istream& is)
		{
			key_event::stream_in(is);
		}

		/// construct a throttle event from value and value change
		vr_throttle_event::vr_throttle_event(void* _device_handle, unsigned _controller_index, const vr::vr_kit_state& _state,
			float _x, float _dx, unsigned _player_index, unsigned _throttle_index, double _time)
			: throttle_event(_x, _dx, _player_index, _controller_index, _throttle_index, _time),
			device_handle(_device_handle), state(_state)
		{
			flags = EF_VR;
		}
		/// write to stream
		void vr_throttle_event::stream_out(std::ostream& os) const
		{
			throttle_event::stream_out(os);
			os << "->" << device_handle;
		}
		/// construct a key event from its textual description 
		vr_stick_event::vr_stick_event(void* _device_handle, unsigned _controller_index, const vr::vr_kit_state& _state,
			StickAction _action, float _x, float _y, float _dx, float _dy,
			unsigned _player_index, unsigned _stick_index, double _time)
			: stick_event(_action, _x, _y, _dx, _dy, _player_index,_controller_index,_stick_index,_time),
			device_handle(_device_handle), state(_state)
		{
			flags = EF_VR;
		}
		/// write to stream
		void vr_stick_event::stream_out(std::ostream& os) const
		{
			stick_event::stream_out(os);
			os << "->" << device_handle;
		}
		/// construct a key event from its textual description 
		vr_pose_event::vr_pose_event(void* _device_handle, short _trackable_index, const vr::vr_kit_state& _state,
			const float *_pose, unsigned short _player_index, double _time)
			: pose_event(_pose, _player_index, _trackable_index, _time),
			device_handle(_device_handle), state(_state)
		{
			flags = EF_VR;
		}
		/// write to stream
		void vr_pose_event::stream_out(std::ostream& os) const
		{
			pose_event::stream_out(os);
			os << "->" << device_handle;
		}

		/// construct server with default configuration
		vr_server::vr_server()
		{
			last_device_scan = -1;
			device_scan_interval = 1;
			event_type_flags = VRE_ALL;
			last_time_stamps.resize(vr_kit_handles.size(), 0);
		}
		/// set time interval in seconds to check for device connection changes
		void vr_server::set_device_scan_interval(double duration)
		{
			device_scan_interval = duration;
		}
		struct button_mapping
		{
			vr::VRButtonStateFlags flag;
			vr::VRKeys key[2];
		};

		/// 
		VREventTypeFlags vr_server::get_event_type_flags() const
		{
			return event_type_flags;
		}
		/// 
		void vr_server::set_event_type_flags(VREventTypeFlags flags)
		{
			event_type_flags = flags;
		}
		///
		void vr_server::emit_events_and_update_state(void* kit_handle, const vr::vr_kit_state& new_state, vr::vr_kit_state& last_state, VREventTypeFlags flags, double time)
		{
			static button_mapping button_keys[5] = {
				{ vr::VRF_MENU, vr::VR_LEFT_MENU, vr::VR_RIGHT_MENU},
				{ vr::VRF_BUTTON0, vr::VR_LEFT_BUTTON0, vr::VR_RIGHT_BUTTON0},
				{ vr::VRF_BUTTON1, vr::VR_LEFT_BUTTON1, vr::VR_RIGHT_BUTTON1},
				{ vr::VRF_BUTTON2, vr::VR_LEFT_BUTTON2, vr::VR_RIGHT_BUTTON2},
				{ vr::VRF_BUTTON3, vr::VR_LEFT_BUTTON3, vr::VR_RIGHT_BUTTON3}
			};
			static button_mapping stick_keys[2] = {
				{ vr::VRF_STICK_TOUCH, vr::VR_LEFT_STICK_TOUCH, vr::VR_RIGHT_STICK_TOUCH},
				{ vr::VRF_STICK, vr::VR_LEFT_STICK, vr::VR_RIGHT_STICK}
			};

			// check for status changes and emit signal for found status changes					
			if ((flags & VRE_STATUS) != 0) {
				if (new_state.hmd.status != last_state.hmd.status)
					on_status_change(kit_handle, -1, last_state.hmd.status, new_state.hmd.status);
				if (new_state.controller[0].status != last_state.controller[0].status)
					on_status_change(kit_handle, 0, last_state.controller[0].status, new_state.controller[0].status);
				if (new_state.controller[1].status != last_state.controller[1].status)
					on_status_change(kit_handle, 1, last_state.controller[1].status, new_state.controller[1].status);
			}
			// check for key changes and emit key_event 
			if ((flags & (VRE_KEY+ VRE_STICK+VRE_STICK_KEY)) != 0) {
				for (int c = 0; c < 2; ++c) {
					if (new_state.controller[c].status != vr::VRS_DETACHED &&
						last_state.controller[c].status != vr::VRS_DETACHED &&
						new_state.controller[c].button_flags != last_state.controller[c].button_flags) {
						if ((flags & VRE_KEY) != 0) {
							for (unsigned j = 0; j < 5; ++j) {
								if ((new_state.controller[c].button_flags & button_keys[j].flag) !=
									(last_state.controller[c].button_flags & button_keys[j].flag)) {
									// construct and emit event
									vr_key_event vrke(kit_handle, c, new_state,
										button_keys[j].key[c],
										(new_state.controller[c].button_flags & button_keys[j].flag) != 0 ? KA_PRESS : KA_RELEASE,
										0, 0, time);
									on_event(vrke);
								}
							}
						}
						if ((flags & VRE_STICK_KEY) != 0) {
							for (unsigned j = 0; j < 2; ++j) {
								if ((new_state.controller[c].button_flags & stick_keys[j].flag) !=
									(last_state.controller[c].button_flags & stick_keys[j].flag)) {
									short key_offset = 0;
									// in case of pressed stick, refine key from direction
									if (j == 1) {
										int x = new_state.controller[c].axes[0] > 0.5f ? 1 :
											(new_state.controller[c].axes[0] < -0.5f ? -1 : 0);
										int y = new_state.controller[c].axes[1] > 0.5f ? 1 :
											(new_state.controller[c].axes[1] < -0.5f ? -1 : 0);
										key_offset = 3 * y + x;
									}
									// construct and emit event
									vr_key_event vrke(kit_handle, c, new_state,
										stick_keys[j].key[c] + key_offset,
										(new_state.controller[c].button_flags & stick_keys[j].flag) != 0 ? KA_PRESS : KA_RELEASE, 0,
										0, time);
									on_event(vrke);
								}
							}
						}
						if ((flags & VRE_STICK) != 0) {
							// check if press/touch state changed
							int new_p_or_t = 0;
							if ((new_state.controller[c].button_flags & vr::VRF_STICK) != 0)
								new_p_or_t = 2;
							else
								if ((new_state.controller[c].button_flags & vr::VRF_STICK_TOUCH) != 0)
									new_p_or_t = 1;
							int old_p_or_t = 0;
							if ((last_state.controller[c].button_flags & vr::VRF_STICK) != 0)
								old_p_or_t = 2;
							else
								if ((last_state.controller[c].button_flags & vr::VRF_STICK_TOUCH) != 0)
									old_p_or_t = 1;
							if (new_p_or_t != old_p_or_t) {
								static StickAction actions[9] = {
									SA_MOVE, SA_TOUCH, SA_PRESS,
									SA_RELEASE, SA_MOVE, SA_PRESS,
									SA_RELEASE, SA_UNPRESS, SA_MOVE
								};
								StickAction action = actions[new_p_or_t + 3 * old_p_or_t];
								vr::vr_kit* kit_ptr = vr::get_vr_kit(kit_handle);
								if (kit_ptr) {
									const auto& throttles_and_sticks = kit_ptr->get_controller_throttles_and_sticks(c);
									for (const auto& throttle_or_stick : throttles_and_sticks) {
										if (throttle_or_stick.second == -1)
											continue;
										int ai = throttle_or_stick.first;
										int aj = throttle_or_stick.second;
										float x = new_state.controller[c].axes[ai];
										float y = new_state.controller[c].axes[aj];
										vr_stick_event vrse(kit_handle, c, new_state,
											action, x, y, 0, 0, 0, 0, time);
										on_event(vrse);
									}
								}
							}
						}
					}
				}
			}
			// check for throttle and stick events
			if (((flags & VRE_THROTTLE) != 0) || ((flags & VRE_STICK) != 0)) {
				vr::vr_kit* kit_ptr = vr::get_vr_kit(kit_handle);
				if (kit_ptr) {
					for (int c = 0; c < 2; ++c) {
						if (new_state.controller[c].status != vr::VRS_DETACHED &&
							last_state.controller[c].status != vr::VRS_DETACHED &&
							array_unequal(new_state.controller[c].axes, last_state.controller[c].axes, 8)) {
							// iterate throttle and sticks
							const auto& throttles_and_sticks = kit_ptr->get_controller_throttles_and_sticks(c);
							int ti = 0;
							int si = 0;
							for (const auto& throttle_or_stick : throttles_and_sticks) {
								if (throttle_or_stick.second == -1) {
									// throttle case
									if ((flags & VRE_THROTTLE) != 0) {
										int ai = throttle_or_stick.first;
										float x = new_state.controller[c].axes[ai];
										float dx = x - last_state.controller[c].axes[ai];
										if (dx != 0) {
											/// construct a throttle event from value and value change
											vr_throttle_event vrte(kit_handle, c, new_state, x, dx, 0, ti, time);
											on_event(vrte);
										}
									}
									++ti;
								}
								else {
									// stick case
									if ((flags & VRE_STICK) != 0) {
										int ai = throttle_or_stick.first;
										int aj = throttle_or_stick.second;
										float x = new_state.controller[c].axes[ai];
										float y = new_state.controller[c].axes[aj];
										float dx = x - last_state.controller[c].axes[ai];
										float dy = y - last_state.controller[c].axes[aj];
										if (dx != 0 || dy != 0) {
											/// construct a throttle event from value and value change
											StickAction action = SA_MOVE;
											if ((new_state.controller[c].button_flags & vr::VRF_STICK) != 0)
												action = SA_DRAG;
											vr_stick_event vrse(kit_handle, c, new_state, 
												action, x, y, dx, dy, 0, si, time);
											on_event(vrse);
										}
									}
									++si;
								}
							}
						}
					}
				}
			}
			// finally check for pose, axis or vibration changes and emit general new_state change if necessary
			if ((flags & VRE_POSE) != 0) {
				if (array_unequal(new_state.hmd.pose, last_state.hmd.pose, 12)) {
					vr_pose_event vrpe(kit_handle, -1, new_state, new_state.hmd.pose, 0, time);
					on_event(vrpe);
				}
				for (int c = 0; c < 2; ++c) {
					if (array_unequal(new_state.controller[c].pose, last_state.controller[c].pose, 12)) {
						vr_pose_event vrpe(kit_handle, c, new_state, new_state.controller[c].pose, 0, time);
						on_event(vrpe);
					}
				}
			}
			last_state = new_state;
		}

		/// check enabled gamepad devices for new events and dispatch them through the on_event signal
		void vr_server::check_device_changes(double time)
		{
			std::vector<bool> is_first_state(vr_kit_handles.size(), false);
			if (last_device_scan < 0 ||
				((device_scan_interval > 0) && (time > last_device_scan + device_scan_interval))) {
				last_device_scan = time;
				std::vector<void*> new_handles = vr::scan_vr_kits();
				std::vector<vr::vr_kit_state> new_last_states;
				new_last_states.resize(new_handles.size());
				is_first_state.resize(new_handles.size(), false);
				if ((get_event_type_flags() & VRE_DEVICE) != 0)
					for (void* h1 : vr_kit_handles)
						if (std::find(new_handles.begin(), new_handles.end(), h1) == new_handles.end())
							on_device_change(h1, false);
				unsigned i = 0;
				for (void* h2 : new_handles) {
					auto iter = std::find(vr_kit_handles.begin(), vr_kit_handles.end(), h2);
					if (iter == vr_kit_handles.end()) {
						if ((get_event_type_flags() & VRE_DEVICE) != 0)
							on_device_change(h2, true);
						is_first_state.at(i) = true;
					}
					else {
						new_last_states[i] = last_states.at(iter - vr_kit_handles.begin());
					}
					++i;
				}
				vr_kit_handles = new_handles;
				last_states = new_last_states;
			}
		}
		/// check enabled gamepad devices for new events and dispatch them through the on_event signal
		void vr_server::check_and_emit_events(double time)
		{
			check_device_changes(time);
			// loop all devices
			unsigned i;
			for (i = 0; i < vr_kit_handles.size(); ++i) {
				vr::vr_kit* kit = vr::get_vr_kit(vr_kit_handles[i]);
				if (!kit)
					continue;
				// query current state
				vr::vr_kit_state state;
				kit->query_state(state, 1);
				emit_events_and_update_state(vr_kit_handles[i], state, last_states[i], event_type_flags, time);
			}
		}

		/// in case the current vr state of a kit had been queried outside, use this function to communicate the new state to the server 
		bool vr_server::check_new_state(void* kit_handle, const vr::vr_kit_state& new_state, double time)
		{
			return check_new_state(kit_handle, new_state, time, event_type_flags);
		}
		/// same as previous function but with overwrite of flags
		bool vr_server::check_new_state(void* kit_handle, const vr::vr_kit_state& new_state, double time, VREventTypeFlags flags)
		{
			auto iter = std::find(vr_kit_handles.begin(), vr_kit_handles.end(), kit_handle);
			if (iter == vr_kit_handles.end())
				return false;
			size_t i = iter - vr_kit_handles.begin();
			emit_events_and_update_state(kit_handle, new_state, last_states[i], flags, time);
			return true;
		}
		/// return a reference to gamepad server singleton
		vr_server& ref_vr_server()
		{
			static vr_server server;
			return server;
		}

		window_ptr& ref_dispatch_window_pointer()
		{
			static window_ptr w;
			return w;
		}
		bool dispatch_vr_event(cgv::gui::event& e)
		{
			return application::get_window(0)->dispatch_event(e);
		}

		/// connect the gamepad server to the given window or the first window of the application, if window is not provided
		void connect_vr_server(bool connect_device_change_only_to_animation_trigger, cgv::gui::window_ptr w)
		{
			if (w.empty())
				w = application::get_window(0);
			ref_dispatch_window_pointer() = w;
			connect(ref_vr_server().on_event, dispatch_vr_event);
			if (connect_device_change_only_to_animation_trigger)
				connect_copy(get_animation_trigger().shoot, cgv::signal::rebind(&ref_vr_server(), &vr_server::check_device_changes, cgv::signal::_1));
			else
				connect_copy(get_animation_trigger().shoot, cgv::signal::rebind(&ref_vr_server(), &vr_server::check_and_emit_events, cgv::signal::_1));
		}

	}
}