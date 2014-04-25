#include <fstream>
#include "callgraph.h"
#include "Cube.h"
#include "CubeMetric.h"
#include <cstdlib>

#include <cassert>

#define VERBOSE 0
#define PRINT_DOT 0

int main(int argc, char** argv){


Callgraph cg;
// The cube documentation says one should always use a try/catch thing around
try{
	// Create cube instance
	cube::Cube cube;
	// Read our cube file
	cube.openCubeReport( argv[1] );
	std::cout << "Reading input.. done." << std::endl;

	// Get the cube nodes
	const std::vector<cube::Cnode*>& cnodes = cube.get_cnodev();

	// Just see what is our first node
	std::cout << cnodes[0]->get_callee()->get_name() << std::endl;

	cube::Metric* metric = cube.get_met("visits");
	const std::vector<cube::Thread*> threads = cube.get_thrdv();

	for(auto node : cnodes){
		// I don't know when this happens, but it does.
		if(node->get_parent() == NULL)
			continue;
		// Put the caller/callee pair into our callgraph
		cg.putFunction(node->get_parent()->get_callee()->get_name(), node->get_parent()->get_callee()->get_mod(), node->get_parent()->get_callee()->get_begn_ln(), node->get_callee()->get_name(), cube.get_sev(metric, node, threads.at(0)));
		// Also keep track of threading things...
		if(threads.size() > 1)
			for(int i = 1; i < threads.size(); i++){
				cg.putFunction(node->get_parent()->get_callee()->get_name(), node->get_parent()->get_callee()->get_mod(), node->get_parent()->get_callee()->get_begn_ln(), node->get_callee()->get_name(), cube.get_sev(metric, node, threads.at(i)));
}
	}
	std::cout << "Finished construction of cg. Now estimating InstROs overhead..." << std::endl;

#if PRINT_DOT == 1
	cg.printDOT();
#endif

	/** JP: This is code to estimate the generated overhead via call-graph guided hook placement. */
	bool once = true; // What exactly is that good for...?
	const int overheadPerCallInNanos = 4; 
	unsigned long long overAllOverhead = 0;
		
#if VERBOSE > 1
	std::cout << "Graph includes: " << cg.getSize() << std::endl;
	for(auto node : cg.getNodesToMark()){
		node->printMinimal();
		std::cout << "\n";
	}
#endif
	// sum up the calls to function we would instrument
	for(auto node : cg.getNodesToMark()){
		overAllOverhead += overheadPerCallInNanos * node->getNumberOfCalls();
	}
	std::cout << " ------ Statistics ------ \nA cg-analysis instrumentation would mark: " << cg.getNodesToMark().size() << " out of " << cg.getSize() << "\n" ;
	std::cout << "Adding: " << overAllOverhead << " nanos" << std::endl;
	std::cout << "In Seconds: " << (overAllOverhead / (1e9)) << std::endl;

} catch(cube::RuntimeError e){
	std::cout << e.get_msg() << std::endl;
}

return EXIT_SUCCESS;
}
