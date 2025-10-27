/**
 * @file GeneticAlgorithm.h
 * @author Goto Kenta
 * @brief 遺伝的アルゴリズムのヘッダーファイル
 */

# pragma once

# include <random>
# include <vector>

// GAのターゲットパラメータ構造体
struct ReverbTargetParams {
    float t60 = 0.4f;
    float edt = 0.06f;
    float c80 = 12.0f;
    float br = 0.7f;
};

// 個体(インパルス応答と適応度を保持する構造体)
struct Individual {
    std::vector<float> ir; // インパルス応答
    double fitness = 1e10; // 適応度

    // 適応度の比較演算子
    bool operator<(const Individual& other) const {
        return fitness < other.fitness;
    }
};

class GeneticAlgorithm {
public:
    GeneticAlgorithm(int populationSize, float mutationRate, float sampleRate);
    ~GeneticAlgorithm();

    std::vector<float> compute(const ReverbTargetParams& targetParams, int numGenerations);

private:
    std::vector<Individual> m_population; // 個体群
    int m_popSize;                        // 個体群のサイズ
    float m_mutationRate;                 // 突然変異率
    float m_sampleRate;                   // サンプリングレート

    // 乱数生成器
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_distNeg1to1{-1.0f, 1.0f};
    std::uniform_real_distribution<float> m_dist0To1{0.0f, 1.0f};

    // GAのロジックを実行する関数
    void initializePopulation(float targetT60);
    void calculatePopulationFitness(const ReverbTargetParams& targetParams);
    std::vector<Individual> createNextGeneration();
    Individual crossover(const Individual& parent1, const Individual& parent2);
    void mutate(Individual& ind);
};