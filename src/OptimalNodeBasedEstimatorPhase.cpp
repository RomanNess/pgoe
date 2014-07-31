
#include "OptimalNodeBasedEstimatorPhase.h"

#define DEBUG 0

OptimalNodeBasedEstimatorPhase::OptimalNodeBasedEstimatorPhase() :
		EstimatorPhase("OptimalNodeBased"),
		optimalCosts(INT64_MAX),
		numberOfSteps(0) {
}

OptimalNodeBasedEstimatorPhase::~OptimalNodeBasedEstimatorPhase() {
}

void OptimalNodeBasedEstimatorPhase::step() {

	if (stateStack.empty()) {
		return;
	}

	// skip already visited combinations
	std::size_t hash = std::hash<OptimalNodeBasedState>()(stateStack.top());
	if (visitedCombinations.find(hash) != visitedCombinations.end()) {

		return;	// this combination has already been visited
	}
	visitedCombinations.insert(hash);

	numberOfSteps++;

#if DEBUG
	std::cout << "+ push " << stateStack.top() << std::endl;
#endif

	for (auto node : stateStack.top().nodeSet) {

		if (node->isRootNode()) {
			continue;
		}

		auto parentNodes = node->getParentNodes();
		auto newState(stateStack.top());

#if DEBUG
		std::cout << "   " << "+ try switching " << *node	<< " for:  [";
		for (auto n : parentNodes) {
			std::cout << *n << ", ";
		}
		std::cout << "] ";
#endif

		if (!parentNodes.empty() && newState.validAfterExchange(node, parentNodes)) {
			unsigned long long costs = newState.getCosts();

			if (costs < optimalCosts) {

				optimalCosts = costs;
				optimalInstrumentation = newState.nodeSet;
			}
#if DEBUG
			std::cout << "--> success" << std::endl;
#endif

			stateStack.push(newState);
			step();

		} else {
#if DEBUG
			std::cout << "--> fail" << std::endl;
#endif
		}
	}

	stateStack.pop();
#if DEBUG
	if (!stateStack.empty()) {
			std::cout << "+ pop  " << stateStack.top() << std::endl;
	}
#endif

}

void OptimalNodeBasedEstimatorPhase::modifyGraph(CgNodePtr mainMethod) {

	findStartingState(mainMethod);

	step();

	for (auto node : optimalInstrumentation) {
		node->setState(CgNodeState::INSTRUMENT);
	}
	mainMethod->setState(CgNodeState::NONE);	// main() is implicitly instrumented

}

void OptimalNodeBasedEstimatorPhase::printAdditionalReport() {
	std::cout << "\t" << "computation steps taken: " << numberOfSteps << std::endl;
}

/** fill the initial stack with all parents of conjunctions and all initial constraints */
void OptimalNodeBasedEstimatorPhase::findStartingState(CgNodePtr mainMethod) {

	CgNodePtrSet startingParents = {mainMethod};	// main() is implicitly instrumented
	ConstraintContainer startingConstraints;

	for (auto node : (*graph)) {
		if (CgHelper::isConjunction(node)) {

			CgNodePtrSet currentParents;

			for (auto parentNode : node->getParentNodes()) {
				currentParents.insert(parentNode);
			}

			startingParents.insert(currentParents.begin(), currentParents.end());
			startingConstraints.push_back(OptimalNodeBasedConstraint(currentParents));
		}
	}

	auto startingState = OptimalNodeBasedState(startingParents, startingConstraints);

	optimalInstrumentation = startingParents;
	optimalCosts = startingState.getCosts();

	stateStack.push(startingState);
}

