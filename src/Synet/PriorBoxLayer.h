/*
* Synet Framework (http://github.com/ermig1979/Synet).
*
* Copyright (c) 2018-2018 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "Synet/Common.h"
#include "Synet/Layer.h"

namespace Synet
{
    template <class T> class PriorBoxLayer : public Synet::Layer<T>
    {
    public:
        typedef T Type;
        typedef Layer<T> Base;
        typedef typename Base::TensorPtrs TensorPtrs;

        PriorBoxLayer(const LayerParam & param)
            : Base(param)
        {
        }

        virtual void Setup(const TensorPtrs & src, const TensorPtrs & buf, const TensorPtrs & dst)
        {
            const PriorBoxParam & param = this->Param().priorBox();
            _minSizes = param.minSize();
            _flip = param.flip();
            _aspectRatios.clear();
            _aspectRatios.push_back(1.0f);
            for (int i = 0; i < param.aspectRatio().size(); ++i) 
            {
                float aspectRatio = param.aspectRatio()[i];
                bool alreadyExist = false;
                for (int j = 0; j < _aspectRatios.size(); ++j)
                {
                    if (::fabs(aspectRatio - _aspectRatios[j]) < 1e-6)
                    {
                        alreadyExist = true;
                        break;
                    }
                }
                if (!alreadyExist) 
                {
                    _aspectRatios.push_back(aspectRatio);
                    if (_flip)
                        _aspectRatios.push_back(1.0f / aspectRatio);
                }
            }
            _numPriors = _aspectRatios.size() * _minSizes.size();
            if (param.maxSize().size() > 0) 
            {
                assert(param.minSize().size() == param.maxSize().size());
                _maxSizes = param.maxSize();
                _numPriors += _maxSizes.size();
            }
            _clip = param.clip();
            if (param.variance().size() > 1)
            {
                assert(param.variance().size() == 4);
                _variance = param.variance();
            }
            else if (param.variance().size() == 1)
                _variance.push_back(param.variance()[0]);
            else 
                _variance.push_back(0.1f);
            if (param.imgSize().size() == 2) 
            {
                _imgH = param.imgSize()[0];
                _imgW = param.imgSize()[1];
            }
            else if (param.imgSize().size() == 1) 
            {
                _imgH = param.imgSize()[0];
                _imgW = param.imgSize()[0];
            }
            else 
            {
                _imgH = 0;
                _imgW = 0;
            }
            if (param.step().size() == 2)
            {
                _stepH = param.step()[0];
                _stepW = param.step()[1];
            }
            else if (param.step().size() == 1)
            {
                _stepH = param.step()[0];
                _stepW = param.step()[0];
            }
            else
            {
                _stepH = 0;
                _stepW = 0;
            }
            _offset = param.offset();
        }

        virtual void Reshape(const TensorPtrs & src, const TensorPtrs & buf, const TensorPtrs & dst)
        {
            size_t layerW = src[0]->Axis(3);
            size_t layerH = src[0]->Axis(2);
            Shape shape(3);
            shape[0] = 1;
            shape[1] = 2;
            shape[2] = layerW * layerH * _numPriors * 4;
            dst[0]->Reshape(shape);
        }

    protected:
        virtual void ForwardCpu(const TensorPtrs & src, const TensorPtrs & buf, const TensorPtrs & dst)
        {
            SYNET_PERF_FUNC();

            size_t layerW = src[0]->Axis(3);
            size_t layerH = src[0]->Axis(2);
            size_t imgW, imgH;
            if (_imgH == 0 || _imgW == 0)
            {
                imgW = src[1]->Axis(3);
                imgH = src[1]->Axis(2);
            }
            else 
            {
                imgW = _imgW;
                imgH = _imgH;
            }
            float stepW, stepH;
            if (_stepW == 0 || _stepH == 0) 
            {
                stepW = float(imgW) / layerW;
                stepH = float(imgH) / layerH;
            }
            else 
            {
                stepW = _stepW;
                stepH = _stepH;
            }
            Type * pDst = dst[0]->CpuData();
            size_t dim = layerH * layerW * _numPriors * 4;
            size_t index = 0;
            for (size_t h = 0; h < layerH; ++h) 
            {
                for (size_t w = 0; w < layerW; ++w) 
                {
                    float centerX = (w + _offset) * stepW;
                    float centerY = (h + _offset) * stepH;
                    float boxW, boxH;
                    for (size_t s = 0; s < _minSizes.size(); ++s) 
                    {
                        int minS = (int)_minSizes[s];
                        boxW = boxH = (float)minS;
                        pDst[index++] = (centerX - boxW / 2.0f) / imgW;
                        pDst[index++] = (centerY - boxH / 2.0f) / imgH;
                        pDst[index++] = (centerX + boxW / 2.0f) / imgW;
                        pDst[index++] = (centerY + boxH / 2.0f) / imgH;
                        if (_maxSizes.size() > 0) 
                        {
                            int maxS = (int)_maxSizes[s];
                            boxW = boxH = (float)::sqrt(minS * maxS);
                            pDst[index++] = (centerX - boxW / 2.0f) / imgW;
                            pDst[index++] = (centerY - boxH / 2.0f) / imgH;
                            pDst[index++] = (centerX + boxW / 2.0f) / imgW;
                            pDst[index++] = (centerY + boxH / 2.0f) / imgH;
                        }
                        for (size_t r = 0; r < _aspectRatios.size(); ++r)
                        {
                            float ar = _aspectRatios[r];
                            if (::fabs(ar - 1.) < 1e-6) 
                                continue;
                            boxW = minS * sqrt(ar);
                            boxH = minS / sqrt(ar);
                            pDst[index++] = (centerX - boxW / 2.0f) / imgW;
                            pDst[index++] = (centerY - boxH / 2.0f) / imgH;
                            pDst[index++] = (centerX + boxW / 2.0f) / imgW;
                            pDst[index++] = (centerY + boxH / 2.0f) / imgH;
                        }
                    }
                }
            }
            if (_clip)
            {
                for (size_t d = 0; d < dim; ++d)
                    pDst[d] = std::min<Type>(std::max<Type>(pDst[d], Type(0)), Type(1));
            }
            pDst += dst[0]->Size(2);
            if (_variance.size() == 1)
                CpuSet(dim, Type(_variance[0]), pDst);
            else 
            {
                size_t offset = 0;
                for (size_t h = 0; h < layerH; ++h)
                    for (size_t w = 0; w < layerW; ++w)
                        for (size_t i = 0; i < _numPriors; ++i)
                            for (size_t j = 0; j < 4; ++j) 
                                pDst[offset++] = _variance[j];
            }
        }
    private:

        Floats _minSizes, _maxSizes, _aspectRatios, _variance;
        bool _flip, _clip;
        size_t _numPriors, _imgW, _imgH;
        float _stepW, _stepH, _offset;
    };
}