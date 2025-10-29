# include "ConvolutionProcessor.h"

# include <cstring>

/**
 * @brief コンボリューションプロセッサークラスの実装
 */
ConvolutionProcessor::ConvolutionProcessor() {
    m_geneticAlgorithm.emplace(50, 0.001f, 44100.0f);
}

/**
 * @brief デストラクタ
 */
ConvolutionProcessor::~ConvolutionProcessor() {
    if (m_isGenerating.load(std::memory_order_acquire)) {
        if (m_geneticAlgorithm)
            m_geneticAlgorithm->cancel();
    }

    if (m_gaThread.joinable())
        m_gaThread.join();
}

/**
 * @brief コンボリューションプロセッサーの準備を行う
 * @param sampleRate サンプリングレート
 * @param maxBlockSize 最大ブロックサイズ
 */
void ConvolutionProcessor::prepare(double sampleRate, unsigned int maxBlockSize) {
    if (m_isGenerating.load(std::memory_order_acquire)) {
        if (m_geneticAlgorithm)
            m_geneticAlgorithm->cancel();
    }

    if (m_gaThread.joinable())
        m_gaThread.join();

    m_isGenerating.store(false, std::memory_order_release);
    m_progress.store(0.0f, std::memory_order_release);

    m_geneticAlgorithm.emplace(50, 0.001f, static_cast<float>(sampleRate));
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;

    // 既存のコンボリューターをクリア
    {
        std::unique_lock<std::shared_mutex> lock(m_convolverMutex);
        m_convolverL.reset();
        m_convolverR.reset();
        m_isIRReady.store(false, std::memory_order_relaxed);
    }
}

/**
 * @brief オーディオ処理を行う
 * @param inBufferL 左チャンネルの入力バッファ
 * @param inBufferR 右チャンネルの入力バッファ
 * @param outBufferL 左チャンネルの出力バッファ
 * @param outBufferR 右チャンネルの出力バッファ
 * @param numSamples 処理するサンプル数
 */
void ConvolutionProcessor::process(float* inBufferL, float* inBufferR, float* outBufferL, float* outBufferR, unsigned int numSamples) {
    // IRが準備できていない場合はゼロ出力
    if (!m_isIRReady.load(std::memory_order_acquire)) {
        std::memset(outBufferL, 0, numSamples * sizeof(float));
        std::memset(outBufferR, 0, numSamples * sizeof(float));
        return;
    }

    // コンボリューション処理
    std::shared_lock<std::shared_mutex> lock(m_convolverMutex);
    m_convolverL.process(inBufferL, outBufferL, numSamples);
    m_convolverR.process(inBufferR, outBufferR, numSamples);
}

/**
 * @brief コンボリューションプロセッサーのリリースを行う
 */
void ConvolutionProcessor::release() {
    if (m_isGenerating.load(std::memory_order_acquire)) {
        if (m_geneticAlgorithm)
            m_geneticAlgorithm->cancel();
    }

    if (m_gaThread.joinable())
        m_gaThread.join();

    m_isGenerating.store(false, std::memory_order_release);
    m_progress.store(0.0f, std::memory_order_release);

    std::unique_lock<std::shared_mutex> lock(m_convolverMutex);
    m_convolverL.reset();
    m_convolverR.reset();
    m_isIRReady.store(false, std::memory_order_relaxed);
}

/**
 * @brief インパルス応答を設定する
 * @param ir インパルス応答データ
 * @param length インパルス応答の長さ
 */
void ConvolutionProcessor::setIR(const float* ir, size_t length) {
    if (!ir || length == 0)
        return;

    // コンボリューターを初期化
    std::unique_lock<std::shared_mutex> lock(m_convolverMutex);
    bool okL = m_convolverL.init(m_maxBlockSize, ir, length);
    bool okR = m_convolverR.init(m_maxBlockSize, ir, length);

    // IR準備完了フラグを設定
    m_isIRReady.store(okL && okR, std::memory_order_release);
}

void ConvolutionProcessor::setTargetParams(const ReverbTargetParams& params) {
    m_params = params;
}

void ConvolutionProcessor::startGenerate() {
    if (m_isGenerating.load(std::memory_order_acquire))
        return;

    if (!m_geneticAlgorithm)
        return;

    if (m_gaThread.joinable())
        m_gaThread.join();

    m_isGenerating.store(true, std::memory_order_release);
    m_progress.store(0.0f, std::memory_order_release);

    m_gaThread = std::thread([this]() {
        // GAインスタンスを取得
        auto* ga = m_geneticAlgorithm ? &*m_geneticAlgorithm : nullptr;
        if (!ga) {
            m_isGenerating.store(false, std::memory_order_release);
            return;
        }

        // キャンセルフラグをリセット
        ga->resetCancel();

        // 進捗コールバックを設定
        ga->setProgressCallback([this](int cur, int total, double) {
            const float p = (total > 0) ? (static_cast<float>(cur) / static_cast<float>(total)) : 0.0f;
            m_progress.store(p, std::memory_order_release);
        });

        // 遺伝的アルゴリズムで最適なIRを計算
        const int numGenerations = 250;
        auto bestIR = ga->compute(m_params, numGenerations);

        // 最終更新（成功時は 1.0、キャンセル/失敗時は据え置き）
        if (!bestIR.empty()) {
            setIR(bestIR.data(), bestIR.size());
            m_progress.store(1.0f, std::memory_order_release);
        }

        // 進捗コールバックをクリア
        ga->setProgressCallback(nullptr);
        m_isGenerating.store(false, std::memory_order_release);
    });
}

bool ConvolutionProcessor::isGenerating() const {
    return m_isGenerating.load(std::memory_order_acquire);
}

float ConvolutionProcessor::progress() const {
    return m_progress.load(std::memory_order_acquire);
}

void ConvolutionProcessor::cancelIR() {
    if (!m_isGenerating.load(std::memory_order_acquire))
        return;

    if (m_geneticAlgorithm)
        m_geneticAlgorithm->cancel();

    if (m_gaThread.joinable())
        m_gaThread.join();

    m_isGenerating.store(false, std::memory_order_release);
    m_progress.store(0.0f, std::memory_order_release);
}

/**
 * @brief 非同期にIRを生成してロードする
 */
void ConvolutionProcessor::generateAndLoadIR_Async() {
    ReverbTargetParams params;
    params.t60 = 0.3914f;
    params.c80 = 12.3611f;
    int numGenerations = 250;

    // 遺伝的アルゴリズムで最適なIRを計算
    std::vector<float> bestIR = m_geneticAlgorithm->compute(params, numGenerations);
    if (bestIR.empty()) return;

    // 計算したIRをコンボリューターに設定
    setIR(bestIR.data(), bestIR.size());
}
