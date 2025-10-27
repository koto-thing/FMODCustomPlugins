# include "ConvolutionProcessor.h"

# include <cstring>

/**
 * @brief コンボリューションプロセッサークラスの実装
 */
ConvolutionProcessor::ConvolutionProcessor()
    : m_geneticAlgorithm(50, 0.001f, 44100.0f) {}

/**
 * @brief デストラクタ
 */
ConvolutionProcessor::~ConvolutionProcessor() = default;

/**
 * @brief コンボリューションプロセッサーの準備を行う
 * @param sampleRate サンプリングレート
 * @param maxBlockSize 最大ブロックサイズ
 */
void ConvolutionProcessor::prepare(double sampleRate, unsigned int maxBlockSize) {
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;
    m_geneticAlgorithm = GeneticAlgorithm(50, 0.001f, static_cast<float>(sampleRate));

    // 既存のコンボリューターをクリア
    {
        std::unique_lock<std::shared_mutex> lock(m_convolverMutex);
        m_convolverL.reset();
        m_convolverR.reset();
        m_isIRReady.store(false, std::memory_order_relaxed);
    }

    // 既にワーカーが動作中ならそのまま継続。動いていなければ起動。
    if (!m_gaThread.joinable()) {
        m_gaThread = std::thread([this]() {
            // 挿入直後のスパイクを避けて少し遅延
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            generateAndLoadIR_Async();
        });
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
    if (m_gaThread.joinable())
        m_gaThread.join();
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

/**
 * @brief 非同期にIRを生成してロードする
 */
void ConvolutionProcessor::generateAndLoadIR_Async() {
    ReverbTargetParams params;
    params.t60 = 0.3914f;
    params.c80 = 12.3611f;
    int numGenerations = 250;

    // 遺伝的アルゴリズムで最適なIRを計算
    std::vector<float> bestIR = m_geneticAlgorithm.compute(params, numGenerations);
    if (bestIR.empty()) return;

    // 計算したIRをコンボリューターに設定
    setIR(bestIR.data(), bestIR.size());
}
