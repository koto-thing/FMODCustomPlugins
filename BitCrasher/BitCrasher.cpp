/**
 *  @file BitCrasher.cpp
 *  @author Goto Kenta
 *  @brief Implementation of the BitCrasher application.
 */

# include <algorithm>
# include <new>

# include "FaustBitCrasher.h"
# include "../ThirdParty/inc/fmod_common.h"
# include "../ThirdParty/inc/fmod_dsp.h"

/* コールバック関数 */
FMOD_RESULT F_CALL GeneticReverb_Create(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL BitCrasher_Release(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL BitCrasher_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op);
FMOD_RESULT F_CALL BitCrasher_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value);
FMOD_RESULT F_CALL BitCrasher_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr);

/**
 * @brief BitCrasher DSPプラグインの内部データ
 */
struct BitCrasherState {
    mydsp* faustDspL;
    mydsp* faustDspR;
};

/**
 * @brief BitCrasher DSPプラグインのパラメータインデックス
 */
enum {
    BITCRASHER_PARAM_BITS = 0,
    BITCRASHER_PARAM_DOWNSAMPLING,
    NUM_PARAMETERS,
};

// パラメータ説明の定義
static FMOD_DSP_PARAMETER_DESC s_Bits;
static FMOD_DSP_PARAMETER_DESC s_Downsampling;
static FMOD_DSP_PARAMETER_DESC* s_Params[NUM_PARAMETERS];

/**
 * @brief BitCrasher DSPプラグインのパラメータ説明の初期化
 */
static void InitParameterDescs() {
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Bits, "Bits", "", "BitDepth", 1.0f, 16.0f, 8.0f); // ビット深度の範囲を0から16に設定
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Downsampling, "Downsampling", "x", "Downsampling Factor", 1.0f, 32.0f, 4.0f); // ダウンサンプリングの範囲を1から16に設定
    s_Params[BITCRASHER_PARAM_BITS] = &s_Bits;
    s_Params[BITCRASHER_PARAM_DOWNSAMPLING] = &s_Downsampling;
}

/**
 * @brief BitCrasher DSPプラグインの説明構造体
 */
FMOD_DSP_DESCRIPTION templateDesc = {
    FMOD_PLUGIN_SDK_VERSION,      // プラグインSDKのバージョン
    "BitCrasher",                 // プラグインの名前
    0x00010000,                   // プラグインのバージョン
    1,                            // 入力バッファの数
    1,                            // 出力バッファの数
    GeneticReverb_Create,            // DSP生成時のコールバック
    BitCrasher_Release,           // DSP解放時のコールバック
    nullptr,                      // DSPリセット時のコールバック
    nullptr,                      // DSP読み取り時のコールバック
    BitCrasher_Process,           // DSPプロセス時のコールバック
    nullptr,                      // 位置設定コールバック
    NUM_PARAMETERS,               // パラメータの数
    s_Params,                     // パラメータの説明
    BitCrasher_SetParameterFloat, // setFloatによってパラメータが設定されたときのコールバック
    nullptr,                      // setIntによってパラメータが設定されたときのコールバック
    nullptr,                      // setBoolによってパラメータが設定されたときのコールバック
    nullptr,                      // setDataによってパラメータが設定されたときのコールバック
    BitCrasher_GetParameterFloat, // getFloatによってパラメータが取得されたときのコールバック
    nullptr,                      // getIntによってパラメータが取得されたときの
    nullptr,                      // getBoolによってパラメータが取得されたときのコールバック
    nullptr,                      // getDataによってパラメータが取得されたときのコールバック
    nullptr,                      // shouldiprocessによってプロセスするかどうかを決定するコールバック
    nullptr,                      // ユーザーデータ
    nullptr,                      // DSPがロードor登録されたときに呼ばれるコールバック
    nullptr,                      // DSPがアンロードor登録解除されたときに呼ばれるコールバック
    nullptr,                      // ミキサー実行開始時に呼ばれるコールバック
};

/**
 * @brief BitCrasher DSPプラグインの作成関数
 * @param dsp_state
 * @return
 */
FMOD_RESULT F_CALL GeneticReverb_Create(FMOD_DSP_STATE *dsp_state) {
    InitParameterDescs();

    // dsp_stateのfunctionsが有効か確認
    if (!dsp_state->functions) {
        return FMOD_ERR_INTERNAL;
    }

    // BitCrasherStateのインスタンスを作成し、初期化する
    FMOD_MEMORY_ALLOC_CALLBACK alloc_callback = dsp_state->functions->alloc;
    if (!alloc_callback) {
        return FMOD_ERR_INTERNAL;
    }

    // 開放用コールバック
    FMOD_MEMORY_FREE_CALLBACK free_callback = dsp_state->functions->free;
    if (!free_callback) {
        return FMOD_ERR_INTERNAL;
    }

    // メモリを確保
    auto *state = static_cast<BitCrasherState*>(alloc_callback(sizeof(BitCrasherState), FMOD_MEMORY_NORMAL, __FILE__));
    if (state == nullptr) {
        return FMOD_ERR_MEMORY;
    }

    // dsp_stateにポインタを渡す
    dsp_state->plugindata = state;
    state->faustDspL = nullptr;
    state->faustDspR = nullptr;

    // サンプルレートを取得
    int sampleRate = 48000;
    if (dsp_state->functions->getsamplerate) {
        dsp_state->functions->getsamplerate(dsp_state, &sampleRate);
    }

    // Lチャンネル用の DSP のメモリを確保して初期化
    void* faust_mem_L = alloc_callback(sizeof(mydsp), FMOD_MEMORY_NORMAL, __FILE__);
    if (faust_mem_L == nullptr) {
        free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);
        return FMOD_ERR_MEMORY;
    }

    state->faustDspL = new(faust_mem_L) mydsp();
    state->faustDspL->init(sampleRate);

    // Rチャンネル用の DSP のメモリを確保して初期化
    void* faust_mem_R = alloc_callback(sizeof(mydsp), FMOD_MEMORY_NORMAL, __FILE__);
    if (faust_mem_R == nullptr) {
        state->faustDspL->~mydsp();
        free_callback(faust_mem_L, FMOD_MEMORY_NORMAL, __FILE__);
        free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);

        return FMOD_ERR_MEMORY;
    }

    state->faustDspR = new(faust_mem_R) mydsp();
    state->faustDspR->init(sampleRate);

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief BitCrasher DSPプラグインの解放関数
 * @param dsp_state
 * @return
 */
FMOD_RESULT F_CALL BitCrasher_Release(FMOD_DSP_STATE *dsp_state) {
    if (!dsp_state->functions) {
        return FMOD_ERR_INTERNAL;
    }

    // BitCrasherStateのインスタンスを取得
    FMOD_MEMORY_FREE_CALLBACK free_callback = dsp_state->functions->free;
    if (!free_callback) {
        return FMOD_ERR_MEMORY;
    }

    // BitCrasherStateのインスタンスを取得
    auto *state = static_cast<BitCrasherState *>(dsp_state->plugindata);
    if (state) {
        // Lチャンネル用DSPの解放
        if (state->faustDspL) {
            state->faustDspL->~mydsp();
            free_callback(state->faustDspL, FMOD_MEMORY_NORMAL, __FILE__);
        }
        // Rチャンネル用DSPの解放
        if (state->faustDspR) {
            state->faustDspR->~mydsp();
            free_callback(state->faustDspR, FMOD_MEMORY_NORMAL, __FILE__);
        }

        free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);
    }

    // 内部データもクリア
    dsp_state->plugindata = nullptr;

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief BitCrasher DSPプラグインのプロセス関数
 * @param dsp_state DSPの内部データ
 * @param length 処理するサンプル数
 * @param inBuffers 入力バッファ配列
 * @param outBuffers 出力バッファ配列
 * @param inputsIdle 入力がアイドル状態かどうか
 * @param op プロセス操作の種類
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はエラーコードを返す
 */
FMOD_RESULT F_CALL BitCrasher_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op) {
    // FMOD_DSP_PROCESS_QUERYの場合、入出力フォーマットのミラーを行う
    if (op == FMOD_DSP_PROCESS_QUERY) {
        if (inBuffers && outBuffers) {
            const int nb = std::min(inBuffers->numbuffers, outBuffers->numbuffers);
            for (int i = 0; i < nb; ++i) {
                outBuffers->buffernumchannels[i] = inBuffers->buffernumchannels[i];
                outBuffers->bufferchannelmask[i] = inBuffers->bufferchannelmask[i];
            }
            outBuffers->speakermode = inBuffers ? inBuffers->speakermode : outBuffers->speakermode;
        }
        return FMOD_OK;
    }

    // 内部データを取得
    auto *state = static_cast<BitCrasherState *>(dsp_state->plugindata);
    if (!state || !state->faustDspL || !state->faustDspR || !inBuffers || !outBuffers ||
        outBuffers->numbuffers == 0 || outBuffers->buffers == nullptr ||
        inBuffers->numbuffers == 0 || inBuffers->buffers == nullptr) {
        return FMOD_ERR_DSP_DONTPROCESS;
    }

    // 入力がアイドル状態の場合、出力バッファをゼロで埋める
    if (inputsIdle || !inBuffers || inBuffers->numbuffers == 0 || !inBuffers->buffers) {
        for (int i = 0 ; i < outBuffers->numbuffers ; ++i) {
            const int chs = outBuffers->buffernumchannels[i];
            float *out = outBuffers->buffers[i];
            for (unsigned int k = 0 ; k < length * static_cast<unsigned int>(chs); ++k) { // ループ変数をiからkに変更
                out[k] = 0.0f;
            }
        }

        return FMOD_ERR_DSP_SILENCE;
    }

    // FMODがDSP_PROCESS_PERFORMの場合、エフェクト処理を行う
    const int nb = std::min(outBuffers->numbuffers, outBuffers->numbuffers);
    for (int b = 0 ; b < nb ; ++b) {
        const int chs = std::min(inBuffers->buffernumchannels[b], outBuffers->buffernumchannels[b]);
        float* in = inBuffers->buffers[b];
        float* out = outBuffers->buffers[b];

        for (unsigned int i = 0 ; i < length ; ++i) {
            for (int ch = 0 ; ch < chs ; ++ch) {
                const int index = static_cast<int>(i) * chs + ch;
                float inSample = in[index];
                float outSample = 0.0f;

                FAUSTFLOAT* fin[1] = { reinterpret_cast<FAUSTFLOAT*>(&inSample) };
                FAUSTFLOAT* fout[1] = { reinterpret_cast<FAUSTFLOAT*>(&outSample) };
                if (ch == 0 && state->faustDspL) {
                    state->faustDspL->compute(1, fin, fout);
                }
                else if (ch == 1 && state->faustDspR) {
                    state->faustDspR->compute(1, fin, fout);
                }
                else if (state->faustDspL) {
                    state->faustDspL->compute(1, fin, fout);
                }

                out[index] = outSample;
            }
        }
    }

    return FMOD_OK;
}

/**
 * @brief BitCrasher DSPプラグインのパラメータ設定関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 設定する値
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL BitCrasher_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value) {
    auto* state = static_cast<BitCrasherState*>(dsp_state->plugindata);
    if (!state || !state->faustDspL || !state->faustDspR) {
        return FMOD_ERR_INVALID_PARAM;
    }

    switch (index) {
        case BITCRASHER_PARAM_BITS:
            state->faustDspL->fHslider1 = static_cast<FAUSTFLOAT>(value);
            state->faustDspR->fHslider1 = static_cast<FAUSTFLOAT>(value);
            break;

        case BITCRASHER_PARAM_DOWNSAMPLING:
            state->faustDspL->fHslider0 = static_cast<FAUSTFLOAT>(value);
            state->faustDspR->fHslider0 = static_cast<FAUSTFLOAT>(value);
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

/**
 * @brief BitCrasher DSPプラグインのパラメータ取得関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 取得する値を格納するポインタ
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL BitCrasher_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr) {
    auto* state = static_cast<BitCrasherState*>(dsp_state->plugindata);
    if (!state || !state->faustDspL) return FMOD_ERR_INVALID_PARAM;

    switch (index) {
        case BITCRASHER_PARAM_BITS:
            if (value) *value = static_cast<float>(state->faustDspL->fHslider1);
            if (valuestr) snprintf(valuestr, 32, "%.0f bits", state->faustDspL->fHslider1);
            break;

        case BITCRASHER_PARAM_DOWNSAMPLING:
            if (value) *value = static_cast<float>(state->faustDspL->fHslider0);
            if (valuestr) snprintf(valuestr, 32, "%.0f x", state->faustDspL->fHslider0);
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/**
 * @brief ビルドしたDLLからFMODがDSPプラグインの説明を取得するためのエクスポート関数
 * @return BitCrasher DSPプラグインの説明構造体へのポインタ
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
        return &templateDesc;
    }
}