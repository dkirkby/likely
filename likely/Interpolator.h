// Created 21-Jun-2011 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#ifndef LIKELY_INTERPOLATOR
#define LIKELY_INTERPOLATOR

#include "likely/types.h"

#include "boost/smart_ptr.hpp"

#include <vector>
#include <string>
#include <iosfwd>

namespace likely {
    // Implements interpolation algorithms.
	class Interpolator {
	public:
        typedef std::vector<double> CoordinateValues;
        Interpolator(CoordinateValues const &x, CoordinateValues const &y,
            std::string const &algorithm);
        virtual ~Interpolator();
        // Returns the interpolated y value for the specified x value. Returns the
        // appropriate endpoint y value if x is outside the interpolation domain.
        double operator()(double x) const;
	private:
        int _nValues;
        CoordinateValues _x, _y;
        class Implementation;
        boost::scoped_ptr<Implementation> _pimpl;
	}; // Interpolator
	
    // Returns a smart pointer to an interpolator based on control points read
    // from the specified file name.
	InterpolatorPtr createInterpolator(std::string const &filename,
        std::string const &algorithm);

	// Creates and returns a shared pointer to a generic function object that wraps a
	// shared pointer pimpl to an implementation function object of class P. The
	// returned shared pointer creates a new reference to the input shared pointer so that
	// the input object is guaranteed to stay alive as long as the returned object does.
	typedef boost::function<double (double)> GenericFunction;
    typedef boost::shared_ptr<GenericFunction> GenericFunctionPtr;
	template <class P> GenericFunctionPtr createFunctionPtr(boost::shared_ptr<P> pimpl);

    // Fills the vectors provided from the columns of the specified input stream.
    // Returns the number of rows successfully read or throws a RuntimeError.
    // Any input beyond the required column values is silently ignore if ignoreExtra
    // is set or, otherwise, generates a RuntimeError.
    int readVectors(std::istream &input, std::vector<std::vector<double> > &vectors,
        bool ignoreExtra = true);
	
} // likely

#endif // LIKELY_INTERPOLATOR
