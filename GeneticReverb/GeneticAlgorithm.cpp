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
    // 初期集団の生成
    initializePopulation(targetParams.t60);

    for (int gen = 0 ; gen < numGenerations ; ++gen) {
        // 全個体の適応度を計算
        calculatePopulationFitness(targetParams);

        // 適応順にソートする
        std::sort(m_population.begin(), m_population.end());
        std::cout << "Generation " << gen << ": Best fitness = " << m_population[0].fitness << std::endl;

        if (m_population[0].fitness < 0.001)
            break;

        m_population = createNextGeneration();
    }

    return m_population[0].ir;
}

/**
 * @brief 初期集団をランダムに生成する関数
 * @param targetT60 目標とするT60値
 */
void GeneticAlgorithm::initializePopulation(float targetT60) {
    // IRの長さ
    size_t irLength = static_cast<size_t>(targetT60 * 1.5 * m_sampleRate);
    if (irLength < 1024)
        irLength = 1024;

    // 各個体のIRをランダムに生成
    for (auto& individual : m_population) {
        individual.ir.resize(irLength);

        // ランダムなインパルス応答を生成
        for (size_t i = 0 ; i < irLength ; ++i) {
            float t = (float)i / m_sampleRate;           // 時間
            float randomNoise = m_distNeg1to1(m_rng); // ランダムノイズ

            // 指定されたT60に基づく指数関数的減衰
            float decay = std::pow(10.0f, (-3.0f *t) / targetT60);
            individual.ir[i] = randomNoise * decay;
        }
    }
}

/**
 * @brief 個体群の適応度を計算する関数
 * @param targetParams 目標とする残響特性のパラメータ
 */
void GeneticAlgorithm::calculatePopulationFitness(const ReverbTargetParams& targetParams) {
    // 各個体の適応度を計算
    for (auto& individual : m_population) {
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
    std::vector<Individual> newPopulation(m_popSize);

    // エリート選択: 上位20%をそのまま次世代にコピー
    int eliteCount = m_popSize * 0.2;
    for (int i = 0 ; i < eliteCount ; ++i) {
        newPopulation[i] = m_population[i];
    }

    // 交叉と突然変異で残りの個体を生成
    for (int i = eliteCount ; i < m_popSize ; ++i) {
        std::uniform_int_distribution<int> distElite(0, eliteCount - 1);
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
    size_t irLength = parent1.ir.size();
    child.ir.resize(irLength);

    // ランダムに親の遺伝子を選択して子に割り当てる
    for (size_t i = 0 ; i < irLength ; ++i) {
        if (m_dist0To1(m_rng) < 0.5f)
            child.ir[i] = parent1.ir[i];
        else
            child.ir[i] = parent2.ir[i];
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
    // 各遺伝子に対して突然変異を適用
    for (size_t i = 0 ; i < ind.ir.size() ; ++i) {
        // 突然変異確率に基づいてランダムノイズを追加
        if (m_dist0To1(m_rng) < m_mutationRate) {
            float randomNoise = m_dist0To1(m_rng) * 0.1f;
            ind.ir[i] += randomNoise;
        }
    }
}