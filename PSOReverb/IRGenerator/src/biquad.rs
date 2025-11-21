use std::f64::consts::PI; // 定数PIをインポート

// 二次フィルタ(Biquad)の構造体定義
pub struct Biquad {
    a0: f64, a1: f64, a2: f64, // フィルタ係数
    b1: f64, b2: f64,          // フィルタ係数
    z1: f64, z2: f64           // 過去の状態変数
}

// Biquad構造体の実装
impl Biquad {
    /// ローパスフィルタの生成関数
    /// - 引数
    ///     - `cutoff`: カットオフ周波数(Hz)
    ///     - `sample_rate`: サンプリング周波数(Hz)
    /// - 戻り値
    ///     - Biquad構造体
    pub fn new_lowpassfilter(cutoff: f64, sample_rate: f64) -> Self {
        let omega = 2.0 * PI * cutoff / sample_rate; // 角周波数の計算
        let sn = omega.sin();                        // サイン値の計算
        let cs = omega.cos();                        // コサイン値の計算
        let alpha = sn / (2.0 * 0.707);              // Q値の計算(0.707はバターワース特性)
        
        // フィルタ係数の正規化を行う
        let norm = 1.0 + alpha;
        Self {
            a0: ((1.0 - cs) / 2.0) / norm,
            a1: (1.0 - cs) / norm,
            a2: ((1.0 - cs) / 2.0) / norm,
            b1: (-2.0 * cs) / norm,
            b2: (1.0 - alpha) / norm,
            z1: 0.0,
            z2: 0.0
        }
    }
    
    /// ハイパスフィルタの生成関数
    /// - 引数
    ///     - `cutoff`: カットオフ周波数(Hz)
    ///     - `sample_rate`: サンプリング周波数(Hz)
    /// - 戻り値
    ///     - Biquad構造体
    pub fn new_highpassfilter(cutoff: f64, sample_rate: f64) -> Self {
        let omega = 2.0 * PI * cutoff / sample_rate; // 角周波数の計算
        let sn = omega.sin();                        // サイン値の計算
        let cs = omega.cos();                        // コサイン値の計算
        let alpha = sn / (2.0 * 0.707);              // Q値の計算(0.707はバターワース特性)
        
        // フィルタ係数の正規化を行う
        let norm = 1.0 + alpha;
        Self {
            a0: ((1.0 + cs) / 2.0) / norm,
            a1: -(1.0 + cs) / norm,
            a2: ((1.0 + cs) / 2.0) / norm,
            b1: (-2.0 * cs) / norm,
            b2: (1.0 - alpha) / norm,
            z1: 0.0,
            z2: 0.0
        }
    }
    
    /// フィルタの処理関数
    /// - 引数
    ///     - `input`: 入力サンプル
    /// - 戻り値
    ///     - 出力サンプル
    pub fn process(&mut self, input: f64) -> f64 {
        // 差分方程式に基づくフィルタ処理
        let output = input * self.a0 + self.z1;
        self.z1 = input * self.a1 + self.z2 - self.b1 * output; // 状態変数の更新
        self.z2 = input * self.a2 - self.b2 * output;           // 状態変数の更新
        
        output
    }
}