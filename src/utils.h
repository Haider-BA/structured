#ifndef _UTILS_H
#define _UTILS_H

template <class T, class Ti>
T*** allocate_3d_array(Ti nx, Ti ny, Ti nz){
    T*** A = new T**[nx];
    for(Ti i(0); i < nx; ++i){
			A[i] = new T*[ny];
			for(Ti j(0); j < ny; ++j){
				A[i][j] = new T[nz];					
				for(Ti k(0); k < nz; ++k){
					A[i][j][k]= 0.;
				}
			}
	}
    return A;
}
template <class T, class Ti>
void release_3d_array(T*** A, Ti nx, Ti ny, Ti nz){
    for (Ti i = 0; i < nx; ++i){
			for (Ti j = 0; j < ny; ++j){
				delete[] A[i][j];
			}
			delete[] A[i];
	}
    delete[] A;
}

template <class T, class Ti>
T** allocate_2d_array(Ti nx, Ti ny){
    T** A = new T*[nx];
    for(Ti i(0); i < nx; ++i){
		A[i] = new T[ny];
	}
    return A;
}

template <class T, class Ti>
void release_2d_array(T** A, Ti nx, Ti ny){
    for (Ti i = 0; i < nx; ++i){
		delete[] A[i];
	}
    delete[] A;
}


template <class T, class Ti>
T* allocate_1d_array(Ti nx){
    T *A = new T[nx];
    return A;
}

template <class T, class Ti>
	void release_1d_array(T* A, Ti nx){
    delete[] A;
}


template<class T, class Ti>
void first_order_xi(Ti ni, Ti nj, T** q, T** ql, T** qr){
	Ti njm = nj-1;
	for(Ti i=0; i<ni; i++){
		for(Ti j=0; j<njm; j++){
			ql[i][j] = q[i][j+1];
			qr[i][j] = q[i+1][j+1];
			//std::cout<<i<<" "<<j<<" "<<ql[i][j]<<" "<<qr[i][j]<<std::endl;

		}
	}
}

template<class T, class Ti>
void first_order_eta(Ti ni, Ti nj, T** q, T** ql, T** qr){
	Ti nim = ni-1;
  	for(Ti i=0; i<nim; i++){
		for(Ti j=0; j<nj; j++){
			ql[i][j] = q[i+1][j];
			qr[i][j] = q[i+1][j+1];
		}
	}
}

template<class T>
void primvars(const T Q[4], T *rho, T *u, T *v, T *p){
  T tmp_rho = Q[0];
  T tmp_u = Q[1]/tmp_rho;
  T tmp_v = Q[2]/tmp_rho;
  *rho = Q[0];
  *u = Q[1]/Q[0];
  *v = Q[2]/Q[0];
  *p = (Q[3] - 0.5*tmp_rho*(tmp_u*tmp_u + tmp_v*tmp_v))*(GAMMA-1);
}


#endif