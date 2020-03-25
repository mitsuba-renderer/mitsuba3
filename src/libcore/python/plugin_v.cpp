#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++2a-extensions"

using Caster = py::object(*)(const mitsuba::Object *);
extern Caster cast_object;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(plugin)
NAMESPACE_BEGIN(detail)

// Helper macro
#define SET_PROP(Type, Setter) 									\
	if(isinstance<Type>(val)) {									\
		Type tmp = val.cast<Type>();                            \
        prop.Setter(key, tmp);                                  \
        continue;												\
    }

using Float = float;

/// Throws if non-whitespace characters are found after the given index.
static void check_whitespace_only(const std::string &s, size_t offset) {
	for (size_t i = offset; i < s.size(); ++i) {
		if (!std::isspace(s[i]))
			Throw("Invalid trailing characters in floating point number \"%s\"", s);
	}
}

static Float stof(const std::string &s) {
	size_t offset = 0;
	Float result = std::stof(s, &offset);
	check_whitespace_only(s, offset);
	return result;
}

void parse_rgb(PluginManager &pgmr, Properties &prop, py::dict &dict, bool within_emitter = false, bool within_spectrum = false) {
  	MTS_PY_IMPORT_TYPES()
  	
  	if(dict.size() != 1)
		Throw("Spectrum dict should be size 1");

	auto item = dict.begin();

	std::string rgb_name = item->first.cast<std::string>();
	Properties::Color3f col = item->second.cast<Properties::Color3f>();

	if(!within_spectrum) {
		bool is_ior = rgb_name == "eta" || rgb_name == "k" || rgb_name == "int_ior" ||
					  rgb_name == "ext_ior";
		Properties nested_prop(within_emitter? "srgb_d65" : "srgb");
		nested_prop.set_color("color", col);

		if(!within_emitter && is_ior)
		  nested_prop.set_bool("unbounded", true);

		ref<Object> obj = pgmr.create_object(nested_prop, 
			Class::for_name("Texture", mitsuba::detail::get_variant<Float, Spectrum >()));
		prop.set_object(rgb_name, obj);
	} else {
		prop.set_color("color", col);
	}
}

void parse_spectrum(PluginManager &pgmr, Properties &parent_prop, py::dict &dict, bool within_emitter = false) {
	MTS_PY_IMPORT_TYPES()

	// handle like obj 
	// can't handle wavelengths
	if(dict.contains("type")) {
		auto it = dict.begin();
		if(!is_scene && it->first.cast<std::string>().compare("type") != 0)
			Throw("First key should 'type'");
		std::string spec_type = item->second.cast<std::string>();
		Properties nested_prop(spec_type);

		for(; it!=dict.end(); ++it) {
			auto key = it.first.cast<std::string();
			auto val = it.second;
			
			if(key.compare("rgb") == 0) {
				auto nested_dict = val.cast<py::dict>();
				parse_rgb(pgmr, nested_prop, nested_dict, within_emitter, true);
			}

			SET_PROP(py::float_, set_float);
			SET_PROP(Properties::Color3f, set_color);
		}
	}

	// for spectrum not constant
	std::vector<Properties::Float> wavelengths, values;
	const Class *tex_cls = Class::for_name("Texture", mitsuba::detail::get_variant<Float, Spectrum >());

	if(dict.size() != 1)
		Throw("Spectrum dict should be size 1");

	auto item = dict.begin();
	std::string spec_name = item->first.cast<std::string>();

	if(isinstance<py::float_>(item->second) || isinstance<py::int_>(item->second)) {
		// constant
		Properties nested_prop("uniform");
		ScalarFloat val = item->second.cast<py::float_>();
		if(within_emitter && is_spectral_v<Spectrum>) {
		  nested_prop.set_plugin_name("d65");
		  nested_prop.set_float("scale", val);
		} else {
		  nested_prop.set_float("value", val);
		}
		ref<Object> obj = pgmr.create_object(nested_prop, tex_cls);
		auto expanded = obj->expand();
		if(!expanded.empty())
		  obj = expanded[0];
		parent_prop.set_object(spec_name, obj);
		return;
	} else {
		std::vector<std::string> tokens = string::tokenize(item->second.cast<std::string>());
		if(1 == tokens.size()) {
			// is filename
			std::string filename = item->second.cast<std::string>();
			spectrum_from_file(filename, wavelengths, values);
		} else {
			for (const std::string &token : tokens) {
				std::vector<std::string> pair = string::tokenize(token, ":");
				if (pair.size() != 2)
					Throw("invalid spectrum (expected wavelength:value pairs)");

				Properties::Float wavelength, value;
				try {
					wavelength = mitsuba::plugin::detail::stof(pair[0]);
					value = mitsuba::plugin::detail::stof(pair[1]);
				} catch (...) {
					Throw("could not parse wavelength:value pair: \"%s\"", tokens);
				}

				wavelengths.push_back(wavelength);
				values.push_back(value);
			}
		}
	}

	// scale and create
	Properties::Float unit_conversion = 1.f;
	if(within_emitter || !is_spectral_v<Spectrum>)
	unit_conversion = MTS_CIE_Y_NORMALIZATION;

	/* Detect whether wavelengths are regularly sampled and potentially
	 apply the conversion factor. */
	bool is_regular = true;
	Properties::Float interval = 0.f;
	for (size_t n = 0; n < wavelengths.size(); ++n) {
	values[n] *= unit_conversion;

	if (n <= 0)
		continue;

	Properties::Float distance = (wavelengths[n] - wavelengths[n - 1]);
	if (distance < 0.f)
		Throw("wavelengths must be specified in increasing order");
	if (n == 1)
		interval = distance;
	else if (std::abs(distance - interval) > math::Epsilon<Float>)
		is_regular = false;
	}

	Properties nested_prop;
	if(is_regular) {
		nested_prop.set_plugin_name("regular");
		nested_prop.set_long("size", wavelengths.size());
		nested_prop.set_float("lambda_min", wavelengths.front());
		nested_prop.set_float("lambda_max", wavelengths.back());
		nested_prop.set_pointer("values", values.data());
	} else {
		nested_prop.set_plugin_name("irregular");
		nested_prop.set_long("size", wavelengths.size());
		nested_prop.set_pointer("wavelengths", wavelengths.data());
		nested_prop.set_pointer("values", values.data());
	}

	ref<Object> obj = pgmr.create_object(nested_prop, tex_cls);

	// In non-spectral mode, pre-integrate against the CIE matching curves
	if (is_spectral_v<Spectrum>) {
		/// Spectral IOR values are unbounded and require special handling
		bool is_ior = spec_name == "eta" || spec_name == "k" || spec_name == "int_ior" ||
					  spec_name == "ext_ior";

		Properties::Color3f color = spectrum_to_rgb(wavelengths, values, !(within_emitter || is_ior));

		Properties props3;
		if (is_monochromatic_v<Spectrum>) {
			props3 = Properties("uniform");
			props3.set_float("value", luminance(color));
		} else {
			props3 = Properties(within_emitter ? "srgb_d65" : "srgb");
			props3.set_color("color", color);

			if (!within_emitter && is_ior)
				props3.set_bool("unbounded", true);
		}

		obj = PluginManager::instance()->create_object(
			props3, tex_cls);
	}

	parent_prop.set_object(spec_name, obj);
}

void parse_texture(PluginManager &pgmr, Properties &parent_prop, py::dict &dict) {
	MTS_PY_IMPORT_TYPES()
	if(dict.size() < 2)
		Throw("Dict should contain 'name' and 'type' in that order");

	auto it = dict.begin();
	if(it->first.cast<std::string>().compare("name") != 0)
		Throw("First key should 'name'");
	auto tex_name = it->second.cast<std::string>();

	++it;
	if(it->first.cast<std::string>().compare("type") != 0)
		Throw("Second key should 'type'");
	
	std::string plugin_name = it->second.cast<std::string>();
	++it;

	plugin_name[0] = std::toupper(plugin_name[0]);
	Properties prop(plugin_name);
	
	for(; it != dict.end(); ++it) {
		auto key = it->first.cast<std::string>();
		auto val = it->second;
		// check if the val is mitsuba texture dict
		if(key.compare("texture") == 0) {
			auto nested_dict = val.cast<py::dict>();
			parse_texture(pgmr, prop, nested_dict);
			
		}  else {
			SET_PROP(py::bool_, set_bool)
			SET_PROP(py::str, set_string)
			SET_PROP(Properties::Transform4f, set_transform)
		}
	}

	ref<Object> obj = pgmr.create_object(prop, 
			Class::for_name("Texture", mitsuba::detail::get_variant<Float, Spectrum >()));
	parent_prop.set_object(tex_name, obj);
}

void create_properties(PluginManager &pgmr, Properties &prop, py::dict &dict, bool within_emitter = false, bool within_spectrum = false, bool is_scene = false) {
	MTS_PY_IMPORT_TYPES()
	// get type
	auto it = dict.begin();

	if(!is_scene && it->first.cast<std::string>().compare("type") != 0)
		Throw("First key should 'type'");

	// Assert( it->first.cast<std::string>().compare("type") == 0, "First dict key should be str 'type'");
	
	std::string plugin_name;
	if(is_scene)
		plugin_name = "scene";
	else 
		plugin_name = it->second.cast<std::string>();
	
	// capitalize the first letter
	if(prop.plugin_name().compare("Diffuse") != 0)
		plugin_name[0] = std::toupper(plugin_name[0]);
	
	prop.set_plugin_name(plugin_name);
	
	// iterate over next props
	if(!is_scene) ++it;
	// check for 
	for(; it != dict.end(); it++) {
		std::string key = it->first.cast<std::string>();
		if(key.compare("rgb") == 0) {
			// parse spectrum
			auto rgb_dict = it->second.cast<py::dict>();
			parse_rgb(pgmr, prop, rgb_dict, within_emitter, within_spectrum);
		} else if(key.compare("spectrum") == 0) {
			// parse spectrum
			auto spec_dict = it->second.cast<py::dict>();
			parse_spectrum(pgmr, prop, spec_dict, within_emitter);
		} else if(key.compare("texture") == 0) {
			// parse texture
			auto tex_dict = it->second.cast<py::dict>();
			parse_texture(pgmr, prop, tex_dict);
		} else {
			auto val = it->second;
			
			// set prop
			SET_PROP(py::bool_, set_bool)
			SET_PROP(py::float_, set_float)
			SET_PROP(py::int_, set_long)
			SET_PROP(std::string, set_string)
			SET_PROP(Properties::Vector3f, set_vector3f)
			SET_PROP(Properties::Point3f, set_point3f)
			SET_PROP(Properties::Transform4f, set_transform)
			SET_PROP(ref<AnimatedTransform>, set_animated_transform)
			SET_PROP(ref<Object>, set_object)

			// check if the val is mitsuba dict
			try {
				auto nested_dict = val.cast<py::dict>();
				bool nested_within_emitter = false, nested_within_spectrum = false;
				std::string parent_class_name = key;
				if(parent_class_name.compare("emitter") == 0)
				  	nested_within_emitter = true;
				if(parent_class_name.compare("rfilter") == 0)
					parent_class_name = "reconstructionFilter";
				
				// capitalize the first letter
				parent_class_name[0] = std::toupper(parent_class_name[0]);
				if(parent_class_name.compare("Bsdf") == 0) parent_class_name = "BSDF";

				Properties nested_prop;
				create_properties(pgmr, nested_prop, nested_dict, nested_within_emitter, nested_within_spectrum);
				const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum >());
				auto obj = pgmr.create_object(nested_prop, class_);
				prop.set_object(key, obj);
				continue;
			} catch (...) {
				Throw("Can't parse ", key, val);
			} 

			
		}
	}
}

NAMESPACE_END(detail)
NAMESPACE_END(plugin)
NAMESPACE_END(mitsuba)

MTS_PY_EXPORT(PluginManager) {
  MTS_PY_IMPORT_TYPES()
  py::class_<PluginManager, std::unique_ptr<PluginManager, py::nodelete>>(m, "PluginManager")
	.def_static_method(PluginManager, instance, py::return_value_policy::reference)
	.def("create", [](PluginManager &pgmr, const py::dict dict) {
		Properties prop;
		
		if(dict.size() != 1)
			Throw("dict size should 1");
		// Assert(dict.size() == 1);
		auto it = dict.begin();
		std::string parent_class_name = it->first.cast<py::str>();
		bool within_emitter = false, within_spectrum = false, is_scene = false;
		
		if(parent_class_name.compare("emitter") == 0)
		  within_emitter = true;
		else if(parent_class_name.compare("rfilter") == 0)
		  parent_class_name = "reconstructionFilter";
		else if(parent_class_name.compare("scene") == 0)
			is_scene = true;

		// capitalize the first letter
		parent_class_name[0] = std::toupper(parent_class_name[0]);
		if(parent_class_name.compare("Bsdf") == 0) parent_class_name = "BSDF";

		auto nested_dict = it->second.cast<py::dict>();
		
		mitsuba::plugin::detail::create_properties(pgmr, prop, nested_dict, within_emitter, within_spectrum, is_scene);
	  	
	  	const Class *class_ = Class::for_name(parent_class_name, mitsuba::detail::get_variant<Float, Spectrum>());
	  	return cast_object((ref<Object>)(pgmr.create_object(prop, class_)));
	}, py::return_value_policy::reference);
}
