# pragma once

# include "GeneticAlgorithm.h"
# include "../ThirdParty/FFTConvolver/FFTConvolver.h"

# include <vector>
# include <atomic>
# include <mutex>
# include <memory>
# include <thread>
# include <shared_mutex>

class ConvolutionProcessor {
public:
    ConvolutionProcessor();
    ~ConvolutionProcessor();

    void prepare(double sampleRate, unsigned int maxBlockSize);
    void process(float* inBufferL, float* inBufferR, float* outBufferL, float* outBufferR, unsigned int numSamples);
    void release();
    void setIR(const float* ir, size_t length);

private:
    GeneticAlgorithm m_geneticAlgorithm;
    std::atomic<bool> m_isIRReady { false };
    std::thread m_gaThread;
    unsigned int m_maxBlockSize { 1024 };
    double m_sampleRate { 44100.0 };

    fftconvolver::FFTConvolver m_convolverL;
    fftconvolver::FFTConvolver m_convolverR;

    std::shared_mutex m_convolverMutex;
    void generateAndLoadIR_Async();
};
