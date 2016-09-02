#ifndef _PETSC_H
#define _PETSC_H
#define CONFIG_PETSC_TOL 1e-12
#define CONFIG_PETSC_MAXITER 1000
template <class Tx>
class LinearSolverPetsc{
	std::shared_ptr<Mesh<Tx>> mesh;
	Vec dq, rhs;
	Mat jac;
	Tx *dq_array;
	PC pc;
	KSP ksp;

 public:
	LinearSolverPetsc(std::shared_ptr<Mesh<Tx>> val_mesh, std::shared_ptr<Config<Tx>> val_config){
		mesh = val_mesh;
		const auto nic = mesh->nic;
		const auto njc = mesh->njc;
		const auto nq = mesh->solution->nq;
		const auto ntrans = mesh->solution->ntrans;
		int nvar = nic*njc*(nq+ntrans);
		PetscInitialize(&val_config->argc, &val_config->argv, NULL, NULL);

		PetscErrorCode ierr;
		ierr = VecCreate(PETSC_COMM_WORLD, &dq);
		ierr = PetscObjectSetName((PetscObject) dq, "Solution");
		ierr = VecSetSizes(dq, PETSC_DECIDE, nvar);
		ierr = VecSetFromOptions(dq);
		ierr = VecDuplicate(dq,&rhs);

		MatCreateSeqAIJ(PETSC_COMM_WORLD, nvar, nvar, MAX_NNZ, NULL, &jac);
		
		ierr = KSPCreate(PETSC_COMM_WORLD, &ksp);
		ierr = KSPSetOperators(ksp, jac, jac);
		ierr = KSPGetPC(ksp, &pc);
		ierr = PCSetType(pc, PCLU);
		ierr = KSPSetType(ksp, KSPGMRES);
		ierr = KSPSetTolerances(ksp, 1e-7, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT);
		ierr = KSPSetFromOptions(ksp);
		MatSetBlockSize(jac, nq+ntrans);
	};

	~LinearSolverPetsc(){
		const auto nic = mesh->nic;
		const auto njc = mesh->njc;
		const auto nq = mesh->solution->nq;
		VecDestroy(&rhs);
		VecDestroy(&dq);
		MatDestroy(&jac);
		PetscFinalize();
	};

	void preallocate(int nnz){

	};
	
	void set_jac(int nnz, unsigned int *rind, unsigned int *cind, Tx *values){
		PetscScalar value_tmp;
		PetscErrorCode ierr;
		int row_idx, col_idx;
		for(uint i=0; i<nnz; i++){
			value_tmp = values[i];
			row_idx = rind[i];
			col_idx = cind[i];
			MatSetValue(jac, row_idx, col_idx, value_tmp ,INSERT_VALUES);
		}
		MatAssemblyBegin(jac, MAT_FINAL_ASSEMBLY);
		MatAssemblyEnd(jac, MAT_FINAL_ASSEMBLY);
	};

	void set_rhs(Tx *val_rhs){
		const auto nic = mesh->nic;
		const auto njc = mesh->njc;
		const auto nq = mesh->solution->nq;
		PetscScalar value_tmp;
		PetscErrorCode ierr;
		const auto ntrans = mesh->solution->ntrans;
		for(int i=0; i<nic*njc*(nq+ntrans); i++){
			value_tmp = val_rhs[i];
			VecSetValue(rhs, i, value_tmp, INSERT_VALUES);
		}
	};

	void solve_and_update(Tx *q, Tx under_relaxation){
		const auto nic = mesh->nic;
		const auto njc = mesh->njc;
		const auto nq = mesh->solution->nq;
		const auto ntrans = mesh->solution->ntrans;
		KSPSolve(ksp, rhs, dq);
		VecGetArray(dq, &dq_array);

		for(int i=0; i<nic*njc*(nq+ntrans); i++){
			q[i] = q[i] + dq_array[i]*under_relaxation;
		}
	};   
};

#endif
