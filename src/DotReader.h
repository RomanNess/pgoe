
#include "Callgraph.h"

#include <string>
#include <fstream>

#ifndef DOTREADER_H_
#define DOTREADER_H_

/**
 * \author roman
 */
namespace DOTCallgraphBuilder {


	std::string extractBetween(const std::string& s, const std::string& pattern, size_t& start) {
		size_t first = s.find(pattern, start) + pattern.size();
		size_t second = s.find(pattern, first);

		start = second + pattern.size();
		return s.substr(first, second-first);
	}

	Callgraph build(std::string filePath, int samplesPerSecond) {
		Callgraph* cg = new Callgraph(samplesPerSecond);

		std::ifstream file(filePath);
		std::string line;

		while (std::getline(file, line)) {
			if (!line.find('"') == 0) {	// starts with "
				continue;
			}

			if (line.find("->") != std::string::npos) {
				// edge
				size_t start = 0;
				std::string parent = extractBetween(line, "\"", start);
				std::string child = extractBetween(line, "\"", start);

				size_t numCallsStart = line.find("label=")+6;
				unsigned long numCalls = stoul(line.substr(numCallsStart));

				cg->putEdge(parent, "", -1, child, numCalls, 0.0);

				///XXX
				std::cout << parent << " -> " << child << " : " << numCalls << std::endl;
			} else {
				// node
				size_t start = 0;
				std::string name = extractBetween(line, "\"", start);
				double time = stod(extractBetween(line, "\\n", start));

				cg->findOrCreateNode(name, time);

				///XXX
				std::cout << "node " << name << " : " << time << std::endl;
			}
		}

		file.close();

		return *cg;
	}
};



#endif
