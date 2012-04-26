// Created 24-Apr-2012 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "likely/BinnedData.h"
#include "likely/RuntimeError.h"
#include "likely/AbsBinning.h"
#include "likely/CovarianceMatrix.h"

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"

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
    _ndata = 0;
    _nbins = 1;
    BOOST_FOREACH(AbsBinningCPtr binning, _axisBinning) {
        _nbins *= binning->getNBins();
    }
    _offset.resize(_nbins,EMPTY_BIN);
}

local::BinnedData::~BinnedData() { }

int local::BinnedData::getIndex(std::vector<int> const &binIndices) const {
    int index(0), nAxes(getNAxes());
    if(binIndices.size() != nAxes) {
        throw RuntimeError("BinnedData::getIndex: invalid input vector size.");
    }
    for(int axis = nAxes-1; axis >= 0; --axis) {
        int binIndex(binIndices[axis]), nBins(_axisBinning[axis]->getNBins());
        if(binIndex < 0 || binIndex >= nBins) {
            throw RuntimeError("BinnedData::getIndex: invalid bin index.");
        }
        index = binIndices[axis] + index*nBins;
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

void local::BinnedData::_checkIndex(int index) const {
    if(index < 0 || index >= _nbins) {
        throw RuntimeError("BinnedData: invalid index " +
            boost::lexical_cast<std::string>(index));
    }
}

void local::BinnedData::getBinIndices(int index, std::vector<int> &binIndices) const {
    _checkIndex(index);
    binIndices.resize(0);
    binIndices.reserve(getNAxes());
    int partial(index);
    BOOST_FOREACH(AbsBinningCPtr binning, _axisBinning) {
        int nBins(binning->getNBins()), binIndex(partial % nBins);
        binIndices.push_back(binIndex);
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

double local::BinnedData::getData(int index) const {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::getData: bin is empty.");
    }
    return _data[_offset[index]];
}

void local::BinnedData::setData(int index, double value) {
    if(hasData(index)) {
        _data[_offset[index]] = value;
    }
    else {
        if(hasCovariance()) {
            throw RuntimeError("BinnedData::setData: cannot add data after covariance.");
        }
        _offset[index] = _index.size();
        _index.push_back(index);
        _data.push_back(value);
    }
}

void local::BinnedData::addData(int index, double offset) {
    if(!hasData(index)) {
        throw RuntimeError("BinnedData::addData: bin is empty.");        
    }
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
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(_ndata));
    }
    _covariance->setCovariance(_offset[index1],_offset[index2],value);
}

void local::BinnedData::setInverseCovariance(int index1, int index2, double value) {
    if(!hasData(index1) || !hasData(index2)) {
        throw RuntimeError("BinnedData::setInverseCovariance: bin is empty.");
    }
    if(!hasCovariance()) {
        // Create a new covariance matrix sized to the number of bins with data.
        _covariance.reset(new CovarianceMatrix(_ndata));
    }
    _covariance->setInverseCovariance(_offset[index1],_offset[index2],value);
}

bool local::BinnedData::compress() const {
    return _covariance.get() ? _covariance->compress() : false;
}

bool local::BinnedData::isCompressed() const {
    return _covariance.get() ? _covariance->isCompressed() : false;
}

std::size_t local::BinnedData::getMemoryUsage() const {
    std::size_t size = sizeof(*this) +
        sizeof(int)*(_offset.capacity() + _index.capacity()) +
        sizeof(double)*_data.capacity();
    if(_covariance.get()) size += _covariance->getMemoryUsage();
    return size;
}
