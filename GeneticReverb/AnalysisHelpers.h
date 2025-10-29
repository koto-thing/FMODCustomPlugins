# pragma once

# include <vector>
# include <cmath>
# include <numeric>
# include <algorithm>
# include <iostream>

/**
 * @brief シュレーダーの残響曲線を計算する関数
 * @param ir インパルス応答のベクトル
 * @return シュレーダーの残響曲線（dBスケール）のベクトル
 */
inline std::vector<float> calculateSchroederDecay(const std::vector<float>& ir) {
    if (ir.empty()) {
        return {};
    }

    size_t n = ir.size();
    std::vector<double> edc(n);

    // 各サンプルのエネルギーを計算
    for (size_t i = 0 ; i < n ; ++i) {
        edc[i] = static_cast<double>(ir[i]) * static_cast<double>(ir[i]);
    }

    // 逆順に累積和を計算する
    std::reverse(edc.begin(), edc.end());
    std::partial_sum(edc.begin(), edc.end(), edc.begin());
    std::reverse(edc.begin(), edc.end());

    // 全エネルギーで正規化して、dBスケールに変換する
    double totalEnergy = edc[0];
    const double minEnergy = 1e-20;

    // 全エネルギーが非常に小さい場合、無効なEDCを返す
    if (totalEnergy < minEnergy) {
        return std::vector<float>(n, -100.0f);
    }

    // dBスケールに変換
    std::vector<float> edc_dB(n);
    for (size_t i = 0 ; i < n ; ++i) {
        const double ratio = std::max(edc[i] / std::max(totalEnergy, minEnergy), minEnergy);
        edc_dB[i] = static_cast<float>(10.0 * std::log10(ratio)); // 明示キャストで C4244 回避
    }

    return edc_dB;
}

/**
 * @brief T60を計算する関数
 * @param edc_dB シュレーダーの残響曲線（dBスケール）のベクトル
 * @param sampleRate サンプリングレート
 * @return T60の値（秒）
 */
inline float calculateT60(const std::vector<float>& edc_dB, float sampleRate) {
    /* T60を計算する */
    auto findTimeForDB = [&](float db) -> int {
        for (size_t i = 0 ; i < edc_dB.size() ; ++i) {
            if (edc_dB[i] <= db)
                return (int)i;
        }

        return (int)edc_dB.size() - 1;
    };

    // -5dBと-35dBの時間を取得
    int t_minus5_samples = findTimeForDB(-5.0f);
    int t_minus35_samples = findTimeForDB(-35.0f);

    // T30のサンプル数
    auto t30_samples = static_cast<float>(t_minus35_samples - t_minus5_samples);
    if (t30_samples <= 0.0f) {
        return 0.0f;
    }

    // T30の時間
    float t30_seconds = t30_samples / sampleRate;

    // T60に変換
    return t30_seconds * 2.0f;
}

/**
 * @brief EDCからEDTを計算する関数
 * @param edc_dB シュレーダーの残響曲線（dBスケール）のベクトル
 * @param sampleRate サンプリングレート
 * @return EDTの値（秒）
 */
inline float calculateEDT(const std::vector<float>& edc_dB, float sampleRate) {
    /* EDTを計算する */
    auto findTimeForDB = [&](float db) -> int {
        for (size_t i = 0 ; i < edc_dB.size() ; ++i) {
            if (edc_dB[i] <= db)
                return (int)i;
        }

        return (int)edc_dB.size() - 1;
    };

    // 0dBと-10dBの時間を取得
    int t0_samples = findTimeForDB(0.0f);
    int t_minus10_samples = findTimeForDB(-10.0f);

    // T10のサンプル数
    auto t10_samples = static_cast<float>(t_minus10_samples - t0_samples);
    if (t10_samples <= 0.0f) {
        return 0.0f;
    }

    // EDTの時間
    float t10_seconds = t10_samples / sampleRate;

    // EDTに変換
    return t10_seconds * 6.0f;
}

/**
 * @brief C80を計算する関数
 * @param ir インパルス応答のベクトル
 * @param sampleRate サンプリングレート
 * @return C80の値（dB）
 */
inline float calculateC80(const std::vector<float>& ir, float sampleRate) {
    int samples_80ms = static_cast<int>(0.08f * sampleRate);

    // エネルギーを計算
    double earlyEnergy = 0.0;
    double lateEnergy = 0.0;
    const double minEnergy = 1e-20;

    // 早期エネルギーと後期エネルギーを分けて計算
    for (size_t i = 0 ; i < ir.size() ; ++i) {
        double energy = static_cast<double>(ir[i]) * static_cast<double>(ir[i]);
        if (i < samples_80ms)
            earlyEnergy += energy;
        else
            lateEnergy += energy;
    }

    // C80を計算
    return static_cast<float>(
        10.0 * std::log10(std::max(earlyEnergy, minEnergy) / std::max(lateEnergy, minEnergy))
    );
}