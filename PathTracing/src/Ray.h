//
// Created by Federico Andrucci on 21/08/23.
//

#ifndef PATHTRACING_RAY_H
#define PATHTRACING_RAY_H

#include <glm/glm.hpp>

struct Ray {
    glm::vec3 Origin;
    glm::vec3 Direction;
};

#endif //PATHTRACING_RAY_H
