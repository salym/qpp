/*
 * internal.h
 *
 *  Created on: Mar 24, 2014
 *      Author: vlad
 */

#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <vector>
#include <iostream>
#include "types.h"
#include "exception.h"

// internal functions, do not modify
namespace qpp
{
namespace internal
{

// Displays a standard container that supports STL iterators
template<typename T>
void _disp_container(const T& x)
{
	auto it = x.begin();
	for (; it != x.end() - 1; it++)
		std::cout << *it << " ";
	std::cout << *(it++);
}

// integer index to multi-index, use C-style array for speed
inline void _n2multiidx(size_t n, size_t numdims, const size_t *dims,
		size_t *result)
{
	size_t maxn = 1;
	for (size_t i = 0; i < numdims; i++)
		maxn *= dims[i];
	if (n > maxn - 1)
		throw Exception("_n2multiidx", Exception::Type::OUT_OF_RANGE);

	size_t _n = n;
	for (size_t i = 0; i < numdims; i++)
	{
		result[numdims - i - 1] = _n % static_cast<int>(dims[numdims - i - 1]);
		_n = _n / static_cast<int>(dims[numdims - i - 1]);
	}
}

// multi index to integer index, use C-style array for speed
inline size_t _multiidx2n(const size_t *midx, size_t numdims,
		const size_t *dims)
{
	for (size_t i = 0; i < numdims; i++)
		if (midx[i] >= dims[i])
			throw Exception("_multiidx2n", Exception::Type::OUT_OF_RANGE);

	size_t *part_prod = new size_t[numdims];

	part_prod[numdims - 1] = 1;
	for (size_t j = 1; j < numdims; j++)
		part_prod[numdims - j - 1] = part_prod[numdims - j] * dims[numdims - j];

	size_t result = 0;
	for (size_t i = 0; i < numdims; i++)
		result += midx[i] * part_prod[i];

	delete[] part_prod;

	return result;
}

// check square matrix
template<typename Scalar>
bool _check_square_mat(const types::DynMat<Scalar>& A)
{
	if (A.rows() != A.cols())
		return false;
	return true;
}

// check whether input is a vector or not
template<typename Scalar>
bool _check_vector(const types::DynMat<Scalar>& A)
{
	if (A.rows() != 1 && A.cols() != 1)
		return false;
	return true;
}

// check whether input is a vector or not
template<typename Scalar>
bool _check_nonzero_size(const types::DynMat<Scalar>& A)
{
	if (A.size() == 0)
		return false;
	return true;
}

// check that dims match the dimension of the matrix
template<typename Scalar>
bool _check_dims_match_mat(const std::vector<size_t>& dims,
		const types::DynMat<Scalar>& A)
{
	size_t proddim = 1;
	for (size_t i : dims)
		proddim *= i;
	if (proddim != static_cast<size_t>(A.rows()))
		return false;
	return true;
}

// check that dims is a valid dimension vector
inline bool _check_dims(const std::vector<size_t>& dims)
{
	if (std::find_if(dims.begin(), dims.end(), [&dims](size_t i) -> bool
	{	if(i==0) return true;
		else return false;}) != dims.end())
		return false;
	return true;
}

// check that all elements in dims equal to dim
inline bool _check_eq_dims(const std::vector<size_t> &dims, size_t dim)
{
	for (size_t i : dims)
		if (i != dim)
			return false;
	return true;
}

// check that subsys is valid with respect to dims
inline bool _check_subsys(const std::vector<size_t>& subsys,
		const std::vector<size_t>& dims)
{
	// sort the subsystems
	std::vector<size_t> subsyssort = subsys;
	std::sort(subsyssort.begin(), subsyssort.end());

	// check valid number of subsystems
	if (subsyssort.size() > dims.size())
		return false;

	// check duplicates
	if (std::unique(subsyssort.begin(), subsyssort.end()) != subsyssort.end())
		return false;

	// check range of subsystems
	if (std::find_if(subsyssort.begin(), subsyssort.end(),
			[&dims](size_t i) -> bool
			{	if(i>dims.size()-1) return true;
				else return false;}) != subsyssort.end())
		return false;

	return true;
}

// check that the permutation is valid with respect to dims
inline bool _check_perm(const std::vector<size_t>& perm,
		const std::vector<size_t>& dims)
{
	std::vector<size_t> sort_perm = perm;
	std::sort(sort_perm.begin(), sort_perm.end());
	for (size_t i = 0; i < dims.size(); i++)
		if (sort_perm[i] != i)
			return false;
	return true;
}

// used inside the #pragma omp parallel for in syspermute
template<typename Scalar>
inline void _syspermute_worker(const size_t *midxcol, size_t numdims,
		const size_t *cdims, const size_t *cperm, size_t i, size_t j,
		size_t &iperm, size_t &jperm, const types::DynMat<Scalar> &A,
		types::DynMat<Scalar> &result)
{
	size_t *midxrow = new size_t[numdims];
	size_t *midxrowtmp = new size_t[numdims];
	size_t *midxcoltmp = new size_t[numdims];
	size_t *permdims = new size_t[numdims];

	for (size_t k = 0; k < numdims; k++)
		permdims[k] = cdims[cperm[k]]; // permuted dimensions

	// compute the row multi index
	_n2multiidx(i, numdims, cdims, midxrow);

	for (size_t k = 0; k < numdims; k++)
	{
		midxrowtmp[k] = midxrow[cperm[k]]; // permuted multi-indexes
		midxcoltmp[k] = midxcol[cperm[k]]; // permuted multi-indexes
	}

	// move back to integer indexes
	iperm = _multiidx2n(midxrowtmp, numdims, permdims);
	jperm = _multiidx2n(midxcoltmp, numdims, permdims);
	result(iperm, jperm) = A(i, j);

	delete[] midxrow;
	delete[] midxrowtmp;
	delete[] midxcoltmp;
	delete[] permdims;
}

// used inside the #pragma omp parallel for in ptranspose
template<typename Scalar>
inline void _ptranspose_worker(const size_t *midxcol, size_t numdims,
		size_t numsubsys, const size_t *cdims, const size_t *csubsys, size_t i,
		size_t j, size_t &iperm, size_t &jperm, const types::DynMat<Scalar> &A,
		types::DynMat<Scalar> &result)
{
	size_t *midxcoltmp = new size_t[numdims];
	for (size_t k = 0; k < numdims; k++)
		midxcoltmp[k] = midxcol[k];
	size_t *midxrow = new size_t[numdims];

	// compute the row multi-index
	_n2multiidx(i, numdims, cdims, midxrow);

	for (size_t k = 0; k < numsubsys; k++)
		std::swap(midxcoltmp[csubsys[k]], midxrow[csubsys[k]]);

	// move back to integer indexes
	iperm = _multiidx2n(midxcoltmp, numdims, cdims);
	jperm = _multiidx2n(midxrow, numdims, cdims);
	result(iperm, jperm) = A(i, j);

	delete[] midxrow;
	delete[] midxcoltmp;
}

}
}

#endif /* INTERNAL_H_ */
