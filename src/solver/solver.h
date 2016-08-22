#ifndef _SOLVER_H
#define _SOLVER_H
#include "common.h"
#include "utils.h"
#include "mesh.h"
#include "linearsolver.h"
#include "config.h"
#include "io.h"
#include "eulerequation.h"

template<class T, class Tad>
class Solver{
public:
	int nnz;
	int repeat = 0;
	unsigned int *rind = nullptr;
	unsigned int *cind = nullptr;
	double *values = nullptr;
	int options[4] = {0,0,0,0};
	Timer timer_la;
	Timer timer_main;
	Timer timer_residual;
	Mesh<T> *mesh;
	void solve();
	double UNDER_RELAXATION;
	double CFL;
	Config *config;
	std::string label;
	std::shared_ptr<spdlog::logger> logger;
	std::shared_ptr<spdlog::logger> logger_convergence;	
	std::shared_ptr<IOManager> iomanager;

	EulerEquation<T, Tad> *equation;

#if defined(ENABLE_ARMA)
	LinearSolverArma *linearsolver;
#endif
#if defined(ENABLE_EIGEN)
	LinearSolverEigen *linearsolver;
#endif
#if defined(ENABLE_PETSC)
	LinearSolverPetsc *linearsolver;
#endif
	
	T *rhs;
	T **lhs;
	T *dt;
	T *q;
	Tad *a_q_ravel, *a_rhs_ravel;

	Solver(Mesh<T> *val_mesh, Config *config);
	~Solver();
	void copy_from_solution();
	void copy_to_solution();
	void calc_dt();
	void initialize();
};
template <class T, class Tad>
void Solver<T, Tad>::calc_dt(){
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;

	for(uint i=0; i<mesh->nic; i++){
		for(uint j=0; j<mesh->njc; j++){
			double rho = q[i*njc*nq + j*nq + 0];
			double u = q[i*njc*nq + j*nq + 1]/rho;
			double v = q[i*njc*nq + j*nq + 2]/rho;
			double rhoE = q[i*njc*nq + j*nq + 3];
			double p = (rhoE - 0.5*rho*(u*u + v*v))*(GAMMA-1.0);
			double lambda = sqrt(GAMMA*p/rho) + abs(u) + abs(v);
			dt[i*njc + j] = std::min(mesh->ds_eta[i][j], mesh->ds_chi[i][j])/lambda*CFL;
		}
	}

}
template <class T, class Tad>
void Solver<T, Tad>::copy_from_solution(){
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;

	for(uint i=0; i<mesh->nic; i++){
		for(uint j=0; j<mesh->njc; j++){
			for(uint k=0; k<mesh->solution->nq; k++){
				q[i*njc*nq + j*nq + k] = mesh->solution->q[i][j][k];
			}
		}
	}
}

template <class T, class Tad>
void Solver<T, Tad>::copy_to_solution(){
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;

	for(uint i=0; i<mesh->nic; i++){
		for(uint j=0; j<mesh->njc; j++){
			for(uint k=0; k<mesh->solution->nq; k++){
				mesh->solution->q[i][j][k] = q[i*njc*nq + j*nq + k];
			}
		}
	}
}

template <class T, class Tad>
void Solver<T, Tad>::initialize(){
	uint nic = mesh->nic;
	uint njc = mesh->njc;

	for(uint i=0; i<nic; i++){
		for(uint j=0; j<njc; j++){
			auto rho_inf = config->freestream->rho_inf;
			auto u_inf = config->freestream->u_inf;
			auto v_inf = config->freestream->v_inf;
			auto p_inf = config->freestream->p_inf;
			mesh->solution->q[i][j][0] = rho_inf;
			mesh->solution->q[i][j][1] = rho_inf*u_inf;
			mesh->solution->q[i][j][2] = rho_inf*v_inf;
			mesh->solution->q[i][j][3] = p_inf/(GAMMA-1.0) + 0.5*rho_inf*(u_inf*u_inf + v_inf*v_inf);
		}
	}
	if(config->io->restart){
		iomanager->read_restart();
	}
}

template <class T, class Tad>
Solver<T, Tad>::Solver(Mesh<T> *val_mesh, Config *val_config){
	config = val_config;
	timer_la = Timer();
	timer_main = Timer();
	timer_residual = Timer();
	mesh = val_mesh;
	uint ni = mesh->ni;
	uint nj = mesh->nj;
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;
 
	
	dt = allocate_1d_array<T>(nic*njc);
	rhs = allocate_1d_array<T>(nic*njc*nq);
	q = allocate_1d_array<T>(nic*njc*nq);
	a_q_ravel = allocate_1d_array<Tad>(nic*njc*nq);
	a_rhs_ravel = allocate_1d_array<Tad>(nic*njc*nq);

#if defined(ENABLE_ARMA)
	linearsolver = new LinearSolverArma(mesh, config);
#endif

#if defined(ENABLE_EIGEN)
	linearsolver = new LinearSolverEigen(mesh, config);
#endif

#if defined(ENABLE_PETSC)
	linearsolver = new LinearSolverPetsc(mesh, config);
#endif


	logger_convergence = spdlog::basic_logger_mt("convergence", "history.dat", true);
	logger_convergence->info(" ");
	logger = spdlog::get("console");

	CFL = config->solver->cfl;
	UNDER_RELAXATION = config->solver->under_relaxation;
	label = config->io->label;


	equation = new EulerEquation<T, Tad>(mesh, config);
	iomanager = std::make_shared<IOManager>(mesh, config);
}
template <class T, class Tad>
Solver<T, Tad>::~Solver(){
	uint ni = mesh->ni;
	uint nj = mesh->nj;
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;

	release_1d_array(q, nic*njc*nq);
	release_1d_array(a_q_ravel, nic*njc*nq);
	release_1d_array(a_rhs_ravel, nic*njc*nq);
	release_1d_array(rhs, nic*njc*nq);
	release_1d_array(dt, nic*njc);
	delete linearsolver;
	delete equation;
}


template <class T, class Tad>
void Solver<T, Tad>::solve(){
#if defined(ENABLE_EIGEN)
	Eigen::SparseLU<Eigen::SparseMatrix<double>> eigen_solver;
	//Eigen::SuperLU<Eigen::SparseMatrix<double>> eigen_solver;
#endif
	
	uint ni = mesh->ni;
	uint nj = mesh->nj;
	uint nic = mesh->nic;
	uint njc = mesh->njc;
	uint nq = mesh->solution->nq;
	double l2norm[nq] = {1e10};

	uint counter = 0;
	double t = 0.0;
	initialize();
	copy_from_solution();
	logger->info("Welcome to structured!");
	while(1){
		timer_residual.reset();
		config->profiler->reset_time_residual();

		trace_on(1);
		for(uint i=0; i<nic; i++){
			for(uint j=0; j<njc; j++){
				for(uint k=0; k<nq; k++){
					a_q_ravel[i*njc*nq + j*nq + k] <<= q[i*njc*nq + j*nq + k];
				}
			}
		}
		equation->calc_residual(a_q_ravel, a_rhs_ravel);
		
		for(uint i=0; i<nic; i++){
			for(uint j=0; j<njc; j++){
				for(uint k=0; k<nq; k++){
					a_rhs_ravel[i*njc*nq + j*nq + k] >>= rhs[i*njc*nq + j*nq + k];
				}
			}
		}
		
		trace_off();
		config->profiler->update_time_residual();
		float dt_perfs = timer_residual.diff();
		logger->info("Residual time = {:03.2f}", dt_perfs);


		for(int j=0; j<nq; j++){
			l2norm[j] = 0.0;
			for(int i=0; i<nic*njc; i++){
				l2norm[j] += rhs[i*nq+j]*rhs[i*nq+j];
			}
			l2norm[j] = sqrt(l2norm[j]);
		}
		if(l2norm[0] < 1e-8){
			logger->info("Convergence reached!");
			copy_to_solution();
			iomanager->write(counter);
			float dt_main = timer_main.diff();
			logger->info("Final:: Step: {:08d} Time: {:.2e} Wall Time: {:.2e} CFL: {:.2e} Density Norm: {:.2e}", counter, t, dt_main, CFL, l2norm[0]);
			break;
		}

		config->profiler->reset_time_jacobian();
		sparse_jac(1,nic*njc*nq,nic*njc*nq,repeat,q,&nnz,&rind,&cind,&values,options);
		config->profiler->update_time_jacobian();
		logger->debug("NNZ = {}", nnz);

		if(counter == 0)
			linearsolver->preallocate(nnz);
		calc_dt();
		double dt_local = 0;
		for(uint i=0; i<nnz; i++){
			dt_local = dt[rind[i]/nq];
			values[i] = -values[i];
			//			std::cout<<dt_local<<std::endl;
			if(rind[i] == cind[i]){values[i] += 1.0/dt_local;}
		}
		timer_la.reset();
		config->profiler->reset_time_linearsolver();
		linearsolver->set_jac(nnz, rind, cind, values);
		linearsolver->set_rhs(rhs);
		linearsolver->solve_and_update(q, UNDER_RELAXATION);
		config->profiler->update_time_linearsolver();
	//q[i][j][k] = q[i][j][k] + rhs[i][j][k]*dt;
		
		float dt_perf = timer_la.diff();
		logger->info("Linear algebra time = {:03.2f}", dt_perf);

		free(rind); rind=nullptr;
		free(cind); cind=nullptr;
		free(values); values=nullptr;
		

		/* for(uint i=0; i<nic; i++){ */
		/* 	for(uint j=0; j<njc; j++){ */
		/* 		for(uint k=0; k<nq; k++){ */
		/* 			//q[i][j][k] = q[i][j][k] + rhs[i][j][k]*dt; */
		/* 			q[i][j][k] = q[i][j][k] + arma_dq(i*njc*nq + j*nq + k, 0); */
		/* 		} */
		/* 	} */
		/* } */
		t += 0;
		counter += 1;

		auto cfl_ramp =  config->solver->cfl_ramp;
		if(cfl_ramp){
			auto cfl_ramp_iteration = config->solver->cfl_ramp_iteration;
			if(counter > cfl_ramp_iteration){
				auto cfl_ramp_exponent = config->solver->cfl_ramp_exponent;
				CFL = pow(CFL, cfl_ramp_exponent);
				CFL = std::min(CFL, 1e6);
			}
		}

		auto under_relaxation_ramp =  config->solver->under_relaxation_ramp;
		if(under_relaxation_ramp){
			auto under_relaxation_ramp_iteration = config->solver->under_relaxation_ramp_iteration;
			if(counter > under_relaxation_ramp_iteration){
				auto under_relaxation_ramp_exponent = config->solver->under_relaxation_ramp_exponent;
				UNDER_RELAXATION = pow(UNDER_RELAXATION, under_relaxation_ramp_exponent);
				UNDER_RELAXATION = std::min(UNDER_RELAXATION, 10.0);
			}
		}


		
		
		if(counter % config->io->stdout_frequency == 0){
			float dt_main = timer_main.diff();
			logger->info("Step: {:08d} Time: {:.2e} Wall Time: {:.2e} CFL: {:.2e} Density Norm: {:.2e}", counter, t, dt_main, CFL, l2norm[0]);
			logger_convergence->info("{:08d} {:.2e} {:.2e} {:.2e} {:.2e} {:.2e} {:.2e} {:.2e}", counter, t, dt_main, CFL, l2norm[0], l2norm[1], l2norm[2], l2norm[3]);
		}
		
		if(counter % config->io->fileout_frequency == 0){
			copy_to_solution();
			iomanager->write(counter);
		}
	}
}


#endif