
#include "EstimatorPhase.h"
#include <iomanip>


//#define TINY_REPORT 1
#define NO_DEBUG

EstimatorPhase::EstimatorPhase(std::string name, bool isMetaPhase) :

		graph(nullptr),	// just so eclipse does not nag
		report(),	// initializes all members of report
		name(name),
		config(nullptr),
		isMetaPhase(isMetaPhase) {
}

void EstimatorPhase::generateReport() {

	for(auto node : (*graph)) {

		if(node->isInstrumented()) {
			report.instrumentedMethods += 1;
			report.instrumentedCalls += node->getNumberOfCalls();

			report.instrumentedNames.insert(node->getFunctionName());
		}
		if(node->isUnwound()) {
			unsigned long long unwindSamples;
			if (node->isUnwoundInstr()) {
				unwindSamples = node->getNumberOfCalls();
			} else if (node->isUnwoundSample()) {
				unwindSamples	= node->getExpectedNumberOfSamples();
			} else {
				std::cerr << "Error in generateRepor." << std::endl;
			}
			unsigned long long unwindSteps = node->getNumberOfUnwindSteps();

			unsigned long long unwindCostsNanos = unwindSamples *
					(CgConfig::nanosPerUnwindSample + unwindSteps * CgConfig::nanosPerUnwindStep);

			report.unwindSamples += unwindSamples;
			report.unwindOvSeconds += (double) unwindCostsNanos / 1e9;

			report.unwConjunctions++;
		}
		if (CgHelper::isConjunction(node)) {
			report.overallConjunctions++;
		}
	}

	report.overallMethods = graph->size();

	report.instrOvSeconds = (double) report.instrumentedCalls * CgConfig::nanosPerInstrumentedCall / 1e9;

	if (config->referenceRuntime > .0) {
 		report.instrOvPercent = report.instrOvSeconds / config->referenceRuntime * 100;
 		report.unwindOvPercent = report.unwindOvSeconds / config->referenceRuntime * 100;

 		report.samplesTaken = config->referenceRuntime * CgConfig::samplesPerSecond;
	} else {
		report.instrOvPercent = 0;
		report.samplesTaken = 0;
	}
	// sampling overhead
	report.samplingOvSeconds = report.samplesTaken * CgConfig::nanosPerSample / 1e9;
	report.samplingOvPercent = (double) (CgConfig::nanosPerSample * CgConfig::samplesPerSecond) / 1e7;


	report.metaPhase = isMetaPhase;
	report.phaseName = name;

	assert(report.instrumentedMethods == report.instrumentedNames.size());
}

void EstimatorPhase::setGraph(Callgraph* graph) {
	this->graph = graph;
}

CgReport EstimatorPhase::getReport() {
	return this->report;
}

void EstimatorPhase::printReport() {

	double overallOvPercent = report.instrOvPercent + report.unwindOvPercent + report.samplingOvPercent;

#ifdef TINY_REPORT
	if (!report.metaPhase) {
		std::cout << "==" << report.phaseName << "==  " << overallOvPercent <<" %" << std::endl;
	}
#else
	std::cout << "==" << report.phaseName << "==  " << std::endl;
	printAdditionalReport();
#endif
}

void EstimatorPhase::printAdditionalReport() {

	double overallOvSeconds = report.instrOvSeconds + report.unwindOvSeconds + report.samplingOvSeconds;
	double overallOvPercent = report.instrOvPercent + report.unwindOvPercent + report.samplingOvPercent;

if (report.instrumentedCalls > 0) {
	std::cout
			<< " INSTR \t" <<std::setw(8) << std::left << report.instrOvPercent << " %"
			<< " | instr. " << report.instrumentedMethods << " of " << report.overallMethods << " methods"
			<< " | instrCalls: " << report.instrumentedCalls
			<< " | instrOverhead: " << report.instrOvSeconds << " s" << std::endl;
}
if (report.unwindSamples > 0) {
	std::cout
			<< "   UNW \t" << std::setw(8) << report.unwindOvPercent << " %"
			<< " | unwound " << report.unwConjunctions << " of " << report.overallConjunctions << " conj."
			<< " | unwindSamples: " << report.unwindSamples
			<< " | undwindOverhead: " << report.unwindOvSeconds << " s" << std::endl;
}
	std::cout
			<< " SAMPL \t" << std::setw(8)  << report.samplingOvPercent << " %"
			<< " | taken samples: " << report.samplesTaken
			<< " | samplingOverhead: " << report.samplingOvSeconds << " s" << std::endl

			<< " ---->\t" <<std::setw(8) << overallOvPercent << " %"
			<< " | overallOverhead: " << overallOvSeconds << " s"
			<< std::endl;
}

//// REMOVE UNRELATED NODES ESTIMATOR PHASE

RemoveUnrelatedNodesEstimatorPhase::RemoveUnrelatedNodesEstimatorPhase(bool onlyRemoveUnrelatedNodes, bool aggressiveReduction) :
		EstimatorPhase("RemoveUnrelated", true),
		numUnconnectedRemoved(0),
		numLeafsRemoved(0),
		numChainsRemoved(0),
		numAdvancedOptimizations(0),
		aggressiveReduction(aggressiveReduction),
		onlyRemoveUnrelatedNodes(onlyRemoveUnrelatedNodes) {
}

RemoveUnrelatedNodesEstimatorPhase::~RemoveUnrelatedNodesEstimatorPhase() {
	nodesToRemove.clear();
}

void RemoveUnrelatedNodesEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	if (mainMethod == nullptr) {
		std::cerr << "Received nullptr as main method." << std::endl;
		return;
	}

	/* remove unrelated nodes */
	CgNodePtrSet nodesReachableFromMain;
	std::queue<CgNodePtr> workQueue;
	/** XXX RN: code duplication */
	workQueue.push(mainMethod);
	while(!workQueue.empty()) {

		auto node = workQueue.front();
		workQueue.pop();
		nodesReachableFromMain.insert(node);

		for (auto childNode : node->getChildNodes()) {
			if (nodesReachableFromMain.find(childNode) == nodesReachableFromMain.end()) {
				workQueue.push(childNode);
			}
		}
	}
	for (auto node : (*graph)) {
		// remove nodes that were not reachable from the main method
		if (nodesReachableFromMain.find(node) == nodesReachableFromMain.end()) {
			graph->erase(node, false, true);
			numUnconnectedRemoved++;
			continue;
		}
	}

	if (onlyRemoveUnrelatedNodes) {
		return;
	}

	/* remove leaf nodes */
	for (auto node : (*graph)) {
		// leaf nodes that are never unwound or instrumented
		if (node->isLeafNode()) {
			checkLeafNodeForRemoval(node);
		}
	}
	// actually remove those nodes
	for (auto node : nodesToRemove) {
		graph->erase(node);
	}

	/* remove linear chains */
	for (auto node : (*graph)) {
		if (node->hasUniqueChild()) {

			if (CgHelper::isConjunction(node)) {
				continue;
			}

			auto uniqueChild = node->getUniqueChild();

			if (CgHelper::hasUniqueParent(uniqueChild)
					&& (node->getDependentConjunctions() == uniqueChild->getDependentConjunctions())
					&& !CgHelper::isOnCycle(node)) {

				numChainsRemoved++;

				if (node->getNumberOfCalls() >= uniqueChild->getNumberOfCalls()) {
					graph->erase(node, true);
				} else {
					graph->erase(uniqueChild, true);
				}
			}
		}
	}

	if (!aggressiveReduction) {
		return;
	}

	for (auto node : (*graph)) {
		// advanced optimization (remove node with subset of dependentConjunctions
		if (node->hasUniqueParent()	&& node->hasUniqueChild()
				&& node->getUniqueParent()->getNumberOfCalls() <= node->getNumberOfCalls() ) {

			CgNodePtrSet intersect = CgHelper::setIntersect(node->getUniqueParent()->getDependentConjunctions(), node->getDependentConjunctions());
			if (intersect == node->getDependentConjunctions()) {
				// TODO: also the two nodes have to serve the same conjunctions

				numAdvancedOptimizations++;
				graph->erase(node, true);
			}
		}
	}

}

void RemoveUnrelatedNodesEstimatorPhase::checkLeafNodeForRemoval(CgNodePtr node) {

	for (auto child : node->getChildNodes()) {
		if (nodesToRemove.find(child) == nodesToRemove.end()) {
			return;	// if a single child is not deleted yet, this node cannot be deleted anyways
		}
	}

	if (node->hasUniqueCallPath()
			|| (aggressiveReduction && node->hasUniqueParent()) ) {
		nodesToRemove.insert(node);
		numLeafsRemoved++;

		for (auto parentNode : node->getParentNodes()) {
			checkLeafNodeForRemoval(parentNode);
		}
	}
}

void RemoveUnrelatedNodesEstimatorPhase::printAdditionalReport() {
	std::cout << "\t" << "Removed " << numUnconnectedRemoved << " unconnected node(s)."	<< std::endl;
	std::cout << "\t" << "Removed " << numLeafsRemoved << " leaf node(s)."	<< std::endl;
	std::cout << "\t" << "Removed " << numChainsRemoved << " node(s) in linear call chains."	<< std::endl;
	std::cout << "\t" << "Removed " << numAdvancedOptimizations << " node(s) in advanced optimization."	<< std::endl;
}

//// GRAPH STATS ESTIMATOR PHASE

GraphStatsEstimatorPhase::GraphStatsEstimatorPhase() :
	EstimatorPhase("GraphStats", true),
	numCyclesDetected(0),
	numberOfConjunctions(0) {
}

GraphStatsEstimatorPhase::~GraphStatsEstimatorPhase() {
}

void GraphStatsEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	// detect cycles
	for (auto node : (*graph)) {
		if (CgHelper::isOnCycle(node)) {
			numCyclesDetected++;
		}
	}

	// dependent conjunctions
	for (auto node : (*graph)) {

		if (!CgHelper::isConjunction(node)) {
			continue;
		}

		numberOfConjunctions++;
		if (hasDependencyFor(node)) {
			continue;
		}

		CgNodePtrSet dependentConjunctions = {node};
		CgNodePtrSet validMarkerPositions = node->getMarkerPositions();

		unsigned int numberOfDependentConjunctions = 0;
		while (numberOfDependentConjunctions != dependentConjunctions.size()) {
			numberOfDependentConjunctions = dependentConjunctions.size();

			CgNodePtrSet reachableConjunctions = CgHelper::getReachableConjunctions(validMarkerPositions);
			for (auto depConj : dependentConjunctions) {
				reachableConjunctions.erase(depConj);
			}

			for (auto reachableConjunction : reachableConjunctions) {
				CgNodePtrSet otherValidMarkerPositions = CgHelper::getPotentialMarkerPositions(reachableConjunction);

				if (!CgHelper::setIntersect(validMarkerPositions, otherValidMarkerPositions).empty()) {
					dependentConjunctions.insert(reachableConjunction);
					validMarkerPositions.insert(otherValidMarkerPositions.begin(), otherValidMarkerPositions.end());
				}
			}
		}

		///XXX
//		if (dependentConjunctions.size() > 100) {
//			for (auto& dep : dependentConjunctions) {
//				graph->erase(dep, false, true);
//			}
//			for (auto& marker: allValidMarkerPositions) {
//				graph->erase(marker, false, true);
//			}
//		}

		dependencies.push_back(ConjunctionDependency(dependentConjunctions, validMarkerPositions));
		allValidMarkerPositions.insert(validMarkerPositions.begin(), validMarkerPositions.end());
	}
}

void GraphStatsEstimatorPhase::printAdditionalReport() {
	std::cout << "\t" << "nodes in cycles: " << numCyclesDetected << std::endl;
	std::cout << "\t" << "numberOfConjunctions: " << numberOfConjunctions
			<< " | allValidMarkerPositions: " << allValidMarkerPositions.size() << std::endl;
	for (auto dependency : dependencies) {
		std::cout << "\t- dependentConjunctions: " << std::setw(3) << dependency.dependentConjunctions.size()
				<< " | validMarkerPositions: " << std::setw(3) << dependency.markerPositions.size() << std::endl;
	}
}

//// OVERHEAD COMPENSATION ESTIMATOR PHASE

OverheadCompensationEstimatorPhase::OverheadCompensationEstimatorPhase(int nanosPerHalpProbe) :
	EstimatorPhase("OvCompensation", true),
	overallRuntime(0),
	numOvercompensatedFunctions(0),
	nanosPerHalpProbe(nanosPerHalpProbe) {}

void OverheadCompensationEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	for (auto node : *graph) {
		auto oldRuntime = node->getRuntimeInSeconds();
		unsigned long long numberOfOwnOverheads = node->getNumberOfCalls();
		unsigned long long numberOfChildOverheads = 0;
		for (auto child : node->getChildNodes()) {
			numberOfChildOverheads += child->getNumberOfCalls(node);
		}

		unsigned long long timestampOverheadNanos = numberOfOwnOverheads * nanosPerHalpProbe + numberOfChildOverheads * nanosPerHalpProbe;
		double timestampOverheadSeconds = (double) timestampOverheadNanos / 1e9;
		double newRuntime = oldRuntime - timestampOverheadSeconds;

		if (newRuntime < 0) {
			node->setRuntimeInSeconds(0);
			numOvercompensatedFunctions++;
		} else {
			node->setRuntimeInSeconds(newRuntime);
		}

		node->updateExpectedNumberOfSamples();

		overallRuntime += newRuntime;
	}

	config->actualRuntime = overallRuntime;
}

void OverheadCompensationEstimatorPhase::printAdditionalReport() {
	std::cout << "\t" << "new runtime in seconds: " << overallRuntime
			<< " | overcompensated: " << numOvercompensatedFunctions
			<< std::endl;
}

//// DIAMOND PATTERN SOLVER ESTIMATOR PHASE

DiamondPatternSolverEstimatorPhase::DiamondPatternSolverEstimatorPhase() :
	EstimatorPhase("DiamondPattern"),
	numDiamonds(0),
	numUniqueConjunction(0),
	numOperableConjunctions(0) {
}

DiamondPatternSolverEstimatorPhase::~DiamondPatternSolverEstimatorPhase() {
}

void DiamondPatternSolverEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	for (auto node : (*graph)) {
		if (CgHelper::isConjunction(node)) {
			for (auto parent1 : node->getParentNodes()) {

				if (parent1->hasUniqueParent() && parent1->hasUniqueChild()) {
					auto grandParent = parent1->getUniqueParent();

					for (auto parent2 : node->getParentNodes()) {
						if (parent1==parent2) {
							continue;
						}

						if (parent2->hasUniqueParent()
								&& parent2->getUniqueParent()==grandParent) {

							numDiamonds++;
							break;
						}

					}
				}
			}
		}
	}

	// conjunctions with a unique solution
	for (auto& node : (*graph)) {
		if (CgHelper::isConjunction(node)) {
			int numParents = node->getParentNodes().size();
			int numPossibleMarkerPositions = node->getMarkerPositions().size();

			assert(numPossibleMarkerPositions >= (numParents -1));

			if (numPossibleMarkerPositions == (numParents-1)) {

				///XXX instrument parents
				for (auto& marker : node->getMarkerPositions()) {
					marker->setState(CgNodeState::INSTRUMENT_WITNESS);
				}

				numUniqueConjunction++;
			}

			if (numPossibleMarkerPositions == numParents) {
				numOperableConjunctions++;
			}
		}
	}
}

void DiamondPatternSolverEstimatorPhase::printAdditionalReport() {
	std::cout << "\t" << "numberOfDiamonds: " << numDiamonds << std::endl;
	std::cout << "\t" << "numUniqueConjunction: " << numUniqueConjunction << std::endl;
	std::cout << "\t" << "numOperableConjunctions: " << numOperableConjunctions << std::endl;
}

//// INSTRUMENT ESTIMATOR PHASE

InstrumentEstimatorPhase::InstrumentEstimatorPhase() :
	EstimatorPhase("Instrument") {
}

InstrumentEstimatorPhase::~InstrumentEstimatorPhase() {
}

void InstrumentEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	if (mainMethod == nullptr) {
		std::cerr << "Received nullptr as main method." << std::endl;
		return;
	}

	std::queue<CgNodePtr> workQueue;
	CgNodePtrSet doneNodes;

	/** XXX RN: code duplication */
	workQueue.push(mainMethod);
	while (!workQueue.empty()) {

		auto node = workQueue.front();
		workQueue.pop();
		doneNodes.insert(node);

		if (CgHelper::isConjunction(node)) {
			for (auto nodeToInstrument : node->getParentNodes()) {
				nodeToInstrument->setState(CgNodeState::INSTRUMENT_WITNESS);
			}
		}

		// add child to work queue if not done yet
		for (auto childNode : node->getChildNodes()) {
			if(doneNodes.find(childNode) == doneNodes.end()) {
				workQueue.push(childNode);
			}
		}
	}
}

//// WL INSTR ESTIMATOR PHASE

WLInstrEstimatorPhase::WLInstrEstimatorPhase(std::string wlFilePath) :
		EstimatorPhase("WLInstr") {

	std::ifstream ifStream(wlFilePath);
	if (!ifStream.good()) {
		std::cerr << "Error: can not find whitelist at .. " << wlFilePath << std::endl;
	}

	std::string buff;
	while (getline(ifStream, buff)) {
		whiteList.insert(buff);
	}
}

void WLInstrEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	for (auto node : (*graph)) {
		if (whiteList.find(node->getFunctionName()) != whiteList.end()) {
			node->setState(CgNodeState::INSTRUMENT_WITNESS);
		}
	}
}

//// LIBUNWIND ESTIMATOR PHASE

LibUnwindEstimatorPhase::LibUnwindEstimatorPhase(bool unwindUntilUniqueCallpath) :
		EstimatorPhase(unwindUntilUniqueCallpath ? "LibUnwUnique" : "LibUnwStandard"),
		currentDepth(0),
		unwindUntilUniqueCallpath(unwindUntilUniqueCallpath) {}

void LibUnwindEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	// find max distance from main for every function
	for (auto child : mainMethod->getChildNodes()) {
		visit (mainMethod, child);
	}
}

void LibUnwindEstimatorPhase::visit(CgNodePtr from, CgNodePtr current) {

	if (!(unwindUntilUniqueCallpath && current->hasUniqueCallPath())) {
		currentDepth++;
	}

	visitedEdges.insert(CallGraphEdge(from, current));

	auto unwindSteps = current->getNumberOfUnwindSteps();
	if (currentDepth > unwindSteps) {
		current->setState(CgNodeState::UNWIND_SAMPLE, currentDepth);
	}

	for (CgNodePtr child : current->getChildNodes()) {
		if (visitedEdges.find(CallGraphEdge(current, child)) == visitedEdges.end()) {
			visit(current, child);
		}
	}

	if (!(unwindUntilUniqueCallpath && current->hasUniqueCallPath())) {
		currentDepth--;
	}
}

//// MOVE INSTRUMENTATION UPWARDS ESTIMATOR PHASE

MoveInstrumentationUpwardsEstimatorPhase::MoveInstrumentationUpwardsEstimatorPhase() :
		EstimatorPhase("MoveInstrumentationUpwards"),
		movedInstrumentations(0) {
}

MoveInstrumentationUpwardsEstimatorPhase::~MoveInstrumentationUpwardsEstimatorPhase() {
}

void MoveInstrumentationUpwardsEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	for (auto node : (*graph)) {

		auto nextAncestor = node;

		// If the node was not selected previously, we continue
		if (!nextAncestor->isInstrumentedWitness()) {
			continue;
		}

		auto minimalCalls = nextAncestor;

		// If it was selected, look for a "cheaper" node upwards
		while (CgHelper::hasUniqueParent(nextAncestor)) {

			nextAncestor = CgHelper::getUniqueParent(nextAncestor);
			if (nextAncestor->getChildNodes().size()>1) {
				break;	// don't move if parent has multiple children
			}

			if (nextAncestor->getNumberOfCalls() < minimalCalls->getNumberOfCalls()) {
				minimalCalls = nextAncestor;
			}
		}

		if (!minimalCalls->isSameFunction(nextAncestor)) {
			node->setState(CgNodeState::NONE);
			minimalCalls->setState(CgNodeState::INSTRUMENT_WITNESS);
			movedInstrumentations++;
		}
	}
}

//// DELETE ONE INSTRUMENTATION ESTIMATOR PHASE

DeleteOneInstrumentationEstimatorPhase::DeleteOneInstrumentationEstimatorPhase() :
		EstimatorPhase("DeleteOneInstrumentation"),
		deletedInstrumentationMarkers(0) {
}

DeleteOneInstrumentationEstimatorPhase::~DeleteOneInstrumentationEstimatorPhase() {
}

void DeleteOneInstrumentationEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	std::priority_queue<CgNodePtr, std::vector<CgNodePtr>, CalledMoreOften> pq;
	for (auto node : (*graph)) {
		if (node->isInstrumentedWitness()) {
			pq.push(node);
		}
	}

	while (!pq.empty()) {
		auto node = pq.top();
		pq.pop();

		if (CgHelper::instrumentationCanBeDeleted(node)) {
			node->setState(CgNodeState::NONE);
			deletedInstrumentationMarkers++;
		}
	}
}

void DeleteOneInstrumentationEstimatorPhase::printAdditionalReport() {
	EstimatorPhase::printAdditionalReport();
#ifndef TINY_REPORT
	std::cout << "\t" << "deleted " << deletedInstrumentationMarkers << " instrumentation marker(s)" << std::endl;
#endif
}

//// CONJUNCTION ESTIMATOR PHASE

ConjunctionEstimatorPhase::ConjunctionEstimatorPhase(bool instrumentOnlyConjunctions) :
		EstimatorPhase(instrumentOnlyConjunctions ? "ConjunctionOnly" : "ConjunctionOrWitness"), instrumentOnlyConjunctions(instrumentOnlyConjunctions) {}

ConjunctionEstimatorPhase::~ConjunctionEstimatorPhase() {}

void ConjunctionEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	if (instrumentOnlyConjunctions) {
		for (auto node : (*graph)) {
			if (CgHelper::isConjunction(node)) {
				node->setState(CgNodeState::INSTRUMENT_CONJUNCTION);
			}
		}
	} else {

		// either instrument all parents or the conjunction itself

		std::queue<CgNodePtr> workQueue;
		CgNodePtrSet doneNodes;

		/** XXX RN: code duplication */
		workQueue.push(mainMethod);
		while (!workQueue.empty()) {

			auto node = workQueue.front();
			workQueue.pop();
			doneNodes.insert(node);

			if (CgHelper::isConjunction(node)) {

				if (!CgHelper::isUniquelyInstrumented(node)) {
					unsigned long long conjunctionCallsToInstrument = node->getNumberOfCalls();
					unsigned long long parentCallsToInstrument = 0;
					for (auto parent : node->getParentNodes()) {
						if (!parent->isInstrumentedWitness()) {
							parentCallsToInstrument += parent->getNumberOfCalls();
						}
					}

					if (parentCallsToInstrument <= conjunctionCallsToInstrument) {
						for (auto parent : node->getParentNodes()) {
							parent->setState(CgNodeState::INSTRUMENT_WITNESS);
						}
					} else {
						node->setState(CgNodeState::INSTRUMENT_CONJUNCTION);
					}
				}
			}

			// add child to work queue if not done yet
			for (auto childNode : node->getChildNodes()) {
				if (doneNodes.find(childNode) == doneNodes.end()) {
					workQueue.push(childNode);
				}
			}
		}
	}
}

//// UNWIND ESTIMATOR PHASE

UnwindEstimatorPhase::UnwindEstimatorPhase(bool unwindInInstr) :
		EstimatorPhase(unwindInInstr ? "UnwindInstr" : "UnwindSample"),
		unwoundNodes(0),
		unwindCandidates(0),
		unwindInInstr(unwindInInstr) {
}

UnwindEstimatorPhase::~UnwindEstimatorPhase() {
}

void UnwindEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	double overallSavedSeconds = .0;

	for (auto node : (*graph)) {

		if (CgHelper::isConjunction(node) && (node->isLeafNode() || unwindInInstr)) {

			unwindCandidates++;

			// unwind in sample
			unsigned long long expectedUnwindSampleOverheadNanos =
					node->getExpectedNumberOfSamples() * (CgConfig::nanosPerUnwindSample + CgConfig::nanosPerUnwindStep);
			// unwind in instrumentation
			unsigned long long expectedUnwindInstrumentOverheadNanos =
					node->getNumberOfCalls() * (CgConfig::nanosPerUnwindSample + CgConfig::nanosPerUnwindStep);
			unsigned long long unwindOverhead;

			if (unwindInInstr) {
				unwindOverhead = expectedUnwindInstrumentOverheadNanos;
			} else {
				unwindOverhead = expectedUnwindSampleOverheadNanos;
			}

			// optimistic
			unsigned long long expectedInstrumentationOverheadNanos =
					CgHelper::getInstrumentationOverheadOfConjunction(node);
			// pessimistic
			unsigned long long expectedActualInstrumentationSavedNanos =
					CgHelper::getInstrumentationOverheadServingOnlyThisConjunction(node);

			unsigned long long instrumentationOverhead =
					(expectedInstrumentationOverheadNanos + expectedActualInstrumentationSavedNanos) / 2;

			if (unwindOverhead < instrumentationOverhead) {

#ifndef NO_DEBUG
				///XXX
				double expectedOverheadSavedSeconds
						= ((long long) instrumentationOverhead - (long long)unwindOverhead) / 1000000000.0;
				if (expectedOverheadSavedSeconds > 0.1) {
					std::cout << std::setprecision(4) << expectedOverheadSavedSeconds << "s\t save expected in: " << node->getFunctionName() << std::endl;
				}
				overallSavedSeconds += expectedOverheadSavedSeconds;
#endif
				if (unwindInInstr) {
					node->setState(CgNodeState::UNWIND_INSTR, 1);
				} else {
					node->setState(CgNodeState::UNWIND_SAMPLE, 1);
				}
				unwoundNodes++;

			// remove redundant instrumentation in direct parents
				for (auto parentNode : node->getParentNodes()) {

					bool redundantInstrumentation = true;
					for (auto childOfParentNode : parentNode->getChildNodes()) {

						if (!childOfParentNode->isUnwound()
								&& CgHelper::isConjunction(childOfParentNode)) {
							redundantInstrumentation = false;
						}
					}

					if (redundantInstrumentation) {
						CgHelper::removeInstrumentationOnPath(parentNode);
					}
				}
			}
		}
	}

#ifndef NO_DEBUG
	std::cout << overallSavedSeconds << " s maximum save through unwinding." << std::endl;
#endif
}

void UnwindEstimatorPhase::printAdditionalReport() {
	EstimatorPhase::printAdditionalReport();
#ifndef TINY_REPORT
	std::cout << "\t" << "unwound " << unwoundNodes << " leaf node(s) of "
			<< unwindCandidates << " candidate(s)" << std::endl;
#endif
}

//// UNWIND ESTIMATOR PHASE

ResetEstimatorPhase::ResetEstimatorPhase() :
		EstimatorPhase("Reset") {
}

ResetEstimatorPhase::~ResetEstimatorPhase() {
}

void ResetEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {
	for (auto node : (*graph)) {
		node->reset();
	}
}

void ResetEstimatorPhase::printReport() {
	std::cout << "==" << report.phaseName << "== Phase " << std::endl << std::endl;
}


