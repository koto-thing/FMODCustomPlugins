/**
 *  @file GeneticReverb.cpp
 *  @author Goto Kenta
 *  @brief 遺伝的アルゴリズムを用いたリバーブDSPプラグイン
 */

# include "ConvolutionProcessor.h"

# include <algorithm>
# include <cstdio>
# include <cstring>
# include <fmod.h>
# include <fmod_dsp.h>

/* コールバック関数 */
FMOD_RESULT F_CALL GeneticReverb_Create(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL GeneticReverb_Release(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL GeneticReverb_Reset(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL GeneticReverb_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op);
FMOD_RESULT F_CALL GeneticReverb_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value);
FMOD_RESULT F_CALL GeneticReverb_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr);
FMOD_RESULT F_CALL GeneticReverb_SetParameterBool(FMOD_DSP_STATE* dsp_state, int index, FMOD_BOOL value);
FMOD_RESULT F_CALL GeneticReverb_GetParameterBool(FMOD_DSP_STATE* dsp_state, int index, FMOD_BOOL* value, char* valuestr);

/**
 * @brief インパルス応答ハンドル構造体
 */
struct IRHandle {
    const float* data{};
    size_t length{};

    void release() { }
};

/**
 * @brief GeneticReverb DSPプラグインの内部データ
 */
struct GeneticReverbState {
    ConvolutionProcessor* processor{};
    std::unique_ptr<float[]> scratchInL;
    std::unique_ptr<float[]> scratchInR;
    std::unique_ptr<float[]> scratchOutL;
    std::unique_ptr<float[]> scratchOutR;
    unsigned int scratchFrames = 0;

    std::atomic<IRHandle*> irToSwap { nullptr };
    int channels = 2;
    float dry = 0.5f;
    float wet = 0.5f;
    float volume = 1.0f;

    ReverbTargetParams params { 0.4f, 0.06f, 12.0f, 0.7f };
    std::atomic<float> lastProgress { -1.0f };
};

/**
 * @brief GeneticReverb DSPプラグインのパラメータインデックス
 */
enum {
    GENETIC_REVERB_PARAM_DRY = 0,
    GENETIC_REVERB_PARAM_WET,
    GENETIC_REVERB_PARAM_VOLUME,
    GENETIC_REVERB_PARAM_T60,
    GENETIC_REVERB_PARAM_C80,
    GENETIC_REVERB_PARAM_GENERATE,
    GENETIC_REVERB_PARAM_CANCEL,
    GENETIC_REVERB_PARAM_PROGRESS,
    NUM_PARAMETERS,
};

// パラメータ説明の定義
static FMOD_DSP_PARAMETER_DESC s_Dry;
static FMOD_DSP_PARAMETER_DESC s_Wet;
static FMOD_DSP_PARAMETER_DESC s_Volume;
static FMOD_DSP_PARAMETER_DESC s_T60;
static FMOD_DSP_PARAMETER_DESC s_C80;
static FMOD_DSP_PARAMETER_DESC s_Generate;
static FMOD_DSP_PARAMETER_DESC s_Cancel;
static FMOD_DSP_PARAMETER_DESC s_Progress;
static FMOD_DSP_PARAMETER_DESC* s_Params[NUM_PARAMETERS];

/**
 * @brief GeneticReverb DSPプラグインのパラメータ説明の初期化
 */
static void InitParameterDescs() {
    // 音量関連パラメータ
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Dry, "Dry", "x", "Dry level", 0.0f, 1.0f, 0.5f);
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Wet, "Wet", "x", "Wet level", 0.0f, 1.0f, 0.5f);
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Volume, "Volume", "x", "Output gain", 0.0f, 2.0f, 1.0f);

    // 残響特性パラメータ
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_T60, "T60", "s", "Target T60 [s]", 0.05f, 10.0f, 0.4f);
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_C80, "C80", "dB", "Target C80 [dB]", -40.0f, 40.0f, 12.0f);

    // 生成制御パラメータ
    FMOD_DSP_INIT_PARAMDESC_BOOL(s_Generate, "Generate", "btn", "Start IR Generation", false, nullptr);
    FMOD_DSP_INIT_PARAMDESC_BOOL(s_Cancel, "Cancel", "btn", "Cancel IR Generation", false, nullptr);

    // 進捗表示パラメータ
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Progress, "Progress", "", "Generation Progress", 0.0f, 1.0f, 0.0f);

    s_Params[GENETIC_REVERB_PARAM_DRY] = &s_Dry;
    s_Params[GENETIC_REVERB_PARAM_WET] = &s_Wet;
    s_Params[GENETIC_REVERB_PARAM_VOLUME] = &s_Volume;
    s_Params[GENETIC_REVERB_PARAM_T60] = &s_T60;
    s_Params[GENETIC_REVERB_PARAM_C80] = &s_C80;
    s_Params[GENETIC_REVERB_PARAM_GENERATE] = &s_Generate;
    s_Params[GENETIC_REVERB_PARAM_CANCEL] = &s_Cancel;
    s_Params[GENETIC_REVERB_PARAM_PROGRESS] = &s_Progress;
}

/**
 * @brief GeneticReverb DSPプラグインの説明構造体
 */
FMOD_DSP_DESCRIPTION geneticReverbDesc = {
    FMOD_PLUGIN_SDK_VERSION,         // プラグインSDKのバージョン
    "GeneticReverb",                 // プラグインの名前
    0x00010000,                      // プラグインのバージョン
    1,                               // 入力バッファの数
    1,                               // 出力バッファの数
    GeneticReverb_Create,            // DSP生成時のコールバック
    GeneticReverb_Release,           // DSP解放時のコールバック
    GeneticReverb_Reset,             // DSPリセット時のコールバック
    nullptr,                         // DSP読み取り時のコールバック
    GeneticReverb_Process,           // DSPプロセス時のコールバック
    nullptr,                         // 位置設定コールバック
    NUM_PARAMETERS,                  // パラメータの数
    s_Params,                        // パラメータの説明
    GeneticReverb_SetParameterFloat, // setFloatによってパラメータが設定されたときのコールバック
    nullptr,                         // setIntによってパラメータが設定されたときのコールバック
    GeneticReverb_SetParameterBool,  // setBoolによってパラメータが設定されたときのコールバック
    nullptr,                         // setDataによってパラメータが設定されたときのコールバック
    GeneticReverb_GetParameterFloat, // getFloatによってパラメータが取得されたときのコールバック
    nullptr,                         // getIntによってパラメータが取得されたときの
    GeneticReverb_GetParameterBool,  // getBoolによってパラメータが取得されたときのコールバック
    nullptr,                         // getDataによってパラメータが取得されたときのコールバック
    nullptr,                         // shouldiprocessによってプロセスするかどうかを決定するコールバック
    nullptr,                         // ユーザーデータ
    nullptr,                         // DSPがロードor登録されたときに呼ばれるコールバック
    nullptr,                         // DSPがアンロードor登録解除されたときに呼ばれるコールバック
    nullptr,                         // ミキサー実行開始時に呼ばれるコールバック
};

/**
 * @brief GeneticReverb DSPプラグインの作成関数
 * @param dsp_state DSPの内部データ
 * @return FMODの処理結果コード
 */
FMOD_RESULT F_CALL GeneticReverb_Create(FMOD_DSP_STATE *dsp_state) {
    InitParameterDescs();

    // dsp_stateのfunctionsが有効か確認
    if (!dsp_state->functions)
        return FMOD_ERR_INTERNAL;

    // Templateのインスタンスを作成し、初期化する
    FMOD_MEMORY_ALLOC_CALLBACK alloc_callback = dsp_state->functions->alloc;
    if (!alloc_callback)
        return FMOD_ERR_INTERNAL;

    // メモリを確保
    auto *state = (GeneticReverbState*)alloc_callback(sizeof(GeneticReverbState), FMOD_MEMORY_NORMAL, __FILE__);
    if (state == nullptr)
        return FMOD_ERR_MEMORY;

    // dsp_stateにポインタを渡す
    dsp_state->plugindata = state;

    // パラメータの初期値を設定
    state->processor = new(std::nothrow) ConvolutionProcessor();
    if (state->processor == nullptr) {
        FMOD_MEMORY_FREE_CALLBACK free_callback = dsp_state->functions->free;
        if (free_callback)
            free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);

        return FMOD_ERR_MEMORY;
    }

    state->params = ReverbTargetParams{ 0.4f, 0.06f, 12.0f, 0.7f };
    state->processor->setTargetParams(state->params);

    state->lastProgress.store(0.0f);

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief GeneticReverb DSPプラグインの解放関数
 * @param dsp_state DSPの内部データ
 * @return FMODの処理結果コード
 */
FMOD_RESULT F_CALL GeneticReverb_Release(FMOD_DSP_STATE *dsp_state) {
    if (!dsp_state->functions)
        return FMOD_ERR_INTERNAL;

    // Templateのインスタンスを取得
    FMOD_MEMORY_FREE_CALLBACK free_callback = dsp_state->functions->free;
    if (!free_callback)
        return FMOD_ERR_INTERNAL;

    // Templateのインスタンスを取得
    auto *state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (state) {
        if (state->processor) {
            state->processor->release();
            delete state->processor;
            state->processor = nullptr;
        }

        free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);
    }

    // 内部データもクリア
    dsp_state->plugindata = nullptr;

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief GeneticReverb DSPプラグインのプロセス関数
 * @param dsp_state DSPの内部データ
 * @param length 処理するサンプル数
 * @param inBuffers 入力バッファ配列
 * @param outBuffers 出力バッファ配列
 * @param inputsIdle 入力がアイドル状態かどうか
 * @param op プロセス操作の種類
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はエラーコードを返す
 */
FMOD_RESULT F_CALL GeneticReverb_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op) {
    auto *state = static_cast<GeneticReverbState *>(dsp_state->plugindata);
    if (!state || !inBuffers || !outBuffers) {
        return FMOD_ERR_DSP_DONTPROCESS;
    }

    // 安全のため、functions ポインタの有無を確認
    if (!dsp_state->functions)
        return FMOD_ERR_DSP_DONTPROCESS;

    // 進捗更新（processorがない場合はスキップ）
    if (state->processor) {
        const float progress = state->processor->progress();
        const float oldProgress = state->lastProgress.load();
        if (progress != oldProgress) {
            state->lastProgress.store(progress);
        }
    }

    // IRの差し替えが要求されていたら実行
    if (IRHandle* ir = state->irToSwap.exchange(nullptr)) {
        state->processor->setIR(ir->data, ir->length);
        ir->release();
    }

    // 出力バッファが無効な場合は何もしない
    if (outBuffers->numbuffers == 0 || outBuffers->buffers == nullptr) {
        return FMOD_OK;
    }

    // 入力バッファが無効、またはアイドル状態の場合は出力をゼロで埋める
    if (inBuffers->numbuffers == 0 || inBuffers->buffers == nullptr || inputsIdle) {
        for (int i = 0 ; i < outBuffers->numbuffers ; ++i) {
            const int chs = outBuffers->buffernumchannels[i];
            float *out = outBuffers->buffers[i];
            std::memset(out, 0, sizeof(float) * length * static_cast<unsigned int>(chs));
        }
        return FMOD_OK;
    }

    // パラメータを取得
    const float dry = state->dry;
    const float wet = state->wet;
    const float volume = state->volume;

    // FMOD_DSP_PROCESS_PERFORMの場合、エフェクト処理を行う
    if (op == FMOD_DSP_PROCESS_PERFORM) {
        const int nb = std::min(inBuffers->numbuffers, outBuffers->numbuffers);

        // 一時バッファが不足していたら拡張
        if (state->scratchFrames < length || !state->scratchInL || !state->scratchInR || !state->scratchOutL || !state->scratchOutR) {
            state->scratchFrames = length;
            state->scratchInL.reset(new float[state->scratchFrames]);
            state->scratchInR.reset(new float[state->scratchFrames]);
            state->scratchOutL.reset(new float[state->scratchFrames]);
            state->scratchOutR.reset(new float[state->scratchFrames]);
        }

        for (int b = 0 ; b < nb ; ++b) {
            const int chs = std::min(inBuffers->buffernumchannels[b], outBuffers->buffernumchannels[b]);
            const float* in = inBuffers->buffers[b];
            float* out = outBuffers->buffers[b];
            if (!in || !out || chs <= 0) continue;

            const unsigned int frames = length;

            // デインタリーブ(Wet生成はL/Rのみ使用。Mono入力は複製)
            for (unsigned int i = 0 ; i < frames ; ++i) {
                const unsigned int base = i * chs;
                const float inL = in[base + 0];
                const float inR = (chs > 1) ? in[base + 1] : inL;
                state->scratchInL[i] = inL;
                state->scratchInR[i] = inR;
            }

            // 畳み込み(IR未準備時は0)
            state->processor->process(state->scratchInL.get(), state->scratchInR.get(),
                                      state->scratchOutL.get(), state->scratchOutR.get(), frames);

            // インタリーブ + 全チャンネルにDry/Wet/Volume適用
            for (unsigned int i = 0 ; i < frames ; ++i) {
                const unsigned int base = i * chs;

                // Wet信号を取得
                const float wetL = state->scratchOutL[i];
                const float wetR = state->scratchOutR[i];
                const float wetMono = 0.5f * (wetL + wetR);

                // 各チャンネルに対してDry/WetミックスとVolume適用
                for (int ch = 0; ch < chs; ++ch) {
                    const float inSample = in[base + ch];
                    float wetSig = wetMono;

                    // チャンネルごとのWet信号を選択
                    if (ch == 0) wetSig = wetL;
                    else if (ch == 1) wetSig = wetR;

                    // Dry/Wetミックス
                    const float mixed = (dry * inSample) + (wet * wetSig);
                    out[base + ch] = mixed * volume;
                }
            }
        }
    }
    // FMOD_DSP_PROCESS_QUERYの場合、入出力フォーマットのミラーを行う
    else {
        const int nb = std::min(inBuffers->numbuffers, outBuffers->numbuffers);
        for (int i = 0 ; i < nb ; ++i) {
            const int chs = std::min(inBuffers->buffernumchannels[i], outBuffers->buffernumchannels[i]);
            const float *in = inBuffers->buffers[i];
            float *out = outBuffers->buffers[i];
            if (!in || !out || chs <= 0) continue;
            std::memcpy(out, in, sizeof(float) * length * static_cast<unsigned int>(chs));
        }
    }

    return FMOD_OK;
}

/**
 * @brief GeneticReverb DSPプラグインのリセット関数
 * @param dsp_state DSPの内部データ
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL GeneticReverb_Reset(FMOD_DSP_STATE* dsp_state) {
    auto* state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (!state) {
        return FMOD_ERR_INVALID_PARAM;
    }

    // サンプリングレートとバッファサイズを取得
    unsigned int bufferSize = 0;
    int samplingRate = 0;
    dsp_state->functions->getblocksize(dsp_state, &bufferSize);
    dsp_state->functions->getsamplerate(dsp_state, &samplingRate);

    if (bufferSize <= 0) bufferSize = 1024;
    if (samplingRate <= 0) samplingRate = 48000;

    // プロセッサを準備
    state->processor->prepare(samplingRate, static_cast<unsigned int>(bufferSize));
    state->scratchFrames = static_cast<unsigned int>(bufferSize);
    state->channels = 2;

    // スクラッチバッファを確保
    state->scratchInL.reset(new float[state->scratchFrames]);
    state->scratchInR.reset(new float[state->scratchFrames]);
    state->scratchOutL.reset(new float[state->scratchFrames]);
    state->scratchOutR.reset(new float[state->scratchFrames]);

    state->processor->setTargetParams(state->params);

    state->lastProgress.store(0.0f);

    return FMOD_OK;
}

/**
 * @brief GeneticReverb DSPプラグインのパラメータ設定関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 設定する値
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL GeneticReverb_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value) {
    auto* state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (!state) {
        return FMOD_ERR_INVALID_PARAM;
    }

    switch (index) {
        case GENETIC_REVERB_PARAM_DRY:
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            state->dry = value;
            break;

        case GENETIC_REVERB_PARAM_WET:
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            state->wet = value;
            break;

        case GENETIC_REVERB_PARAM_VOLUME:
            if (value < 0.0f) value = 0.0f;
            if (value > 2.0f) value = 2.0f;
            state->volume = value;
            break;

        case GENETIC_REVERB_PARAM_T60: {
                if (value < 0.05f) value = 0.05f;
                if (value > 10.0f) value = 10.0f;
                state->params.t60 = value;
                if (state->processor) state->processor->setTargetParams(state->params);
                break;
        }

        case GENETIC_REVERB_PARAM_C80: {
                if (value < -40.0f) value = -40.0f;
                if (value > 40.0f)  value = 40.0f;
                state->params.c80 = value;
                if (state->processor) state->processor->setTargetParams(state->params);
                break;
        }

        case GENETIC_REVERB_PARAM_PROGRESS:
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

/**
 * @brief GeneticReverb DSPプラグインのパラメータ取得関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 取得する値を格納するポインタ
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL GeneticReverb_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr) {
    auto* state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (!state) return FMOD_ERR_INVALID_PARAM;

    switch (index) {
        case GENETIC_REVERB_PARAM_DRY:
            if (value) *value = state->dry;
            if (valuestr) snprintf(valuestr, 32, "%.2f x", state->dry);
            break;

        case GENETIC_REVERB_PARAM_WET:
            if (value) *value = state->wet;
            if (valuestr) snprintf(valuestr, 32, "%.2f x", state->wet);
            break;

        case GENETIC_REVERB_PARAM_VOLUME:
            if (value) *value = state->volume;
            if (valuestr) snprintf(valuestr, 32, "%.2f x", state->volume);
            break;

        case GENETIC_REVERB_PARAM_T60:
            if (value) *value = state->params.t60;
            if (valuestr) snprintf(valuestr, 32, "%.3f s", state->params.t60);
            break;

        case GENETIC_REVERB_PARAM_C80:
            if (value) *value = state->params.c80;
            if (valuestr) snprintf(valuestr, 32, "%.2f dB", state->params.c80);
            break;

        case GENETIC_REVERB_PARAM_PROGRESS: {
                const float progress = state->lastProgress.load();
                if (value) *value = progress;
                if (valuestr) snprintf(valuestr, 32, "%.0f %%", progress * 100.0f);
                break;
        }

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT GeneticReverb_SetParameterBool(FMOD_DSP_STATE* dsp_state, int index, FMOD_BOOL value) {
    auto* state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (!state || !state->processor)
        return FMOD_ERR_INVALID_PARAM;

    switch (index) {
        case GENETIC_REVERB_PARAM_GENERATE:
            if (value) state->processor->startGenerate();
            break;

        case GENETIC_REVERB_PARAM_CANCEL:
            if (value) state->processor->cancelIR();
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT GeneticReverb_GetParameterBool(FMOD_DSP_STATE* dsp_state, int index, FMOD_BOOL* value, char* valuestr) {
    auto* state = static_cast<GeneticReverbState*>(dsp_state->plugindata);
    if (!state || !value)
        return FMOD_ERR_INVALID_PARAM;

    switch (index) {
        case GENETIC_REVERB_PARAM_GENERATE:
            *value = (state->processor && state->processor->isGenerating()) ? 1 : 0;
            break;

        case GENETIC_REVERB_PARAM_CANCEL:
            // ボタン扱いのため常に false
            *value = 0;
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/**
 * @brief ビルドしたDLLからFMODがDSPプラグインの説明を取得するためのエクスポート関数
 * @return GeneticReverb DSPプラグインの説明構造体へのポインタ
 */
#if defined(_WIN32)
    #define FMOD_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
    #define FMOD_EXPORT __attribute__((visibility("default")))
#else
    #define FMOD_EXPORT
#endif

extern "C" {
    FMOD_EXPORT FMOD_DSP_DESCRIPTION* FMODGetDSPDescription() {
        InitParameterDescs();
        return &geneticReverbDesc;
    }
}