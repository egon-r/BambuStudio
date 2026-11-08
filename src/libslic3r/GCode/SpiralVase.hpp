#ifndef slic3r_SpiralVase_hpp_
#define slic3r_SpiralVase_hpp_

#include "../libslic3r.h"
#include "../GCodeReader.hpp"

namespace Slic3r {

class SpiralVase
{
public:
    class SpiralPoint
    {
    public:
        SpiralPoint(float paramx, float paramy) : x(paramx), y(paramy) {}

    public:
        float x, y;
    };
    SpiralVase(const PrintConfig &config) : m_config(config)
    {
        //BBS
        //m_reader.z() = (float)m_config.z_offset;
        m_reader.z() = 0.0f;
        m_reader.apply_config(m_config);
        m_previous_layer = NULL;
        m_smooth_spiral = config.spiral_mode_smooth;
    };

    void 		enable(bool en) {
   		m_transition_layer = en && ! m_enabled;
    	m_enabled 		   = en;
    }

    std::string process_layer(const std::string &gcode, bool last_layer);
    // egon-r fork: multi-wall spiral vase. Each travel-delimited extrusion run
    // of the layer becomes its own continuous helix; walls are offset in Z by
    // layer_height / wall_count and the nozzle hops Z down between walls.
    void set_wall_count(size_t n) { m_wall_count = n; }
    void set_max_xy_smoothing(float max) {
        m_max_xy_smoothing = max;
    }
private:
    std::string process_layer_multi_wall(const std::string &gcode, bool last_layer);
private:
    const PrintConfig  &m_config;
    GCodeReader 		m_reader;
    float               m_max_xy_smoothing = 0.f;

    bool 				m_enabled = false;
    // First spiral vase layer. Layer height has to be ramped up from zero to the target layer height.
    bool 				m_transition_layer = false;
    // Whether to interpolate XY coordinates with the previous layer. Results in no seam at layer changes
    bool                m_smooth_spiral = false;
    // Number of walls for multi-wall spiral mode (1 = classic single-wall behavior).
    size_t              m_wall_count { 1 };
    // Per-run (per-wall) point history for smooth spiral interpolation in multi-wall mode.
    std::vector<std::shared_ptr<std::vector<SpiralPoint>>> m_previous_layer_runs;
    std::shared_ptr<std::vector<SpiralPoint>> m_previous_layer;
};
}

#endif // slic3r_SpiralVase_hpp_
