#include "SpiralVase.hpp"
#include "GCode.hpp"
#include <sstream>
#include <cmath>
#include <numeric>
#include <limits>

namespace Slic3r {

namespace SpiralVaseHelpers {
/** == Smooth Spiral Helpers == */
/** Distance between a and b */
float distance(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) {
    return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

SpiralVase::SpiralPoint subtract(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b)
{
    return SpiralVase::SpiralPoint(a.x - b.x, a.y - b.y);
}

SpiralVase::SpiralPoint add(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) {
    return SpiralVase::SpiralPoint(a.x + b.x, a.y + b.y);
}

SpiralVase::SpiralPoint scale(SpiralVase::SpiralPoint a, float factor) {
    return SpiralVase::SpiralPoint(a.x * factor, a.y * factor);
}

/** dot product */
float dot(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) {
    return a.x * b.x + a.y * b.y;
}

/** Find the point on line ab closes to point c */
SpiralVase::SpiralPoint nearest_point_on_line(SpiralVase::SpiralPoint c, SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b, float& dist)
{
    SpiralVase::SpiralPoint ab = subtract(b, a);
    SpiralVase::SpiralPoint ac = subtract(c, a);
    float t = dot(ac, ab) / dot(ab, ab);
    t = t > 1 ? 1 : t;
    t = t < 0 ? 0 : t;
    SpiralVase::SpiralPoint closest = SpiralVase::SpiralPoint(add(a, scale(ab, t)));
    dist = distance(c, closest);
    return closest;
}

/** Given a set of lines defined by points such as line[n] is the line from points[n] to points[n+1],
 *  find the closest point to p that falls on any of the lines */
SpiralVase::SpiralPoint nearest_point_on_lines(SpiralVase::SpiralPoint                               p,
                                               std::shared_ptr<std::vector<SpiralVase::SpiralPoint>> points,
                                               bool&                                                 found,
                                               float&                                                dist)
{
    if (points->size() < 2) {
        found = false;
        return SpiralVase::SpiralPoint(0, 0);
    }
    float min = std::numeric_limits<float>::max();
    SpiralVase::SpiralPoint closest(0, 0);
    for (unsigned long i = 0; i < points->size() - 1; i++) {
        float currentDist = 0;
        SpiralVase::SpiralPoint current = nearest_point_on_line(p, points->at(i), points->at(i + 1), currentDist);
        if (currentDist < min) {
            min = currentDist;
            closest = current;
            found = true;
        }
    }
    dist = min;
    return closest;
}
} // namespace SpiralVase

std::string SpiralVase::process_layer(const std::string &gcode, bool last_layer)
{
    /*  This post-processor relies on several assumptions:
        - all layers are processed through it, including those that are not supposed
          to be transformed, in order to update the reader with the XY positions
        - each call to this method includes a full layer, with a single Z move
          at the beginning
        - each layer is composed by suitable geometry (i.e. a single complete loop)
        - loops were not clipped before calling this method  */
    
    // If we're not going to modify G-code, just feed it to the reader
    // in order to update positions.
    if (! m_enabled) {
        m_reader.parse_buffer(gcode);
        return gcode;
    }

    // egon-r fork: multi-wall spiral vase
    if (m_wall_count > 1)
        return process_layer_multi_wall(gcode, last_layer);
    
    // Get total XY length for this layer by summing all extrusion moves.
    float total_layer_length = 0;
    float layer_height = 0;
    float z = 0.f;
    
    {
        //FIXME Performance warning: This copies the GCodeConfig of the reader.
        GCodeReader r = m_reader;  // clone
        bool set_z = false;
        r.parse_buffer(gcode, [&total_layer_length, &layer_height, &z, &set_z]
            (GCodeReader &reader, const GCodeReader::GCodeLine &line) {
            if (line.cmd_is("G1")) {
                if (line.extruding(reader)) {
                    total_layer_length += line.dist_XY(reader);
                } else if (line.has(Z)) {
                    layer_height += line.dist_Z(reader);
                    if (!set_z) {
                        z = line.new_Z(reader);
                        set_z = true;
                    }
                }
            }
        });
    }

    // Remove layer height from initial Z.
    z -= layer_height;

    std::shared_ptr<std::vector<SpiralVase::SpiralPoint>>  current_layer  = std::make_shared<std::vector<SpiralVase::SpiralPoint>>();
    std::shared_ptr<std::vector<SpiralVase::SpiralPoint>> previous_layer = m_previous_layer;

    bool smooth_spiral = m_smooth_spiral;
    std::string new_gcode;
    std::string transition_gcode;
    float max_xy_dist_for_smoothing = m_max_xy_smoothing;
    //FIXME Tapering of the transition layer only works reliably with relative extruder distances.
    // For absolute extruder distances it will be switched off.
    // Tapering the absolute extruder distances requires to process every extrusion value after the first transition
    // layer.
    bool  transition_in = m_transition_layer && m_config.use_relative_e_distances.value;
    bool  transition_out = last_layer && m_config.use_relative_e_distances.value;
    float len = 0.f;
    //set initial point
    SpiralVase::SpiralPoint last_point = previous_layer != NULL && previous_layer->size() > 0 ? previous_layer->at(previous_layer->size()-1): SpiralVase::SpiralPoint(0,0);

    m_reader.parse_buffer(gcode, [&new_gcode, &z, total_layer_length, layer_height, transition_in, &len, &current_layer, &previous_layer, &transition_gcode, transition_out,
                                  smooth_spiral, &max_xy_dist_for_smoothing, &last_point]
        (GCodeReader &reader, GCodeReader::GCodeLine line) {
        if (line.cmd_is("G1")) {
            if (line.has_z()) {
                // If this is the initial Z move of the layer, replace it with a
                // (redundant) move to the last Z of previous layer.
                line.set(reader, Z, z);
                new_gcode += line.raw() + '\n';
                return;
            } else {
                float dist_XY = line.dist_XY(reader);
                if (dist_XY > 0) {
                    if (line.extruding(reader)) { // Exclude wipe and retract
                        len += dist_XY;
                        float factor = len / total_layer_length;
                        if (transition_in)
                            // Transition layer, interpolate the amount of extrusion from zero to the final value.
                            line.set(reader, E, line.e() * factor, 5 /*decimal_digits*/);
                        else if (transition_out) {
                            // We want the last layer to ramp down extrusion, but without changing z height!
                            // So clone the line before we mess with its Z and duplicate it into a new layer that ramps down E
                            // We add this new layer at the very end
                            GCodeReader::GCodeLine transitionLine(line);
                            transitionLine.set(reader, E, line.e() * (1 - factor), 5 /*decimal_digits*/);
                            transition_gcode += transitionLine.raw() + '\n';
                        }
                        // This line is the core of Spiral Vase mode, ramp up the Z smoothly
                        line.set(reader, Z, z + factor * layer_height);
                        if (smooth_spiral) {
                            // Now we also need to try to interpolate X and Y
                            SpiralVase::SpiralPoint p(line.x(), line.y()); // Get current x/y coordinates
                            current_layer->push_back(p); // Store that point for later use on the next layer
                            if (previous_layer != NULL) {
                                bool found = false;
                                float dist = 0;
                                SpiralVase::SpiralPoint nearestp = SpiralVaseHelpers::nearest_point_on_lines(p, previous_layer, found, dist);
                                if (found && dist < max_xy_dist_for_smoothing) {
                                    // Interpolate between the point on this layer and the point on the previous layer
                                    SpiralVase::SpiralPoint target = SpiralVaseHelpers::add(SpiralVaseHelpers::scale(nearestp, 1 - factor), SpiralVaseHelpers::scale(p, factor));

                                    // BBS: remove too short movement
                                    // We need to figure out the distance of this new line!
                                    float modified_dist_XY = SpiralVaseHelpers::distance(last_point, target);
                                    if (modified_dist_XY < 0.001)
                                        line.clear();
                                    else {
                                        line.set(reader, X, target.x);
                                        line.set(reader, Y, target.y);
                                        // Scale the extrusion amount according to change in length
                                        line.set(reader, E, line.e() * modified_dist_XY / dist_XY, 5 /*decimal_digits*/);
                                        last_point = target;
                                    }
                                } else {
                                    last_point = p;
                                }
                            }
                        }
                        new_gcode += line.raw() + '\n';
                    }
                    return;
                    /*  Skip travel moves: the move to first perimeter point will
                        cause a visible seam when loops are not aligned in XY; by skipping
                        it we blend the first loop move in the XY plane (although the smoothness
                        of such blend depend on how long the first segment is; maybe we should
                        enforce some minimum length?).
                        When smooth_spiral is enabled, we're gonna end up exactly where the next layer should
                        start anyway, so we don't need the travel move */
                }
            }
        }
        new_gcode += line.raw() + '\n';
        if(transition_out) {
            transition_gcode += line.raw() + '\n';
        }
    });

    m_previous_layer = current_layer;

    return new_gcode + transition_gcode;
}

std::string SpiralVase::process_layer_multi_wall(const std::string &gcode, bool last_layer)
{
    /*  Multi-wall spiral vase (egon-r fork, EXPERIMENTAL).

        The layer consists of N travel-delimited extrusion runs, one per wall
        loop. Wall r is turned into its own continuous helix of pitch
        layer_height: its Z ramps from (layer_base + r*h/N) up by a full
        layer height over the course of its loop. The move to the next wall's
        start point then drops Z back down by (h - h/N); that drop happens on
        the first (short, seam-blended) extrusion move of the next wall since
        travel moves are dropped, exactly like the classic seam blend.

        Fallback: if the layer does not contain exactly N runs, it is emitted
        unchanged (with discrete layer Z) so a bad layer degrades to a normal
        layer instead of corrupting the print. */

    // Phase A: measure the layer with a cloned reader: base Z, height and
    // per-run extrusion lengths.
    float              z            = 0.f;
    float              layer_height = 0.f;
    std::vector<float> run_lengths;
    {
        GCodeReader r = m_reader; // clone
        bool set_z = false;
        int  run   = -1;
        r.parse_buffer(gcode, [&z, &set_z, &run, &run_lengths, &layer_height](GCodeReader &reader, const GCodeReader::GCodeLine &line) {
            if (line.cmd_is("G1")) {
                float dist_XY = line.dist_XY(reader);
                if (line.extruding(reader)) {
                    if (run < 0) run = 0;
                    if ((size_t) run >= run_lengths.size()) run_lengths.push_back(0.f);
                    run_lengths[run] += dist_XY;
                } else if (dist_XY > 0) {
                    // travel move: the following extrusions belong to the next run
                    ++run;
                } else if (line.has(Z)) {
                    layer_height += line.dist_Z(reader);
                    if (!set_z) { z = line.new_Z(reader); set_z = true; }
                }
            }
        });
    }

    const size_t wall_count = m_wall_count;
    if (run_lengths.size() != wall_count || layer_height <= 0.f) {
        BOOST_LOG_TRIVIAL(warning) << "spiral multi-wall: expected " << wall_count << " runs, got " << run_lengths.size()
                                   << "; emitting layer unmodified";
        m_reader.parse_buffer(gcode);
        return gcode;
    }
    for (float l : run_lengths)
        if (l <= 0.f) {
            BOOST_LOG_TRIVIAL(warning) << "spiral multi-wall: empty run; emitting layer unmodified";
            m_reader.parse_buffer(gcode);
            return gcode;
        }

    // Remove layer height from initial Z: base of wall 0.
    z -= layer_height;

    const float total_layer_length = std::accumulate(run_lengths.begin(), run_lengths.end(), 0.f);
    const bool  transition_in  = m_transition_layer && m_config.use_relative_e_distances.value;
    const bool  transition_out = last_layer && m_config.use_relative_e_distances.value;
    const bool  smooth_spiral  = m_smooth_spiral;
    const float max_xy_dist_for_smoothing = m_max_xy_smoothing;

    std::string new_gcode;
    std::string transition_gcode;

    // Per-run state for emission.
    std::vector<float> run_len(wall_count, 0.f);
    // Per-run point history for smooth spiral interpolation.
    std::vector<std::shared_ptr<std::vector<SpiralPoint>>> current_runs(wall_count);
    std::vector<SpiralPoint>                               last_points(wall_count, SpiralPoint(0, 0));
    std::vector<char>                                      has_last_point(wall_count, 0);
    const std::vector<std::shared_ptr<std::vector<SpiralPoint>>> &previous_runs = m_previous_layer_runs;
    const bool have_previous = previous_runs.size() == wall_count;

    float len_global = 0.f; // cumulative extrusion length across the whole layer (for E ramps)
    int   run        = -1;

    m_reader.parse_buffer(gcode, [&](GCodeReader &reader, GCodeReader::GCodeLine line) {
        if (line.cmd_is("G1")) {
            if (line.has_z()) {
                // Initial Z move of the layer: replace it with a (redundant)
                // move to the base of wall 0.
                line.set(reader, Z, z);
                new_gcode += line.raw() + '\n';
                return;
            }
            float dist_XY = line.dist_XY(reader);
            if (dist_XY > 0) {
                if (line.extruding(reader)) { // Exclude wipe and retract
                    if (run < 0) run = 0;
                    const size_t r = (size_t) run;
                    run_len[r] += dist_XY;
                    len_global += dist_XY;

                    const float factor_global = len_global / total_layer_length;
                    const float factor_run    = run_len[r] / run_lengths[r];
                    // Wall r's own helix: base offset r*h/N, ramping a full layer height.
                    const float wall_base = z + (float) r * layer_height / (float) wall_count;

                    if (transition_in)
                        line.set(reader, E, line.e() * factor_global, 5);
                    else if (transition_out) {
                        GCodeReader::GCodeLine transitionLine(line);
                        transitionLine.set(reader, E, line.e() * (1 - factor_global), 5);
                        transition_gcode += transitionLine.raw() + '\n';
                    }
                    line.set(reader, Z, wall_base + factor_run * layer_height);

                    if (smooth_spiral) {
                        SpiralVase::SpiralPoint p(line.x(), line.y());
                        if (!current_runs[r]) current_runs[r] = std::make_shared<std::vector<SpiralPoint>>();
                        current_runs[r]->push_back(p);
                        if (have_previous && previous_runs[r]) {
                            bool  found = false;
                            float dist  = 0;
                            SpiralVase::SpiralPoint nearestp = SpiralVaseHelpers::nearest_point_on_lines(p, previous_runs[r], found, dist);
                            if (found && dist < max_xy_dist_for_smoothing) {
                                SpiralVase::SpiralPoint target = SpiralVaseHelpers::add(SpiralVaseHelpers::scale(nearestp, 1 - factor_run),
                                                                                         SpiralVaseHelpers::scale(p, factor_run));
                                float modified_dist_XY = has_last_point[r] ? SpiralVaseHelpers::distance(last_points[r], target) : dist;
                                if (modified_dist_XY < 0.001) {
                                    line.clear();
                                } else {
                                    line.set(reader, X, target.x);
                                    line.set(reader, Y, target.y);
                                    line.set(reader, E, line.e() * modified_dist_XY / dist_XY, 5);
                                    last_points[r]    = target;
                                    has_last_point[r] = 1;
                                }
                            } else if (!has_last_point[r]) {
                                last_points[r]    = p;
                                has_last_point[r] = 1;
                            }
                        } else if (!has_last_point[r]) {
                            last_points[r]    = p;
                            has_last_point[r] = 1;
                        }
                    }
                    new_gcode += line.raw() + '\n';
                } else {
                    // travel move - dropped like the classic seam blend; the
                    // following extrusions belong to the next run.
                    ++run;
                }
                return;
            }
        }
        new_gcode += line.raw() + '\n';
        if (transition_out)
            transition_gcode += line.raw() + '\n';
    });

    m_previous_layer_runs = current_runs;

    return new_gcode + transition_gcode;
}

}
