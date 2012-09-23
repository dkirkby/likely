// Created 24-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/BinnedData.h"
#include "likely/RuntimeError.h"
#include "likely/AbsBinning.h"
#include "likely/CovarianceMatrix.h"

#include "boost/foreach.hpp"
#include "boost/format.hpp"
#include "boost/lexical_cast.hpp"

#include <iostream>

namespace local = likely;

local::BinnedData::BinnedData(std::vector<AbsBinningCPtr> axes)
: _axisBinning(axes)
{
    if(0 == axes.size()) {
        throw RuntimeError("BinnedData: no axes provided.");
    }
    _initialize();
}

local::BinnedData::BinnedData(AbsBinningCPtr axis1)
{
    if(!axis1) {
        throw RuntimeError("BinnedData: missing axis1.");
    }
    _axisBinning.push_back(axis1);
    _initialize();
}

local::BinnedData::BinnedData(AbsBinningCPtr axis1, AbsBinningCPtr axis2)
{
    if(!axis1 || !axis2) {
        throw RuntimeError("BinnedData: missing axis data.");
    }
    _axisBinning.push_back(axis1);    
    _axisBinning.push_back(axis2);
    _initialize();
}

local::BinnedData::BinnedData(AbsBinningCPtr axis1, AbsBinningCPtr axis2, AbsBinningCPtr axis3)
{
    if(!axis1 || !axis2 || !axis3) {
        throw RuntimeError("BinnedData: missing axis data.");
    }
    _axisBinning.push_back(axis1);
    _axisBinning.push_back(axis2);
    _axisBinning.push_back(axis3);
    _initialize();
}

void local::BinnedData::_initialize() {
    _nbins = 1;
    BOOST_FOREACH(AbsBinningCPtr binning, _axisBinning) {
        _nbins *= binning->getNBins();
    }
    _offset.resize(_nbins,EMPTY_BIN);
    _weight = 1;
    _weighted = false;
    _finalized = false;
}

local::BinnedData::~BinnedData() { }

local::BinnedData *local::BinnedData::clone(bool binningOnly) const {
    return binningOnly ? new BinnedData(getAxisBinning()) : new BinnedData(*this);
}

// The pass-by-value semantics used here are not a mistake: see
// http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
local::BinnedData& local::BinnedData::operator=(BinnedData other) {
    swap(*this,other);
    return *this;
}

void local::swap(BinnedData& a, BinnedData& b) {
    // Enable argument-dependent lookup (ADL)
    using std::swap;
    swap(a._nbins,b._nbins);
    swap(a._axisBinning,b._axisBinning);
    swap(a._offset,b._offset);
    swap(a._index,b._index);
    swap(a._data,b._data);
    swap(a._covariance,b._covariance);
    swap(a._weight,b._weight);
    swap(a._weighted,b._weighted);
    swap(a._finalized,b._finalized);
}

void local::BinnedData::cloneCovariance() {
    if(hasCovariance()) {
        // Release our reference to the original covariance matrix and reset our
        // smart pointer to a copy of the original covariance matrix.
        _covariance.reset(new CovarianceMatrix(*_covariance));
    }
}

void local::BinnedData::dropCovariance(double weight) {
    if(hasCovariance() && isFinalized()) {
        throw RuntimeError("BinnedData::dropCovariance: object is finalized.");
    }
    setWeighted(false);
    _covariance.reset();
    _weight = weight;
}

local::BinnedData& local::BinnedData::add(BinnedData const& other, double weight) {
    // All done if the requested weight is zero.
    if(0 == weight) return *this;
    // Do we have any data yet?
    if(0 == getNBinsWithData()) {
        // If we are empty, then we only require that the other dataset have the same binning.
        if(!isCongruent(other,true)) {
            throw RuntimeError("BinnedData::add: datasets have different binning.");
        }
        // Initialize each occupied bin of the other dataset to zero contents in our dataset.
        for(IndexIterator iter = other.begin(); iter != other.end(); ++iter) {
            setData(*iter,0);
        }
        // If the other dataset has a covariance matrix, initialize ours now.
        if(other.hasCovariance()) {
            _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
        }
        else {
            // The scalar _weight plays the role of Cinv in the absence of any _covariance.
            // Set it to zero here since we will be adding the other data's weight below.
            _weight = 0;
        }
        // Our zero data vector should be interpreted as Cinv.data for the
        // purposes of adding to the other dataset, below. We don't call setWeighted here
        // because we don't actually want to transform the existing _data.
        _weighted = true;
    }
    else {
        // We already have data, so we will try to add the other data to ours.
        if(!isCongruent(other)) {
            throw RuntimeError("BinnedData::add: datasets are not congruent.");
        }
        // If we have a covariance, it must be modifiable.
        if(hasCovariance() && !isCovarianceModifiable()) {
            throw RuntimeError("BinnedData::add: cannot modify shared covariance.");
        }
    }
    // Add the weighted _data vectors, element by element, and save the result in our _data.
    setWeighted(true);
    other.setWeighted(true);
    for(int offset = 0; offset < _data.size(); ++offset) {
        _data[offset] += weight*other._data[offset];
    }
    if(hasCovariance()) {
        // Add Cinv matrices and save the result as our new Cinv matrix.
        _covariance->addInverse(*other._covariance,weight);        
    }
    else {
        _weight += other._weight*weight;
    }
    return *this;
}

void local::BinnedData::setWeighted(bool weighted) const {
    // Are we already in the desired state?
    if(weighted == _weighted) return;
    if(weighted) {
        if(hasCovariance() && getNBinsWithData() > 0) {
            // Change data to Cinv.data
            _covariance->multiplyByInverseCovariance(_data);
        }
        else if(_weight != 1) {
            // Scale data by _weight, which plays the role of Cinv.
            for(int offset = 0; offset < _data.size(); ++offset) _data[offset] *= _weight;
        }
    }
    else {
        if(hasCovariance() && getNBinsWithData() > 0) {
            // Change Cinv.data to data = C.Cinv.data
            _covariance->multiplyByCovariance(_data);
        }
        else if(_weight != 1) {
            // Scale data by 1/_weight, which plays the role of C.
            for(int offset = 0; offset < _data.size(); ++offset) _data[offset] /= _weight;
        }
    }
    // Record our new state.
    _weighted = weighted;
}

bool local::BinnedData::isCongruent(BinnedData const& other, bool onlyBinning) const {
    // Must have same number of axes.
    int nAxes(getNAxes());
    if(other.getNAxes() != nAxes) return false;
    // Binning must be represented by the same (not equivalent) object along each axis.
    for(int axis = 0; axis < nAxes; ++axis) {
        if(other._axisBinning[axis] != _axisBinning[axis]) return false;
    }
    if(!onlyBinning) {
        // Both must have or not have an associated covariance matrix.
        if(other.hasCovariance() && !hasCovariance()) return false;
        if(!other.hasCovariance() && hasCovariance()) return false;
        // List (not set) of bins with data must be the same.
        if(other.getNBinsWithData() != getNBinsWithData()) return false;
        for(int offset = 0; offset < _index.size(); ++offset) {
            if(other._index[offset] != _index[offset]) return false;
        }
    }
    return true;
}

int local::BinnedData::getIndex(std::vector<int> const &binIndices) const {
    int nAxes(getNAxes());
    if(binIndices.size() != nAxes) {
        throw RuntimeError("BinnedData::getIndex: invalid input vector size.");
    }
    int index(0);
    for(int axis = 0; axis < nAxes; ++axis) {
        int binIndex(binIndices[axis]), nBins(_axisBinning[axis]->getNBins());
        if(binIndex < 0 || binIndex >= nBins) {
            throw RuntimeError("BinnedData::getIndex: invalid bin index.");
        }
        index = binIndex + index*nBins;
    }
    return index;
}

int local::BinnedData::getIndex(std::vector<double> const &values) const {
    int index(0), nAxes(getNAxes());
    if(values.size() != nAxes) {
        throw RuntimeError("BinnedData::getIndex: invalid input vector size.");
    }
    std::vector<int> binIndices;
    for(int axis = 0; axis < nAxes; ++axis) {
        binIndices.push_back(_axisBinning[axis]->getBinIndex(values[axis]));
    }
    return getIndex(binIndices);
}

int local::BinnedData::getIndexAtOffset(int offset) const {
    if(offset < 0 || offset >= _index.size()) {
        throw RuntimeError("BinnedData::getIndexAtOffset: invalid offset.");
    }
    return _index[offset];
}

int local::BinnedData::getOffsetForIndex(int index) const {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::getOffsetForIndex: no data at index.");
    }
    return _offset[index];
}

void local::BinnedData::_checkIndex(int index) const {
    if(index < 0 || index >= _nbins) {
        throw RuntimeError("BinnedData: invalid index " +
            boost::lexical_cast<std::string>(index));
    }
}

void local::BinnedData::getBinIndices(int index, std::vector<int> &binIndices) const {
    _checkIndex(index);
    int nAxes(getNAxes());
    binIndices.resize(nAxes,0);
    int partial(index);
    for(int axis = nAxes-1; axis >= 0; --axis) {
        AbsBinningCPtr binning = _axisBinning[axis];
        int nBins(binning->getNBins()), binIndex(partial % nBins);
        binIndices[axis] = binIndex;
        partial = (partial - binIndex)/nBins;
    }
}

void local::BinnedData::getBinCenters(int index, std::vector<double> &binCenters) const {
    _checkIndex(index);
    binCenters.resize(0);
    binCenters.reserve(getNAxes());
    std::vector<int> binIndices;
    getBinIndices(index,binIndices);
    int nAxes(getNAxes());
    for(int axis = 0; axis < nAxes; ++axis) {
        AbsBinningCPtr binning = _axisBinning[axis];
        binCenters.push_back(binning->getBinCenter(binIndices[axis]));
    }
}

void local::BinnedData::getBinWidths(int index, std::vector<double> &binWidths) const {
    _checkIndex(index);
    binWidths.resize(0);
    binWidths.reserve(getNAxes());
    std::vector<int> binIndices;
    getBinIndices(index,binIndices);
    int nAxes(getNAxes());
    for(int axis = 0; axis < nAxes; ++axis) {
        AbsBinningCPtr binning = _axisBinning[axis];
        binWidths.push_back(binning->getBinWidth(binIndices[axis]));
    }
}

bool local::BinnedData::hasData(int index) const {
    _checkIndex(index);
    return !(_offset[index] == EMPTY_BIN);
}

double local::BinnedData::getData(int index, bool weighted) const {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::getData: bin is empty.");
    }
    setWeighted(weighted);
    return _data[_offset[index]];
}

void local::BinnedData::setData(int index, double value, bool weighted) {
    setWeighted(weighted);
    if(hasData(index)) {
        _data[_offset[index]] = value;
    }
    else {
        if(hasCovariance()) {
            throw RuntimeError("BinnedData::setData: cannot add data after covariance.");
        }
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setData: object is finalized.");
        }
        _offset[index] = _index.size();
        _index.push_back(index);
        _data.push_back(value);
    }
}

void local::BinnedData::addData(int index, double offset, bool weighted) {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::addData: bin is empty.");        
    }
    setWeighted(weighted);
    _data[_offset[index]] += offset;
}

double local::BinnedData::getCovariance(int index1, int index2) const {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::getCovariance: has no covariance specified.");
    }
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::getCovariance: bin is empty.");
    }
    return _covariance->getCovariance(_offset[index1],_offset[index2]);
}

double local::BinnedData::getInverseCovariance(int index1, int index2) const {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::getInverseCovariance: has no covariance specified.");
    }
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::getInverseCovariance: bin is empty.");
    }
    return _covariance->getInverseCovariance(_offset[index1],_offset[index2]);
}

void local::BinnedData::setCovariance(int index1, int index2, double value) {
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::setCovariance: bin is empty.");
    }
    if(!hasCovariance()) {
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setCovariance: object is finalized.");
        }
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
    }
    if(!isCovarianceModifiable()) {
        throw RuntimeError("BinnedData::setCovariance: cannot modify shared covariance.");
    }
    // Note that we do not call setWeighted here, so we are changing the meaning
    // of _data in a way that depends on the current value of _weighted.
    _covariance->setCovariance(_offset[index1],_offset[index2],value);
}

void local::BinnedData::setInverseCovariance(int index1, int index2, double value) {
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::setInverseCovariance: bin is empty.");
    }
    if(!hasCovariance()) {
        if(isFinalized()) {
            throw RuntimeError("BinnedData::setInverseCovariance: object is finalized.");
        }
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(getNBinsWithData()));
    }
    if(!isCovarianceModifiable()) {
        throw RuntimeError("BinnedData::setInverseCovariance: cannot modify shared covariance.");
    }
    // Note that we do not call setWeighted here, so we are changing the meaning
    // of _data in a way that depends on the current value of _weighted.
    _covariance->setInverseCovariance(_offset[index1],_offset[index2],value);
}

void local::BinnedData::transformCovariance(CovarianceMatrixPtr D) {
    if(!hasCovariance()) {
        throw RuntimeError("BinnedData::transformCovariance: no covariance to transform.");
    }
    // Make sure that our _data vector is independent of our _covariance before it changes.
    setWeighted(false);
    // Replace D with C.Dinv.C where C is our original covariance matrix.
    D->replaceWithTripleProduct(*_covariance);
    // Swap C with D
    swap(*D,*_covariance);
}

void local::BinnedData::setCovarianceMatrix(CovarianceMatrixPtr covariance) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::setCovarianceMatrix: object is finalized.");
    }
    if(covariance->getSize() != getNBinsWithData()) {
        throw RuntimeError("BinnedData::setCovarianceMatrix: new covariance has the wrong size.");
    }
    _covariance = covariance;
}

void local::BinnedData::shareCovarianceMatrix(BinnedData const &other) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::shareCovarianceMatrix: object is finalized.");
    }
    // Create a temporary (and empty) covariance matrix of the right size, if necessary,
    // so this will not prevent us being congruent with the other binned data.
    if(!hasCovariance()) _covariance = CovarianceMatrixPtr(new CovarianceMatrix(getNBinsWithData()));
    if(!isCongruent(other)) {
        throw RuntimeError("BinnedData::shareCovarianceMatrix: datasets are not congruent.");
    }
    _covariance = other._covariance;
}

bool local::BinnedData::compress(bool weighted) const {
    setWeighted(weighted);
    return _covariance.get() ? _covariance->compress() : false;
}

void local::BinnedData::finalize() {
    _finalized = true;
}

bool local::BinnedData::isCompressed() const {
    return _covariance.get() ? _covariance->isCompressed() : false;
}

std::size_t local::BinnedData::getMemoryUsage(bool includeCovariance) const {
    std::size_t size = sizeof(*this) +
        sizeof(int)*(_offset.capacity() + _index.capacity()) + sizeof(double)*_data.capacity();
    if(hasCovariance() && includeCovariance) size += _covariance->getMemoryUsage();
    return size;
}

void local::BinnedData::prune(std::set<int> const &keep) {
    if(isFinalized()) {
        throw RuntimeError("BinnedData::prune: object is finalized.");
    }
    // Create a parallel set of internal offsets for each global index, checking that
    // all indices are valid.
    std::set<int> offsets;
    BOOST_FOREACH(int index, keep) {
        _checkIndex(index);
        offsets.insert(_offset[index]);
    }
    // Are we actually removing anything?
    int newSize(offsets.size());
    if(newSize == getNBinsWithData()) return;
    // Reset our vector of offset for each index with data.
    _offset.assign(_nbins,EMPTY_BIN);
    // Shift our (unweighted) data vector elements down to compress out any elements
    // we are not keeping. We are using the fact that std::set guarantees that iteration
    // follows sort order, from smallest to largest key value.
    setWeighted(false);
    int newOffset(0);
    BOOST_FOREACH(int oldOffset, offsets) {
        // oldOffset >= newOffset so we will never clobber an element that we still need
        assert(oldOffset >= newOffset);
        int index = _index[oldOffset];
        _offset[index] = newOffset;
        _index[newOffset] = index;
        _data[newOffset] = _data[oldOffset];
        newOffset++;
    }
    _index.resize(newSize);
    _data.resize(newSize);
    // Prune our covariance matrix, if any.
    if(hasCovariance()) {
        if(!isCovarianceModifiable()) cloneCovariance();
        _covariance->prune(offsets);
    }
}

double local::BinnedData::chiSquare(std::vector<double> pred) const {
    if(pred.size() != getNBinsWithData()) {
        throw RuntimeError("BinnedData::chiSquare: prediction vector has wrong size.");
    }
    // Subtract our data vector from the prediction.
    IndexIterator nextIndex(begin());
    std::vector<double>::iterator nextPred(pred.begin());
    double residual, unweighted(0);
    while(nextIndex != end()) {
        residual = (*nextPred++ -= getData(*nextIndex++));
        unweighted += residual*residual;
    }
    // Our input vector now holds deltas. Our covariance does the rest of the work.
    return hasCovariance() ? _covariance->chiSquare(pred) : unweighted*_weight;
}

void local::BinnedData::getDecorrelatedWeights(std::vector<double> const &pred,
std::vector<double> &dweights) const {
    int nbins(getNBinsWithData());
    if(pred.size() != nbins) {
        throw RuntimeError("BinnedData::getDecorrelatedErrors: prediction vector has wrong size.");
    }
    dweights.reserve(nbins);
    dweights.resize(0);
    // Subtract the prediction from our data vector.
    std::vector<double> delta;
    delta.reserve(nbins);
    std::vector<double>::const_iterator nextPred(pred.begin());
    for(IndexIterator iter = begin(); iter != end(); ++iter) {
        delta.push_back(getData(*iter) - *nextPred++);
    }
    // Loop over bins
    for(int j = 0; j < nbins; ++j) {
        double dweight(0);
        if(hasCovariance()) {
            double deltaj(delta[j]);
            if(0 == deltaj) {
                dweight = _covariance->getInverseCovariance(j,j);
            }
            else {
                for(int k = 0; k < nbins; ++k) {
                    dweight += _covariance->getInverseCovariance(j,k)*delta[k]/deltaj;
                }
            }
        }
        else {
            dweight = _weight;
        }
        dweights.push_back(dweight);
    }
}

void local::BinnedData::printToStream(std::ostream &out, std::string format) const {
    boost::format indexFormat("[%4d] "),valueFormat(format);
    for(IndexIterator iter = begin(); iter != end(); ++iter) {
        int index(*iter);
        out << (indexFormat % index) << (valueFormat % getData(index)) << std::endl;
    }
}

local::BinnedDataPtr local::BinnedData::sample(RandomPtr random) const {
    // Create a new dataset with the same binning.
    bool binningOnly(true);
    BinnedDataPtr sampled(this->clone(binningOnly));
    // Fill the new dataset with noise sampled from our covariance.
    _covariance->sample(sampled->_data,random);
    // Copy our data vector book-keeping arrays to the sampled dataset.
    sampled->_offset = _offset;
    sampled->_index = _index;
    // Add our (unweighted) data vector to the sampled noise.
    setWeighted(false);
    for(int offset = 0; offset < _data.size(); ++offset) {
        sampled->_data[offset] += _data[offset];
    }
    // Copy our covariance matrix to the sampled data.
    sampled->setCovarianceMatrix(_covariance);
    return sampled;
}
