#include "vr_driver.h"
#include <map>
#include <iostream>

namespace vr {
	/// access to vr_kit map singleton 
	std::map<void*, vr_kit*>& ref_vr_kit_map()
	{
		static std::map<void*, vr_kit*> vr_kit_map;
		return vr_kit_map;
	}

	/// provide reference to reference states
	vr_trackable_state& vr_driver::ref_reference_state(const std::string& serial_nummer) 
	{
		return reference_states[serial_nummer]; 
	}
	
	/// remove all reference states
	void vr_driver::clear_reference_states() 
	{
		reference_states.clear(); 
	}
	
	/// mark all reference states as untracked
	void vr_driver::mark_references_as_untracked() 
	{
		for (auto& s : reference_states)
			s.second.status = VRS_DETACHED;
	}

	/// provide read only access to reference states
	const std::map<std::string, vr_trackable_state>& vr_driver::get_reference_states() const 
	{ 
		return reference_states; 
	}

	/// declare destructor virtual to ensure it being called also for derived classes
	vr_driver::~vr_driver()
	{

	}
	/// put a 3d x direction into passed array
	void vr_driver::put_x_direction(float* x_dir) const
	{
		x_dir[0] = 1;
		x_dir[1] = x_dir[2] = 0;
	}
	/// call this method during replacement of vr kits. In case vr kit handle had been registered before it is replaced, otherwise it is registered
	void vr_driver::replace_vr_kit(void* handle, vr_kit* kit)
	{
		auto& kit_map = ref_vr_kit_map();
		auto iter = kit_map.find(handle);
		if (iter != kit_map.end())
			iter->second = kit;
		else
			kit_map[handle] = kit;
	}
	/// call this method during scanning of vr kits but only in case vr kit id does not yield vr kit through global get_vr_kit function
	void vr_driver::register_vr_kit(void* handle, vr_kit* kit)
	{
		auto& kit_map = ref_vr_kit_map();
		auto iter = kit_map.find(handle);
		if (iter != kit_map.end()) {
			std::cerr << "WARNING: vr kit <" << kit->get_name() << "> recreated. Deleted copy <" 
				<< iter->second->get_name() << "> might be still in use." << std::endl;
			delete iter->second;
			iter->second = kit;
		}
		else {
			kit_map[handle] = kit;
		}
	}
	/// initialize the camera of a vr_kit and return whether this was successful
	bool vr_driver::initialize_camera(vr_kit* kit) const
	{
		return kit->get_camera()->initialize();
	}

	void vr_driver::set_index(unsigned _idx)
	{
		driver_index = _idx;
	}

	/// return registered drivers
	std::vector<vr_driver*>& ref_drivers()
	{
		static std::vector<vr_driver*> drivers;
		return drivers;
	}

	/// register a new driver
	void register_driver(vr_driver* vrd)
	{
		vrd->set_index((unsigned)ref_drivers().size());
		ref_drivers().push_back(vrd);
	}
	/// return a vector with all registered vr drivers
	std::vector<vr_driver*>& get_vr_drivers()
	{
		return ref_drivers();
	}

	/// iterate all registered vr drivers to scan for vr kits and return vector of vr kit handles
	std::vector<void*> scan_vr_kits()
	{
		std::vector<void*> kit_handles;
		for (auto driver_ptr : ref_drivers()) {
			auto new_ids = driver_ptr->scan_vr_kits();
			kit_handles.insert(kit_handles.end(), new_ids.begin(), new_ids.end());
		}
		return kit_handles;
	}
	/// scan vr kits and if one of given index exists replace it by passed kit and return index to replaced kit; in case of failure return 0 pointer
	vr_kit* replace_by_index(int vr_kit_index, vr_kit* new_kit_ptr)
	{
		vr_kit* kit_ptr = 0;
		for (auto driver_ptr : ref_drivers())
			if (kit_ptr = driver_ptr->replace_by_index(vr_kit_index, new_kit_ptr))
				break;
		return kit_ptr;
	}
	/// scan vr kits and if one with given pointer exists, replace it by passed kit; return whether replacement was successful
	bool replace_by_pointer(vr_kit* old_kit_ptr, vr_kit* new_kit_ptr)
	{
		for (auto driver_ptr : ref_drivers())
			if (driver_ptr->replace_by_pointer(old_kit_ptr, new_kit_ptr))
				return true;
		return false;
	}
	/// query a pointer to a vr kit by its device handle, function can return null pointer in case that no vr kit exists for given handle
	vr_kit* get_vr_kit(void* handle)
	{
		auto iter = ref_vr_kit_map().find(handle);
		if (iter == ref_vr_kit_map().end())
			return NULL;
		return iter->second;
	}
	/// unregister a previously registered vr kit by handle and pointer
	bool unregister_vr_kit(void* handle, vr_kit* vr_kit_ptr)
	{
		auto iter = ref_vr_kit_map().find(handle);
		if (iter == ref_vr_kit_map().end())
			return false;
		if (iter->second != vr_kit_ptr)
			return false;
		ref_vr_kit_map().erase(handle);
		return true;
	}
}