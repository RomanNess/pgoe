
#include "Callgraph.h"

#include <cube/Cube.h>
#include <cube/CubeMetric.h>

#include <string>

#ifndef CUBEREADER_H_
#define CUBEREADER_H_

/**
 * \author roman
 * \author JPL
 */
namespace CubeCallgraphBuilder {

	Callgraph build(std::string filePath, int samplesPerSecond);

};



#endif