# pragma once

# include "GeneticAlgorithm.h"
# include "../ThirdParty/FFTConvolver/FFTConvolver.h"

# include <vector>
# include <atomic>
# include <mutex>
# include <memory>
# include <optional>
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

    void setTargetParams(const ReverbTargetParams& params);
    void startGenerate();

    // 進捗コールバック関数の設定
    bool isGenerating() const;
    float progress() const;
    void cancelIR();

private:
    std::optional<GeneticAlgorithm> m_geneticAlgorithm;
    ReverbTargetParams m_params{ };
    std::atomic<bool> m_isIRReady { false };
    std::thread m_gaThread;
    unsigned int m_maxBlockSize { 1024 };
    double m_sampleRate { 44100.0 };

    // 進捗管理
    std::atomic<bool> m_isGenerating { false };
    std::atomic<float> m_progress { 0.0f };

    fftconvolver::FFTConvolver m_convolverL;
    fftconvolver::FFTConvolver m_convolverR;

    std::shared_mutex m_convolverMutex;
    void generateAndLoadIR_Async();
};
