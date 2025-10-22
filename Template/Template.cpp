/**
 *  @file Template.cpp
 *  @author Goto Kenta
 *  @brief FMOD DSPプラグインのテンプレート実装
 */

# include <algorithm>
# include <cstdio>
# include <cstring>

# include "../ThirdParty/inc/fmod_common.h"
# include "../ThirdParty/inc/fmod_dsp.h"

/* コールバック関数 */
FMOD_RESULT F_CALL BitCrasher_Create(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL BitCrasher_Release(FMOD_DSP_STATE* dsp_state);
FMOD_RESULT F_CALL BitCrasher_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op);
FMOD_RESULT F_CALL BitCrasher_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value);
FMOD_RESULT F_CALL Template_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr);

/**
 * @brief Template DSPプラグインの内部データ
 */
struct TemplateState {
    float volume;
};

/**
 * @brief Template DSPプラグインのパラメータインデックス
 */
enum {
    TEMPLATE_PARAM_VOLUME = 0,
    NUM_PARAMETERS,
};

// パラメータ説明の定義
static FMOD_DSP_PARAMETER_DESC s_Volume;
static FMOD_DSP_PARAMETER_DESC* s_Params[NUM_PARAMETERS];

/**
 * @brief Template DSPプラグインのパラメータ説明の初期化
 */
static void InitParameterDescs() {
    FMOD_DSP_INIT_PARAMDESC_FLOAT(s_Volume, "Volume", "x", "Linear gain of the Template effect", 0.0f, 2.0f, 1.0f);
    s_Params[TEMPLATE_PARAM_VOLUME] = &s_Volume;
}

/**
 * @brief Template DSPプラグインの説明構造体
 */
FMOD_DSP_DESCRIPTION templateDesc = {
    FMOD_PLUGIN_SDK_VERSION,      // プラグインSDKのバージョン
    "Template",                   // プラグインの名前
    0x00010000,                   // プラグインのバージョン
    1,                            // 入力バッファの数
    1,                            // 出力バッファの数
    BitCrasher_Create,            // DSP生成時のコールバック
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
    Template_GetParameterFloat, // getFloatによってパラメータが取得されたときのコールバック
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
 * @brief Template DSPプラグインの作成関数
 * @param dsp_state
 * @return
 */
FMOD_RESULT F_CALL BitCrasher_Create(FMOD_DSP_STATE *dsp_state) {
    InitParameterDescs();

    // dsp_stateのfunctionsが有効か確認
    if (!dsp_state->functions) {
        return FMOD_ERR_INTERNAL;
    }

    // Templateのインスタンスを作成し、初期化する
    FMOD_MEMORY_ALLOC_CALLBACK alloc_callback = dsp_state->functions->alloc;
    if (!alloc_callback) {
        return FMOD_ERR_INTERNAL;
    }

    // メモリを確保
    auto *state = (TemplateState*)alloc_callback(sizeof(TemplateState), FMOD_MEMORY_NORMAL, __FILE__);
    if (state == nullptr) {
        return FMOD_ERR_MEMORY;
    }

    // dsp_stateにポインタを渡す
    dsp_state->plugindata = state;

    // デフォルトのビット深度を設定
    state->volume = s_Volume.floatdesc.defaultval;

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief Template DSPプラグインの解放関数
 * @param dsp_state
 * @return
 */
FMOD_RESULT F_CALL BitCrasher_Release(FMOD_DSP_STATE *dsp_state) {
    if (!dsp_state->functions) {
        return FMOD_ERR_INTERNAL;
    }

    // Templateのインスタンスを取得
    FMOD_MEMORY_FREE_CALLBACK free_callback = dsp_state->functions->free;
    if (!free_callback) {
        return FMOD_ERR_INTERNAL;
    }

    // Templateのインスタンスを取得
    auto *state = static_cast<TemplateState *>(dsp_state->plugindata);
    if (state) {
        free_callback(state, FMOD_MEMORY_NORMAL, __FILE__);
    }

    // 内部データもクリア
    dsp_state->plugindata = nullptr;

    // 正常終了を返す
    return FMOD_OK;
}

/**
 * @brief Template DSPプラグインのプロセス関数
 * @param dsp_state DSPの内部データ
 * @param length 処理するサンプル数
 * @param inBuffers 入力バッファ配列
 * @param outBuffers 出力バッファ配列
 * @param inputsIdle 入力がアイドル状態かどうか
 * @param op プロセス操作の種類
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はエラーコードを返す
 */
FMOD_RESULT F_CALL BitCrasher_Process(FMOD_DSP_STATE* dsp_state, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op) {
    // 内部データを取得
    auto *state = static_cast<TemplateState *>(dsp_state->plugindata);
    if (!state || !inBuffers || !outBuffers) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if (outBuffers->numbuffers == 0 || outBuffers->buffers == nullptr) {
        return FMOD_OK;
    }

    if (inBuffers->numbuffers == 0 || inBuffers->buffers == nullptr) {
        for (int i = 0 ; i < outBuffers->numbuffers ; ++i) {
            const int chs = outBuffers->buffernumchannels[i];
            float *out = outBuffers->buffers[i];
            for (unsigned int k = 0 ; k < length * static_cast<unsigned int>(chs); ++k) {
                out[k] = 0.0f;
            }
        }
        return FMOD_OK;
    }

    // 入力がアイドル状態の場合、出力バッファをゼロで埋める
    if (inputsIdle) {
        for (int i = 0 ; i < outBuffers->numbuffers ; ++i) {
            const int chs = outBuffers->buffernumchannels[i];
            float *out = outBuffers->buffers[i];
            for (unsigned int k = 0 ; k < length * static_cast<unsigned int>(chs); ++k) { // ループ変数をiからkに変更
                out[k] = 0.0f;
            }
        }
        return FMOD_OK;
    }

    const float gain = state->volume;

    // FMOD_DSP_PROCESS_PERFORMの場合、エフェクト処理を行う
    if (op == FMOD_DSP_PROCESS_PERFORM) {
        const int nb = std::min(inBuffers->numbuffers, outBuffers->numbuffers);
        for (int i = 0 ; i < nb ; ++i) {
            const int chs = std::min(inBuffers->buffernumchannels[i], outBuffers->buffernumchannels[i]);
            const float *in = inBuffers->buffers[i]; // ★ここでセグフォ
            float *out = outBuffers->buffers[i];
            for (unsigned int j = 0 ; j < length * static_cast<unsigned int>(chs); ++j) {
                out[j] = in[j] * gain;
            }
        }
    }
    // FMOD_DSP_PROCESS_QUERYの場合、単純にバッファをコピーする
    else {
        const int nb = std::min(inBuffers->numbuffers, outBuffers->numbuffers);
        for (int i = 0 ; i < nb ; ++i) {
            const int chs = std::min(inBuffers->buffernumchannels[i], outBuffers->buffernumchannels[i]);
            const float *in = inBuffers->buffers[i];
            float *out = outBuffers->buffers[i];
            std::memcpy(out, in, sizeof(float) * length * static_cast<unsigned int>(chs));
        }
    }

    return FMOD_OK;
}

/**
 * @brief Template DSPプラグインのパラメータ設定関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 設定する値
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL BitCrasher_SetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float value) {
    auto* state = static_cast<TemplateState*>(dsp_state->plugindata);
    if (!state) {
        return FMOD_ERR_INVALID_PARAM;
    }

    switch (index) {
        case TEMPLATE_PARAM_VOLUME:
            if (value < 0.0f) value = 0.0f;
            if (value > 2.0f) value = 2.0f;
            state->volume = value;
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

/**
 * @brief Template DSPプラグインのパラメータ取得関数
 * @param dsp_state DSPの内部データ
 * @param index パラメータのインデックス
 * @param value 取得する値を格納するポインタ
 * @return 処理が成功した場合はFMOD_OKを返す、それ以外はFMOD_ERR_INVALID_PARAMを返す
 */
FMOD_RESULT F_CALL Template_GetParameterFloat(FMOD_DSP_STATE* dsp_state, int index, float* value, char* valuestr) {
    auto* state = static_cast<TemplateState*>(dsp_state->plugindata);
    if (!state) return FMOD_ERR_INVALID_PARAM;

    switch (index) {
        case TEMPLATE_PARAM_VOLUME:
            if (value) *value = state->volume;
            if (valuestr) {
                snprintf(valuestr, 32, "%.2f x", state->volume);
            }
            break;

        default:
            return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}


/**
 * @brief ビルドしたDLLからFMODがDSPプラグインの説明を取得するためのエクスポート関数
 * @return Template DSPプラグインの説明構造体へのポインタ
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