/***********************************************************************************
Copyright (c) 2017, Michael Neunert, Markus Giftthaler, Markus Stäuble, Diego Pardo,
Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
 * Neither the name of ETH ZURICH nor the names of its contributors may be used
      to endorse or promote products derived from this software without specific
      prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL ETH ZURICH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************************/

#pragma once

namespace ct{
namespace optcon{

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::GNMS(std::shared_ptr<Backend_t>& backend_, const Settings_t& settings) :
BASE(backend_)
{}

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::~GNMS()
{}

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::configure(const Settings_t& settings)
{
	this->backend_->configure(settings);
}

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::setInitialGuess(const Policy_t& initialGuess)
{
	this->backend_->setInitialGuess(initialGuess);
}

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
bool GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::runIteration()
{
	prepareIteration();

	return finishIteration();
}

template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::prepareIteration()
{
	bool debugPrint = this->backend_->getSettings().debugPrint;

	auto startPrepare = std::chrono::steady_clock::now();

	if (!this->backend_->isInitialized())
		throw std::runtime_error("GNMS is not initialized!");

	if (!this->backend_->isConfigured())
		throw std::runtime_error("GNMS is not configured!");

	this->backend_->checkProblem();

	int K = this->backend_->getNumSteps();
	int K_shot = this->backend_->getNumStepsPerShot();

	// if first iteration, compute shots and rollout and cost!
	if(this->backend_->iteration() == 0)
	{
		this->backend_->rolloutShots(K_shot, K-1);
	}


	auto start = std::chrono::steady_clock::now();
	this->backend_->computeLinearizedDynamicsAroundTrajectory(K_shot, K-1);
	auto end = std::chrono::steady_clock::now();
	auto diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Linearizing from index " << K_shot << " to N-1 took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	start = std::chrono::steady_clock::now();
	this->backend_->computeQuadraticCostsAroundTrajectory(K_shot, K-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Cost computation for index " << K_shot << " to N-1 took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	if(debugPrint)
		std::cout<<"[GNMS]: Solving prepare stage of LQOC Problem"<<std::endl;

	start = std::chrono::steady_clock::now();
	this->backend_->prepareSolveLQProblem(K_shot);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Prepare phase of LQOC problem took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;

	auto endPrepare = std::chrono::steady_clock::now();
	if(debugPrint)
		std::cout << "[GNMS]: prepareIteration() took "<<std::chrono::duration <double, std::milli> (endPrepare-startPrepare).count() << " ms" << std::endl;

} //! prepareIteration()


template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
bool GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::finishIteration()
{
	int K_shot = this->backend_->getNumStepsPerShot();

	bool debugPrint = this->backend_->getSettings().debugPrint;

	auto startFinish = std::chrono::steady_clock::now();

	// if first iteration, compute shots and rollout and cost!
	if(this->backend_->iteration() == 0)
	{
		this->backend_->rolloutShots(0, K_shot-1);
		this->backend_->updateCosts();
		this->backend_->computeDefectsNorm();
	}

#ifdef MATLAB_FULL_LOG
	if (this->backend_->iteration() == 0)
		this->backend_->logInitToMatlab();
#endif

	auto start = std::chrono::steady_clock::now();
	this->backend_->computeLinearizedDynamicsAroundTrajectory(0, K_shot-1);
	auto end = std::chrono::steady_clock::now();
	auto diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Linearizing for first shot took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	start = std::chrono::steady_clock::now();
	this->backend_->computeQuadraticCostsAroundTrajectory(0, K_shot-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Cost computation for first shot took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	if(debugPrint)
		std::cout<<"[GNMS]: Finish phase LQOC Problem"<<std::endl;

	start = std::chrono::steady_clock::now();
	this->backend_->finishSolveLQProblem(K_shot-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Finish solving LQOC problem took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;



	//! update solutions
	this->backend_->getFeedback();
	this->backend_->getControlUpdates();
	this->backend_->getStateUpdates();


	start = std::chrono::steady_clock::now();
	bool foundBetter = this->backend_->lineSearchMultipleShooting();
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS]: Line search took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	if(debugPrint){
		auto endFinish = std::chrono::steady_clock::now();
		std::cout << "[GNMS]: finishIteration() took "<<std::chrono::duration <double, std::milli> (endFinish-startFinish).count() << " ms" << std::endl;
	}


	this->backend_->printSummary();

#ifdef MATLAB_FULL_LOG
	this->backend_->logToMatlab(this->backend_->iteration());
#endif //MATLAB_FULL_LOG

	this->backend_->iteration()++;

	return foundBetter;

} //! finishIteration()


template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
void GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::prepareMPCIteration()
{
	bool debugPrint = this->backend_->getSettings().debugPrint;

	auto startPrepare = std::chrono::steady_clock::now();

	if (!this->backend_->isInitialized())
		throw std::runtime_error("GNMS is not initialized!");

	if (!this->backend_->isConfigured())
		throw std::runtime_error("GNMS is not configured!");

	this->backend_->checkProblem();

	int K = this->backend_->getNumSteps();
	int K_shot = this->backend_->getNumStepsPerShot();

	this->backend_->resetDefects();

	this->backend_->rolloutShots(K_shot, K-1);

	auto start = std::chrono::steady_clock::now();
	this->backend_->computeLinearizedDynamicsAroundTrajectory(K_shot, K-1);
	auto end = std::chrono::steady_clock::now();
	auto diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Linearizing from index " << K_shot << " to N-1 took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	start = std::chrono::steady_clock::now();
	this->backend_->computeQuadraticCostsAroundTrajectory(K_shot, K-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Cost computation for index " << K_shot << " to N-1 took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	if(debugPrint)
		std::cout<<"[GNMS-MPC]: Solving prepare stage of LQOC Problem"<<std::endl;

	start = std::chrono::steady_clock::now();
	this->backend_->prepareSolveLQProblem(K_shot);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Prepare phase of LQOC problem took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	auto endPrepare = std::chrono::steady_clock::now();
	if(debugPrint)
		std::cout << "[GNMS-MPC]: prepareIteration() took "<<std::chrono::duration <double, std::milli> (endPrepare-startPrepare).count() << " ms" << std::endl;


} //! prepareMPCIteration()



template <size_t STATE_DIM, size_t CONTROL_DIM, size_t P_DIM, size_t V_DIM, typename SCALAR>
bool GNMS<STATE_DIM, CONTROL_DIM, P_DIM, V_DIM, SCALAR>::finishMPCIteration()
{
	int K_shot = this->backend_->getNumStepsPerShot();

	bool debugPrint = this->backend_->getSettings().debugPrint;

	auto startFinish = std::chrono::steady_clock::now();


	this->backend_->rolloutShots(0, K_shot-1);
	this->backend_->updateCosts();   		//! todo: replace by a simple sum after computeQuadraticCostsAround....
	this->backend_->computeDefectsNorm();	//! todo: we might not need this in MPC


#ifdef MATLAB_FULL_LOG
	if (this->backend_->iteration() == 0)
		this->backend_->logInitToMatlab();
#endif

	auto start = std::chrono::steady_clock::now();
	this->backend_->computeLinearizedDynamicsAroundTrajectory(0, K_shot-1);
	auto end = std::chrono::steady_clock::now();
	auto diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Linearizing for first shot took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	start = std::chrono::steady_clock::now();
	this->backend_->computeQuadraticCostsAroundTrajectory(0, K_shot-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Cost computation for first shot took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;

	if(debugPrint)
		std::cout<<"[GNMS-MPC]: Finish phase LQOC Problem"<<std::endl;

	start = std::chrono::steady_clock::now();
	this->backend_->finishSolveLQProblem(K_shot-1);
	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Finish solving LQOC problem took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	//! update solutions
	start = std::chrono::steady_clock::now();
	this->backend_->getFeedback();
	this->backend_->getControlUpdates();
	this->backend_->getStateUpdates();

	//!update state and controls, no line-search, overwriting happens only at next rollout
	this->backend_->doFullStepUpdate();

	end = std::chrono::steady_clock::now();
	diff = end - start;
	if(debugPrint)
		std::cout << "[GNMS-MPC]: Solution update took "<<std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;


	if(debugPrint){
		auto endFinish = std::chrono::steady_clock::now();
		std::cout << "[GNMS-MPC]: finishIteration() took "<<std::chrono::duration <double, std::milli> (endFinish-startFinish).count() << " ms" << std::endl;
	}

	this->backend_->printSummary();

#ifdef MATLAB_FULL_LOG
	this->backend_->logToMatlab(this->backend_->iteration());
#endif //MATLAB_FULL_LOG

	this->backend_->iteration()++;

	return true; // note: will always return foundBetter

}	//! finishIteration()

}	// namespace optcon
}	// namespace ct

