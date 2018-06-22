//Copyright (C) 2017 Tim Kuipers
//Copyright (c) 2018 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#include "SierpinskiFillProvider.h"

#include "../utils/math.h"

namespace cura
{


constexpr bool SierpinskiFillProvider::get_constructor;
constexpr bool SierpinskiFillProvider::use_dithering;

SierpinskiFillProvider::SierpinskiFillProvider(const AABB3D aabb_3d, coord_t min_line_distance, const coord_t line_width)
: fractal_config(getFractalConfig(aabb_3d, min_line_distance, false))
, density_provider(new UniformDensityProvider((float)line_width / min_line_distance))
, fill_pattern_for_all_layers(get_constructor, *density_provider, fractal_config.aabb.flatten(), fractal_config.depth, line_width, use_dithering)
{
}

SierpinskiFillProvider::SierpinskiFillProvider(const AABB3D aabb_3d, coord_t min_line_distance, coord_t line_width, std::string cross_subdisivion_spec_image_file)
: fractal_config(getFractalConfig(aabb_3d, min_line_distance, false))
, density_provider(new ImageBasedDensityProvider(cross_subdisivion_spec_image_file, aabb_3d))
, fill_pattern_for_all_layers(get_constructor, *density_provider, fractal_config.aabb.flatten(), fractal_config.depth, line_width, use_dithering)
{
}

float SierpinskiFillProvider::CombinedDensityProvider::operator()(const AABB3D& aabb) const
{
    float density = (*existing_density_provider)(aabb);
    return density * .45 + .05;
    
//     float density = (*existing_density_provider)(aabb);
//     float height_proportion = std::min(1.0f, std::max(0.0f, static_cast<float>(aabb.getMiddle().z) / total_aabb.size().z));
//     float height_modified = square(density * (1.0 - height_proportion) + height_proportion) * 0.9;
// 
//     Point3 middle = Point3(total_aabb.max.x, total_aabb.min.y, total_aabb.max.z);;
//     float distance_density = std::min(1.0, 1.0 / INT2MM((middle - aabb.getMiddle()).vSize() * 3000 / total_aabb.size().vSize()) );
//     return (1 - distance_density) * height_modified * .6 + .1;
    
    // dense inside
//     Point3 middle = total_aabb.getMiddle();
//     return std::min(1.0, 1.0 / INT2MM((middle - aabb.getMiddle()).vSize() * 10000 / total_aabb.size().vSize()) ) * .4;
    
    // dense moutside
//     auto manhattan = [](Point3 in) { return std::abs(in.x) + std::abs(in.y) + std::abs(in.z); };
//     auto l2 = [](Point3 in, double p) { return std::pow(std::pow(INT2MM(in.x) * 1.4, p) + std::pow(INT2MM(in.y) * 1.4, p) + std::pow(INT2MM(in.z) * 1.4, p), 1.0 / p); };
//     Point3 middle = total_aabb.getMiddle();
//     return .3 - std::sqrt(std::min(1.0, 1.0 / (l2(middle - aabb.getMiddle(), 10.0) * 12000 / total_aabb.size().vSize()) )) * .3;

//     return .3 - std::sqrt(std::min(1.0, 1.0 / INT2MM((manhattan(middle - aabb.getMiddle())) * 10000 / total_aabb.size().vSize()) )) * .3;
}

SierpinskiFillProvider::SierpinskiFillProvider(const AABB3D aabb_3d, coord_t min_line_distance, const coord_t line_width, bool)
: fractal_config(getFractalConfig(aabb_3d, min_line_distance, true))
, density_provider(new UniformDensityProvider((float)line_width / min_line_distance))
, subdivision_structure_3d(get_constructor, *density_provider, fractal_config.aabb, fractal_config.depth, line_width)
{
    subdivision_structure_3d->initialize();
//     subdivision_structure_3d->createMinimalDensityPattern();
    subdivision_structure_3d->createDitheredPattern();
//     subdivision_structure_3d->sanitize();
    z_to_start_cell_cross3d = subdivision_structure_3d->getSequenceStarts();
}

SierpinskiFillProvider::SierpinskiFillProvider(const AABB3D aabb_3d, coord_t min_line_distance, const coord_t line_width, std::string cross_subdisivion_spec_image_file, bool)
: fractal_config(getFractalConfig(aabb_3d, min_line_distance, true))
, density_provider(new CombinedDensityProvider(new ImageBasedDensityProvider(cross_subdisivion_spec_image_file, aabb_3d), aabb_3d))
, subdivision_structure_3d(get_constructor, *density_provider, fractal_config.aabb, fractal_config.depth, line_width)
{
    subdivision_structure_3d->initialize();
    subdivision_structure_3d->createMinimalDensityPattern();
//     subdivision_structure_3d->createDitheredPattern();
    subdivision_structure_3d->sanitize();
    z_to_start_cell_cross3d = subdivision_structure_3d->getSequenceStarts();
}

Polygon SierpinskiFillProvider::generate(EFillMethod pattern, coord_t z, coord_t line_width, coord_t pocket_size) const
{
    if (fill_pattern_for_all_layers)
    {
        if (pattern == EFillMethod::CROSS_3D)
        {
            return fill_pattern_for_all_layers->generateCross(z, line_width / 2, pocket_size);
        }
        else
        {
            return fill_pattern_for_all_layers->generateCross();
        }
    }
    else if (subdivision_structure_3d)
    {
        std::map<coord_t, const Cross3D::Cell*>::const_iterator start_cell_iter = z_to_start_cell_cross3d.upper_bound(z);
        if (start_cell_iter != z_to_start_cell_cross3d.begin())
        { // don't get a start cell below the bottom one
            start_cell_iter--; // map.upper_bound always rounds up, while the map contains the min of the z_range of the cells
        }
        Cross3D::SliceWalker slicer_walker = subdivision_structure_3d->getSequence(*start_cell_iter->second, z);
        return subdivision_structure_3d->generateCross(slicer_walker, z);
    }
    else
    {
        Polygon ret;
        logError("Different density sierpinski fill for different layers is not implemented yet!\n");
        std::exit(-1);
        return ret;
    }
}

SierpinskiFillProvider::~SierpinskiFillProvider()
{
    if (density_provider)
    {
        delete density_provider;
    }
}

SierpinskiFillProvider::FractalConfig SierpinskiFillProvider::getFractalConfig(const AABB3D aabb_3d, coord_t min_line_distance, bool make_3d)
{
    Point3 model_aabb_size = aabb_3d.max - aabb_3d.min;
    coord_t max_side_length = std::max(model_aabb_size.x, model_aabb_size.y);
    if (make_3d)
    {
        max_side_length = std::max(max_side_length, model_aabb_size.z);
    }
    Point3 model_middle = aabb_3d.getMiddle();

    int depth = 0;
    coord_t aabb_size = min_line_distance;
    while (aabb_size < max_side_length)
    {
        aabb_size *= 2;
        depth += 2;
    }
    const float half_sqrt2 = .5 * sqrt2;
    if (!make_3d && aabb_size * half_sqrt2 >= max_side_length)
    {
        aabb_size *= half_sqrt2;
        depth--;
    }

    Point3 radius(aabb_size / 2, aabb_size / 2, aabb_size / 2);
    AABB3D aabb(model_middle - radius, model_middle + radius);
    return FractalConfig{depth, aabb};
}



}; // namespace cura
