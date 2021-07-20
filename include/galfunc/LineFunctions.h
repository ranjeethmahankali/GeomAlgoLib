#pragma once

#include <galcore/Line.h>
#include <galfunc/Functions.h>

namespace gal {
namespace func {

GAL_FUNC_DECL(line2d,
              2,
              1,
              "Creates a 2d line from the given 2d points.",
              ((glm::vec2, start, "First point of the line."),
               (glm::vec2, end, "Second point of the line.")),
              ((gal::Line2d, line, "The line.")));

}
}  // namespace gal

#define GAL_LineFunctions line2d