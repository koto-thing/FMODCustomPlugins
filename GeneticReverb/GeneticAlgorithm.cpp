# include "GeneticAlgorithm.h"
# include "AnalysisHelpers.h"

# include <random>

/**
 * @brief 遺伝的アルゴリズムクラスの実装
 */
GeneticAlgorithm::GeneticAlgorithm(int populationSize, float mutationRate, float sampleRate)
    : m_popSize(populationSize),
      m_mutationRate(mutationRate),
      m_sampleRate(sampleRate),
      m_rng(std::random_device{}())
{
    m_population.resize(m_popSize);
}

/**
 * @brief デストラクタ
 */
GeneticAlgorithm::~GeneticAlgorithm() = default;

/**
 * @brief 遺伝的アルゴリズムを実行して最適なインパルス応答を生成する関数
 * @param targetParams 目標とする残響特性のパラメータ
 * @param numGenerations 世代数
 * @return 最適化されたインパルス応答のベクトル
 */
std::vector<float> GeneticAlgorithm::compute(const ReverbTargetParams& targetParams, int numGenerations) {
    // 集団サイズが0以下の場合は空のベクトルを返す
    if (m_popSize <= 0) {
        std::cerr << "GA: Population size  <= 0" << std::endl;
        return { };
    }

    // T60が非常に小さい場合は警告を出して最小値にクランプする
    float safedT60 = targetParams.t60;
    if (safedT60 <= 0.0001f) {
        std::cerr << "GA: targetT60 too small, clamping to 0.001" << std::endl;
        safedT60 = 0.001f;
    }

    if (m_onProgress)
        m_onProgress(0, numGenerations, 1e10);

    // 初期集団をランダムに生成
    initializePopulation(targetParams.t60);

    for (int gen = 0 ; gen < numGenerations ; ++gen) {
        // 全個体の適応度を計算
        calculatePopulationFitness(targetParams);

        if (m_population.empty()) {
            std::cerr << "GA: Population empty before sort" << std::endl;
            return { };
        }

        // 適応順にソートする
        std::sort(m_population.begin(), m_population.end());

        if (m_population[0].ir.empty()) {
            std::cerr << "GA: Best individual's IR is empty at generation " << gen << std::endl;
            return { };
        }

        if (m_onProgress) {
            double best = m_population[0].fitness;
            m_onProgress(gen + 1, numGenerations, best);
        }

        if (m_population[0].fitness < 0.001)
            break;

        // キャンセルが要求された場合はループを抜ける
        if (m_cancel.load(std::memory_order_relaxed))
            break;

        // 次世代の個体群を生成
        m_population = createNextGeneration();
        if (m_population.empty()) {
            std::cerr << "GA: CreateNextGeneration produced empty population" << std::endl;
            return { };
        }
    }

    if (m_onProgress) {
        const double best = (m_population.empty() || m_population[0].ir.empty()) ? 1e10 : m_population[0].fitness;
        m_onProgress(numGenerations, numGenerations, best);
    }

    if (m_population.empty() || m_population[0].ir.empty()) {
        std::cerr << "GA: Returning empty IR" << std::endl;
        return { };
    }

    return m_population[0].ir;
}

/**
 * @brief 進捗コールバック関数の設定
 * @param callback コールバック関数
 */
void GeneticAlgorithm::setProgressCallback(std::function<void(int, int, double)> callback) {
    m_onProgress = std::move(callback);
}

/**
 * @brief 処理のキャンセルを要求する関数
 */
void GeneticAlgorithm::cancel()
{
    m_cancel.store(true, std::memory_order_relaxed);
}

/**
 * @brief キャンセルフラグをリセットする関数
 */
void GeneticAlgorithm::resetCancel()
{
    m_cancel.store(false, std::memory_order_relaxed);
}

/**
 * @brief 初期集団をランダムに生成する関数
 * @param targetT60 目標とするT60値
 */
void GeneticAlgorithm::initializePopulation(float targetT60) {
    if (m_population.empty()) {
        std::cerr << "GA: Population size is empty" << std::endl;
        return;
    }

    if (targetT60 <= 0.0001f)
        targetT60 = 0.001f;

    // IRの長さ
    auto irLength = static_cast<size_t>(static_cast<double>(targetT60) * 1.5 * static_cast<double>(m_sampleRate));
    if (irLength < 1024)
        irLength = 1024;

    // 各個体のIRをランダムに生成
    for (auto& individual : m_population) {
        individual.ir.clear();
        individual.ir.resize(irLength);

        // ランダムなインパルス応答を生成
        for (size_t i = 0 ; i < irLength ; ++i) {
            float t = static_cast<float>(i) / m_sampleRate; // 時間
            float randomNoise = m_distNeg1to1(m_rng);    // ランダムノイズ

            // 指定されたT60に基づく指数関数的減衰
            float decay = 1.0f;
            if (targetT60 > 1e-6f)
                decay = std::pow(10.0f, (-3.0f * t) / targetT60);
            else
                decay = 1.0f;

            individual.ir[i] = randomNoise * decay;
        }

        individual.fitness = 1e10; // 初期適応度を高く設定
    }
}

/**
 * @brief 個体群の適応度を計算する関数
 * @param targetParams 目標とする残響特性のパラメータ
 */
void GeneticAlgorithm::calculatePopulationFitness(const ReverbTargetParams& targetParams) {
    if (m_population.empty())
        return;

    // 各個体の適応度を計算
    for (auto& individual : m_population) {
        if (individual.ir.empty()) {
            individual.fitness = 1e10;
            continue;
        }

        // シュレーダーの残響曲線を計算
        auto edc = calculateSchroederDecay(individual.ir);
        if (edc.empty())
            continue;

        // T60とC80を計算
        float t60 = calculateT60(edc, m_sampleRate);
        float c80 = calculateC80(individual.ir, m_sampleRate);

        // 目標パラメータとの差を計算
        double errorT60 = std::abs(t60 - targetParams.t60);
        double errorC80 = std::abs(c80 - targetParams.c80);

        // 適応度を計算（T60の誤差を重視）
        individual.fitness = (errorT60 * 100.0) + (errorC80 * 1.0);
    }
}

/**
 * @brief 次世代の個体群を生成する関数
 * @return 新しい個体群のベクトル
 */
std::vector<Individual> GeneticAlgorithm::createNextGeneration() {
    std::vector<Individual> newPopulation;
    if (m_popSize <= 0)
        return newPopulation;

    newPopulation.resize(m_popSize);

    // エリート選択: 上位20%をそのまま次世代にコピー
    int eliteCount = m_popSize * 20 / 100;
    if (eliteCount < 1) eliteCount = 1;
    if (eliteCount > m_popSize) eliteCount = m_popSize;

    for (int i = 0 ; i < eliteCount ; ++i) {
        newPopulation[i] = m_population[i];
    }

    // 交叉と突然変異で残りの個体を生成
    std::uniform_int_distribution<int> distElite(0, eliteCount - 1);
    for (int i = eliteCount ; i < m_popSize ; ++i) {
        const Individual& parent1 = m_population[distElite(m_rng)];
        const Individual& parent2 = m_population[distElite(m_rng)];

        // 交叉操作
        Individual child = crossover(parent1, parent2);

        // 突然変異操作
        mutate(child);

        // 新しい個体を次世代に追加
        newPopulation[i] = child;
    }

    return newPopulation;
}

/**
 * @brief 交叉操作を行う関数
 * @param parent1 親個体1
 * @param parent2 親個体2
 * @return 生成された子個体
 */
Individual GeneticAlgorithm::crossover(const Individual& parent1, const Individual& parent2) {
    // 単純な一様交叉
    Individual child;
    size_t len1 = parent1.ir.size();
    size_t len2 = parent2.ir.size();
    size_t irLength = std::max(len1, len2);

    if (irLength == 0) {
        child.fitness = 1e10;
        return child;
    }

    child.ir.resize(irLength);

    // ランダムに親の遺伝子を選択して子に割り当てる
    for (size_t i = 0; i < irLength; ++i) {
        if (bool pickFirst = (m_dist0To1(m_rng) < 0.5f)) {
            child.ir[i] = (i < len1) ? parent1.ir[i] : ((i < len2) ? parent2.ir[i] : 0.0f);
        } else {
            child.ir[i] = (i < len2) ? parent2.ir[i] : ((i < len1) ? parent1.ir[i] : 0.0f);
        }
    }

    // 初期適応度を高く設定
    child.fitness = 1e10;
    return child;
}

/**
 * @brief 突然変異操作を行う関数
 * @param ind 突然変異を適用する個体
 */
void GeneticAlgorithm::mutate(Individual& ind) {
    if (ind.ir.empty())
        return;

    for (float & i : ind.ir) {
        if (m_dist0To1(m_rng) < m_mutationRate){
            float randomNoise = (m_distNeg1to1(m_rng)) * 0.1f;
            i += randomNoise;
        }
    }
}